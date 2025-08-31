#pragma once
#include "doomtype.h"

void NET_Snapshot_SetIsHost(int is_host);
void NET_Snapshot_Tick(void);
void NET_Snapshot_Receive(const byte *data, int len);

// Packet type constant you’ll add to your net layer:
#define PKT_SNAPSHOT 0x53  // 'S'
