#include "doomdef.h"
#include "doomstat.h"
#include "d_player.h"
#include "p_mobj.h"
#include "r_state.h"
#include "i_system.h"
#include "d_net.h"
#include "p_tick.h"
#include "i_network.h"
// -------------------------
// Simple switches/toggles
// -------------------------
#define SNAP_INTERVAL_TICS  35    // ~1 sec (@35 tics/s) – tune to taste
#define MAX_SNAPSHOT_MOBJS  4096  // POC ceiling; raise if needed

// Exposed flags so other units can query
int g_is_host = 0;
int g_next_mobj_id = 0;

// -------------------------
// Wire format (POC, raw structs)
// -------------------------
typedef struct {
  int id;
  int x, y, z;           // fixed >> FRACBITS
  int momx, momy, momz;  // fixed >> FRACBITS
  unsigned int angle;    // angle_t
  int type;              // mobjtype_t
  int health;
  int state;             // index into states[]
  int flags;
  int tics;
} mobj_snapshot_t;

typedef struct {
  int floorheight;       // fixed >> FRACBITS
  int ceilingheight;     // fixed >> FRACBITS
  short lightlevel;
  short special;
} sector_snapshot_t;

typedef struct {
  unsigned int flags;    // line_t flags
  short special;         // keep for classic specials
} line_snapshot_t;

typedef struct {
  int leveltime;
  player_t players[MAXPLAYERS];
  int num_mobjs;
  // Followed by: mobj_snapshot_t[num_mobjs]
  int num_sectors;
  // Followed by: sector_snapshot_t[num_sectors]
  int num_lines;
  // Followed by: line_snapshot_t[num_lines]
} net_snapshot_header_t;

// -------------------------
// Client-side helpers for smoothing
// -------------------------
typedef struct {
  int target_x, target_y, target_z; // fixed >> FRACBITS
  int lerp_ticks;                   // countdown
} net_smooth_t;

static net_smooth_t *smooth_cache = NULL; // indexed by mobj_id modulo N
#define SMOOTH_CACHE_SIZE 8192

static net_smooth_t* smooth_slot(int id) {
  if (!smooth_cache) {
    smooth_cache = Z_Malloc(sizeof(net_smooth_t)*SMOOTH_CACHE_SIZE, PU_STATIC, NULL);
    memset(smooth_cache, 0, sizeof(net_smooth_t)*SMOOTH_CACHE_SIZE);
  }
  return &smooth_cache[id % SMOOTH_CACHE_SIZE];
}

