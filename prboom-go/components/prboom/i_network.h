/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *  Low level network interface.
 *-----------------------------------------------------------------------------*/


#ifndef I_NETWORK_H
#define I_NETWORK_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "protocol.h"

/* SDL_net compatibility typedefs */
typedef uint16_t Uint16;
typedef uint8_t  byte;
typedef int      UDP_SOCKET;   /* socket file descriptor */
typedef int      UDP_CHANNEL;  /* index into channel map */

/* IPaddress-like struct (SDL_net compatible) */
typedef struct {
    uint32_t host;  /* network byte order (same as in_addr.s_addr) */
    Uint16   port;  /* network byte order port */
} IPaddress;

/* Global network state */
extern UDP_CHANNEL sentfrom;     /* channel index of last received packet */
extern IPaddress   sentfrom_addr;
extern UDP_SOCKET  udp_socket;   /* UDP socket descriptor */
extern size_t      sentbytes;    /* Total bytes sent */
extern size_t      recvdbytes;   /* Total bytes received */

/* Network interface functions */
void I_InitNetwork(void);
void I_ShutdownNetwork(void);

UDP_SOCKET I_Socket(Uint16 port);
void I_CloseSocket(UDP_SOCKET sock);

int I_ConnectToServer(const char *serv);
void I_Disconnect(void);

UDP_CHANNEL I_RegisterPlayer(IPaddress *ipaddr);
void I_UnRegisterPlayer(UDP_CHANNEL channel);

size_t I_GetPacket(packet_header_t* buffer, size_t buflen);
void I_SendPacket(packet_header_t* packet, size_t len);
void I_SendPacketTo(packet_header_t* packet, size_t len, UDP_CHANNEL *to);

void I_WaitForPacket(int ms);
void I_PrintAddress(FILE* fp, UDP_CHANNEL *addr);

/* Packet alloc/free helpers (SDL_net compatibility) */
typedef struct {
    byte *data;
    int   maxlen;
    int   len;
    UDP_CHANNEL channel;
    struct sockaddr_in address;
} UDP_PACKET;

UDP_PACKET *I_AllocPacket(int size);
void I_FreePacket(UDP_PACKET *packet);

#endif /* I_NETWORK_H */
