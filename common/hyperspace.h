/************************************************************************************************//**
 * @file		hyperspace.h
 *
 * @copyright	Copyright 2022 Kurt Hildebrand.
 * @license		Licensed under the Apache License, Version 2.0 (the "License"); you may not use this
 *				file except in compliance with the License. You may obtain a copy of the License at
 *
 *				http://www.apache.org/licenses/LICENSE-2.0
 *
 *				Unless required by applicable law or agreed to in writing, software distributed under
 *				the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF
 *				ANY KIND, either express or implied. See the License for the specific language
 *				governing permissions and limitations under the License.
 *
 ***************************************************************************************************/
#ifndef HYPERSPACE_H
#define HYPERSPACE_H

#if __STDC_VERSION__ < 199901L
#error Compile with C99 or higher!
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* Includes -------------------------------------------------------------------------------------- */
#include <stdint.h>

#include <net/net_pkt.h>

#include "matrix.h"


/* Public Macros --------------------------------------------------------------------------------- */
#define HYPERSPACE_COORD_OPT_TYPE	(0x22)
#define PACKET_CACHE_TABLE_SIZE		(64)
#define PACKET_CACHE_ENTRY_TIMEOUT	(2*60*1000)	/* 2 min timeout */


/* Public Types ---------------------------------------------------------------------------------- */
typedef struct __packed {
	char     header[4];	/* HYPR */
	uint32_t type;
	float    x;
	float    y;
	float    z;
	uint8_t  bindex;
} Hyperspace_Coord_Update;


typedef struct __packed {
	float r;
	float t;
} Hypercoord;


/* TODO: source and destination coords need their own sequence counter */
/*                                 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                 | Opt Type      | Opt Length    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Src Coord Seq | Dst Coord Seq | Packet ID                     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Source Coordinate (Radius)                                    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Source Coordinate (Theta)                                     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Destination Coordinate (Radius)                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Destination Coordinate (Theta)                                |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
typedef struct __packed {
	uint8_t  src_seq;
	uint8_t  dest_seq;
	uint16_t packet_id;
	Hypercoord src;
	Hypercoord dest;
} HyperOpt;


typedef struct {
	struct in6_addr addr;
	Hypercoord coord;
	int64_t last_used;	/* The timestamp this routing entry was last used */
	struct k_work_delayable retry_timer;
	struct net_if* iface;
	uint8_t requests;	/* Number of requests sent */
	uint8_t coord_seq;
	uint8_t valid;		/* True = route is valid. False = route is pending. */

	/* remaining = reachable + reachable_timeout - current */
} HyperRoute;


typedef struct {
	uint8_t    lladdr[8];
	int64_t    last_seen;
	Hypercoord coord;
	uint16_t   last_dsn;	/* Last IEEE 802.15.4 data sequence number. */
	uint16_t   last_ebsn;	/* Last IEEE 802.15.4 beacon sequence number. */
} HyperNbr;


typedef struct {
	Hypercoord  coord;
	uint8_t     coord_seq;
	Vec3        last_loc;
	struct k_mutex nbr_mutex;
	struct k_mutex route_mutex;
} Hyperspace;


/* Public Functions ------------------------------------------------------------------------------ */
void      hyperspace_init       (void);
void      hyperspace_init_root  (void);
uint16_t  hyperspace_next_pkt_id(void);
bool      hyperspace_is_pkt_dup (struct net_pkt*);
int       hyperspace_send       (struct net_pkt*);
int       hyperspace_recv       (struct net_pkt*);
int       hyperspace_route      (struct net_pkt*);

uint8_t   hyperspace_coord_seq  (void);
float     hyperspace_coord_r    (void);
float     hyperspace_coord_t    (void);
void      hyperspace_update     (float, float, float);

struct net_ipv6_hdr* net_pkt_get_ipv6_hdr(struct net_pkt*);
HyperOpt*            net_pkt_get_hyperopt(struct net_pkt*);


#ifdef __cplusplus
}
#endif

#endif // HYPERSPACE_H
/******************************************* END OF FILE *******************************************/