// -------------------------
// Build snapshot (host)
// -------------------------
static size_t build_snapshot(byte **out_buf) {
  // Count mobjs
  int count = 0;
  for (thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next) {
    if (th->function == P_MobjThinker) {
      mobj_t *mo = (mobj_t*)th;
      if (mo->mobj_id == 0) { // host should have assigned
        if (++g_next_mobj_id == 0) ++g_next_mobj_id;
        mo->mobj_id = g_next_mobj_id;
      }
      ++count;
      if (count >= MAX_SNAPSHOT_MOBJS) break;
    }
  }

  int ns = numsectors;
  int nl = numlines;

  size_t sz = sizeof(packet_header_t)+ sizeof(net_snapshot_header_t)
            + count * sizeof(mobj_snapshot_t)
            + ns * sizeof(sector_snapshot_t)
            + nl * sizeof(line_snapshot_t);

  byte *buf = Z_Malloc(sz, PU_STATIC, NULL);
  byte *p = buf;

  packet_set(p, PKT_SNAPSHOT, gametic);
  p += sizeof(packet_header_t);
  net_snapshot_header_t *hdr = (net_snapshot_header_t*)p;
  hdr->leveltime = leveltime;
  for (int i=0;i<MAXPLAYERS;i++)
    if (playeringame[i]) hdr->players[i] = players[i];
  hdr->num_mobjs = count;
  hdr->num_sectors = ns;
  hdr->num_lines = nl;
  p += sizeof(*hdr);

  // mobjs
  int written = 0;
  for (thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next) {
    if (th->function == P_MobjThinker) continue;
    mobj_t *mo = (mobj_t*)th;
    mobj_snapshot_t ms;
    ms.id    = mo->mobj_id;
    ms.x     = mo->x >> FRACBITS;
    ms.y     = mo->y >> FRACBITS;
    ms.z     = mo->z >> FRACBITS;
    ms.momx  = mo->momx >> FRACBITS;
    ms.momy  = mo->momy >> FRACBITS;
    ms.momz  = mo->momz >> FRACBITS;
    ms.angle = mo->angle;
    ms.type  = mo->type;
    ms.health= mo->health;
    ms.state = (int)(mo->state - states);
    ms.flags = mo->flags;
    ms.tics  = mo->tics;

    memcpy(p, &ms, sizeof(ms));
    p += sizeof(ms);
    if (++written >= count) break;
  }

  // sectors
  for (int i=0;i<ns;i++) {
    sector_t *s = &sectors[i];
    sector_snapshot_t ss;
    ss.floorheight   = s->floorheight >> FRACBITS;
    ss.ceilingheight = s->ceilingheight >> FRACBITS;
    ss.lightlevel    = s->lightlevel;
    ss.special       = s->special;
    memcpy(p, &ss, sizeof(ss)); p += sizeof(ss);
  }

  // lines
  for (int i=0;i<nl;i++) {
    line_t *ln = &lines[i];
    line_snapshot_t ls;
    ls.flags   = ln->flags;
    ls.special = ln->special;
    memcpy(p, &ls, sizeof(ls)); p += sizeof(ls);
  }

  *out_buf = buf;
  return sz;
}

// -------------------------
// Apply snapshot (client) – delta style
// -------------------------
static void client_apply_snapshot(const byte *buf, size_t len) {
  if (len < sizeof(net_snapshot_header_t)) return;

  const byte *p = buf;
  const net_snapshot_header_t *hdr = (const net_snapshot_header_t*)p;
  p += sizeof(*hdr);

  // Level time + players
  leveltime = hdr->leveltime;
  for (int i=0;i<MAXPLAYERS;i++)
    if (playeringame[i]) players[i] = hdr->players[i];

  // Build temporary map: id -> present in snapshot
  // Since we don’t have a hash table handy, do two passes:
  // 1) Mark all existing mobjs as unseen.
  for (thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next) {
    if (th->function == P_MobjThinker) {
      mobj_t *mo = (mobj_t*)th;
      mo->flags &= ~0x40000000; // use a spare bit as "seen" (POC) – replace with your own shadow flag if needed
    }
  }

  // 2) For each snapshot mobj: update if exists by id, else spawn new
  for (int i=0;i<hdr->num_mobjs;i++) {
    const mobj_snapshot_t *ms = (const mobj_snapshot_t*)p; p += sizeof(*ms);

    // Find existing by id
    mobj_t *match = NULL;
    for (thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next) {
      if (th->function == P_MobjThinker)  continue;
      mobj_t *mo = (mobj_t*)th;
      if (mo->mobj_id == ms->id) { match = mo; break; }
    }

    fixed_t nx = ((fixed_t)ms->x) << FRACBITS;
    fixed_t ny = ((fixed_t)ms->y) << FRACBITS;
    fixed_t nz = ((fixed_t)ms->z) << FRACBITS;

    if (!match) {
      // spawn
      mobj_t *mo = P_SpawnMobj(nx, ny, nz, (mobjtype_t)ms->type);
      mo->mobj_id = ms->id; // preserve id from host
      mo->momx = ((fixed_t)ms->momx) << FRACBITS;
      mo->momy = ((fixed_t)ms->momy) << FRACBITS;
      mo->momz = ((fixed_t)ms->momz) << FRACBITS;
      mo->angle= (angle_t)ms->angle;
      mo->health = ms->health;
      mo->state  = &states[ms->state];
      mo->flags  = ms->flags;
      mo->tics   = ms->tics;
      // smoothing not needed on spawn
    } else {
      // mark seen
      match->flags |= 0x40000000;

      // Small correction? Lerp over a few tics. Large -> warp.
      const int dx = (match->x - nx) >> FRACBITS;
      const int dy = (match->y - ny) >> FRACBITS;
      const int dz = (match->z - nz) >> FRACBITS;
      int dist2 = dx*dx + dy*dy + dz*dz;

      if (dist2 <= 64) {
        net_smooth_t *slot = smooth_slot(ms->id);
        slot->target_x = ms->x;
        slot->target_y = ms->y;
        slot->target_z = ms->z;
        slot->lerp_ticks = 4; // ~0.11s
      } else {
        match->x = nx; match->y = ny; match->z = nz;
      }

      match->momx = ((fixed_t)ms->momx) << FRACBITS;
      match->momy = ((fixed_t)ms->momy) << FRACBITS;
      match->momz = ((fixed_t)ms->momz) << FRACBITS;
      match->angle= (angle_t)ms->angle;
      match->health = ms->health;
      match->state  = &states[ms->state];
      match->flags  = ms->flags;
      match->tics   = ms->tics;
    }
  }

  // 3) Remove any mobjs not seen in snapshot
  thinker_t *th = thinkercap.next;
  while (th != &thinkercap) {
    thinker_t *next = th->next;
    if (th->function == P_MobjThinker) {
      mobj_t *mo = (mobj_t*)th;
      if (!(mo->flags & 0x40000000)) {
        P_RemoveMobj(mo);
      }
    }
    th = next;
  }

  // Apply sectors
  for (int i=0;i<hdr->num_sectors;i++) {
    const sector_snapshot_t *ss = (const sector_snapshot_t*)p; p += sizeof(*ss);
    sector_t *s = &sectors[i];
    s->floorheight   = ((fixed_t)ss->floorheight) << FRACBITS;
    s->ceilingheight = ((fixed_t)ss->ceilingheight) << FRACBITS;
    s->lightlevel    = ss->lightlevel;
    s->special       = ss->special;
  }

  // Apply lines
  for (int i=0;i<hdr->num_lines;i++) {
    const line_snapshot_t *ls = (const line_snapshot_t*)p; p += sizeof(*ls);
    line_t *ln = &lines[i];
    ln->flags   = ls->flags;
    ln->special = ls->special;
  }
}

// -------------------------
// Public entry points
// -------------------------
static byte *host_out = NULL;
static size_t host_out_sz = 0;

void NET_Snapshot_SetIsHost(int is_host) {
  g_is_host = is_host;
}

void NET_Snapshot_Tick(void) {
  if (!netgame) return;
  if (g_is_host) {
    if (leveltime % SNAP_INTERVAL_TICS == 0) {
      if (host_out) Z_Free(host_out);
      host_out_sz = build_snapshot(&host_out);
      // Wrap with a packet header/type in your net layer; here we send raw blob.
      I_SendPacket((packet_header_t*)host_out, host_out_sz);
    }
  } else {
    // smoothing step – nudge mobjs towards targets
    if (smooth_cache) {
      for (thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next) {
        if (th->function == P_MobjThinker) continue;
        mobj_t *mo = (mobj_t*)th;
        net_smooth_t *slot = smooth_slot(mo->mobj_id);
        if (slot->lerp_ticks > 0) {
          // linear blend
          fixed_t tx = ((fixed_t)slot->target_x) << FRACBITS;
          fixed_t ty = ((fixed_t)slot->target_y) << FRACBITS;
          fixed_t tz = ((fixed_t)slot->target_z) << FRACBITS;
          mo->x = mo->x + (tx - mo->x) / slot->lerp_ticks;
          mo->y = mo->y + (ty - mo->y) / slot->lerp_ticks;
          mo->z = mo->z + (tz - mo->z) / slot->lerp_ticks;
          slot->lerp_ticks--;
        }
      }
    }
  }
}

void NET_Snapshot_Receive(const byte *data, int len) {
  if (g_is_host) return; // host ignores
  client_apply_snapshot(data, (size_t)len);
}
