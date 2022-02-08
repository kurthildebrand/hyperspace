/************************************************************************************************//**
 * @file		hyperspace.c
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
#include <ipv6.h>
#include <math.h>
#include <string.h>
#include <zephyr.h>

#include "calc.h"
#include "hyperspace.h"
#include "location.h"
#include "pool.h"
#include "ringbuffer.h"
#include "tsch.h"

#include "logging/log.h"
LOG_MODULE_REGISTER(hyperspace, LOG_LEVEL_DBG);


/* Private Constants ----------------------------------------------------------------------------- */
#define HYPER_LATTICE_R					(2.6339157938f)
#define NUM_HYPERROUTES					(16)
// #define MAX_HYPER_COORD_REQUESTS		(1)
#define MAX_HYPER_COORD_REQUESTS		(3)
#define COORD_REQUEST_TIMEOUT_MS		(30*1000)	/* Coordinate request timeout in seconds */
#define HYPER_ROUTE_TIMEOUT_MS			(5*60*1000)	/* Hyperspace route timeout in ms */
#define PACKET_CACHE_TABLE_SIZE			(64)
#define PACKET_CACHE_ENTRY_TIMEOUT_MS	(2*60*1000)	/* 2 min timeout */


/* Private Types --------------------------------------------------------------------------------- */
typedef struct {
	int64_t timestamp;		/* Packet reception in ms. */
	struct in6_addr src;	/* Packet source address. */
	uint16_t packet_id;		/* Packet ID given by hyperspace HBH option. */
	uint16_t fragmented;	/* True if the packet was fragmented. False if it wasn't. */
	uint16_t fragoffset;	/* Fragment offset. */
} HyperCache;


/* Private Functions ----------------------------------------------------------------------------- */
static void        hyperspace_pkt_cache_init   (void);
static bool        hyperspace_pkt_cache_put    (struct net_ipv6_hdr*, HyperOpt*, bool, uint16_t);
static bool        hyperspace_pkt_cache_find   (struct net_ipv6_hdr*, HyperOpt*, bool, uint32_t);
static void        hyperspace_pkt_cache_timeout(struct k_work*);
static void        hyperspace_pkt_cache_update (void);

static HyperRoute* hyperspace_route_alloc (struct in6_addr*, struct net_if*);
static void        hyperspace_route_remove(HyperRoute*);
static void        hyperspace_route_clean (void);
static HyperRoute* hyperspace_route_find  (struct in6_addr*);

static struct net_pkt*      create_coord_req (struct net_if*, struct in6_addr*, uint16_t, uint16_t);
static void                 coord_req_timeout(struct k_work*);

// static struct net_ipv6_hdr* net_pkt_get_ipv6_hdr   (struct net_pkt*);
// static HyperOpt*            net_pkt_get_hyperopt   (struct net_pkt*);
static bool                 net_pkt_get_frag_offset(struct net_pkt*, uint16_t*);

static Neighbor* hyperspace_closest  (const Hypercoord*);
static float     hyperspace_dist     (float, float, float, float);
static void      hyperspace_translate(double[2], double, double);


/* Private Variables ----------------------------------------------------------------------------- */
NET_L2_DECLARE_PUBLIC(TSCH_L2);

static HyperCache packet_caches[PACKET_CACHE_TABLE_SIZE];
static RingBuffer packet_rb;
static struct k_work_delayable packet_cache_work;
static uint16_t packet_id;

HyperRoute hyperroutes[NUM_HYPERROUTES];
Hyperspace hyperspace;
Pool hyperroute_pool;


/* hyperspace_init ******************************************************************************//**
 * @brief		Initializes hyperspace routing. */
void hyperspace_init(void)
{
	hyperspace.coord.r   = NAN;
	hyperspace.coord.t   = NAN;
	hyperspace.coord_seq = 0;
	hyperspace.last_loc  = make_vec3(NAN, NAN, NAN);

	hyperspace_pkt_cache_init();

	pool_init(&hyperroute_pool, hyperroutes, NUM_HYPERROUTES, sizeof(hyperroutes[0]));
}


/* hyperspace_init_root *************************************************************************//**
 * @brief		Initializes hyperspace routing as the root node. */
void hyperspace_init_root(void)
{
	hyperspace.coord.r   = 0;
	hyperspace.coord.t   = 0;
	hyperspace.coord_seq = 0;
	hyperspace.last_loc  = make_vec3(0, 0, 0);

	hyperspace_pkt_cache_init();

	pool_init(&hyperroute_pool, hyperroutes, NUM_HYPERROUTES, sizeof(hyperroutes[0]));
}


/* hyperspace_coord_seq *************************************************************************//**
 * @brief		Returns this node's hyperspace coordinate sequence number. */
uint8_t hyperspace_coord_seq(void)
{
	return hyperspace.coord_seq;
}


/* hyperspace_coord_r ***************************************************************************//**
 * @brief		Returns this node's hyperspace radius coordinate. */
float hyperspace_coord_r(void)
{
	return hyperspace.coord.r;
}


/* hyperspace_coord_t ***************************************************************************//**
 * @brief		Returns this node's hyperspace theta coordinate. */
float hyperspace_coord_t(void)
{
	return hyperspace.coord.t;
}


/* hyperspace_update ****************************************************************************//**
 * @brief		Updates this node's hyperspace coordinate. */
void hyperspace_update(float x, float y, float z)
{
	Vec3 loc = make_vec3(x, y, z);

	if(!isfinite(x) || !isfinite(y) || !isfinite(z))
	{
		hyperspace.last_loc = loc;
		hyperspace.coord.r  = NAN;
		hyperspace.coord.t  = NAN;
	}
	else if(!isfinite(hyperspace.last_loc.x) ||
	        !isfinite(hyperspace.last_loc.y) ||
	        !isfinite(hyperspace.last_loc.z) ||
	        vec3_dist(hyperspace.last_loc, loc) > 2.0f * LATTICE_R)
	{
		hyperspace.last_loc = loc;

		double v[2] = { 0, 0 };
		x = roundf(x / LATTICE_R) * HYPER_LATTICE_R;
		y = roundf(y / LATTICE_R) * HYPER_LATTICE_R;
		z = roundf(z / LATTICE_R) * HYPER_LATTICE_R;

		/* x > y > z */
		if(x >= y && y >= z)
		{
			hyperspace_translate(v, z, calc_deg_to_rad_f(120));
			hyperspace_translate(v, y, calc_deg_to_rad_f(60));
			hyperspace_translate(v, x, calc_deg_to_rad_f(0));
		}
		/* x > z > y */
		else if(x >= z && z >= y)
		{
			hyperspace_translate(v, y, calc_deg_to_rad_f(60));
			hyperspace_translate(v, z, calc_deg_to_rad_f(120));
			hyperspace_translate(v, x, calc_deg_to_rad_f(0));
		}
		/* y > x > z */
		else if(y >= x && x >= z)
		{
			hyperspace_translate(v, z, calc_deg_to_rad_f(120));
			hyperspace_translate(v, x, calc_deg_to_rad_f(0));
			hyperspace_translate(v, y, calc_deg_to_rad_f(60));
		}
		/* y > z > x */
		else if(y >= z && z >= x)
		{
			hyperspace_translate(v, x, calc_deg_to_rad_f(0));
			hyperspace_translate(v, z, calc_deg_to_rad_f(120));
			hyperspace_translate(v, y, calc_deg_to_rad_f(60));
		}
		/* z > x > y */
		else if(z >= x && x >= y)
		{
			hyperspace_translate(v, y, calc_deg_to_rad_f(60));
			hyperspace_translate(v, x, calc_deg_to_rad_f(0));
			hyperspace_translate(v, z, calc_deg_to_rad_f(120));
		}
		/* z > y > x */
		else if(z >= y && y >= x)
		{
			hyperspace_translate(v, x, calc_deg_to_rad_f(0));
			hyperspace_translate(v, y, calc_deg_to_rad_f(60));
			hyperspace_translate(v, z, calc_deg_to_rad_f(120));
		}

		hyperspace.coord.r = v[0];
		hyperspace.coord.t = v[1];
		hyperspace.coord_seq++;

		// if(hyperspace.on_coord_update)
		// {
		// 	hyperspace.on_coord_update(hyperspace.coord.r, hyperspace.coord.t, loc.x, loc.y, loc.z);
		// }
	}
}


/* hyperspace_next_pkt_id ***********************************************************************//**
 * @brief		Returns the next packet id. */
uint16_t hyperspace_next_pkt_id(void)
{
	return packet_id++;
}


/* hyperspace_is_pkt_dup ************************************************************************//**
 * @brief		Checks if a packet has already been received. */
bool hyperspace_is_pkt_dup(struct net_pkt* pkt)
{
	uint16_t fragoffset;
	struct net_ipv6_hdr* hdr = net_pkt_get_ipv6_hdr(pkt);
	HyperOpt* hyperopt       = net_pkt_get_hyperopt(pkt);
	bool      fragmented     = net_pkt_get_frag_offset(pkt, &fragoffset);

	if(!hyperopt)
	{
		LOG_ERR("no hyperopt");
		return true;
	}

	if(net_ipv6_is_my_addr(&hdr->src))
	{
		LOG_DBG("packet originated from this node.");
		return true;
	}

	return hyperspace_pkt_cache_find(hdr, hyperopt, fragmented, fragoffset);
}


/* hyperspace_send ******************************************************************************//**
 * @brief		Sends a packet from this node using hyperspace routing. */
int hyperspace_send(struct net_pkt* pkt)
{
	/* hyperspace_send needs to set packet lladdr when returning NET_OK. hyperspace_send must unref
	 * the packet if the packet needs to be dropped. The function call sequence is:
	 *
	 * 		net_send_data()
	 * 			if(net_if_send_data(net_pkt_iface(pkt), pkt) == NET_DROP) {
	 * 				return -EIO;
	 * 			}
	 * 			return 0;
	 *
	 * 			net_if_send_data():
	 * 				verdict = net_ipv6_prepare_for_send():
	 * 					hyperspace_send()
	 * 				if(verdict == NET_DROP) { ... }
	 * 				else if(verdict == NET_OK) {
	 * 					net_if_queue_tx(iface, pkt);
	 * 				}
	 * 				return verdict;
	 */
	LOG_DBG("send");

	struct net_pkt*      req = 0;
	struct net_ipv6_hdr* hdr = net_pkt_get_ipv6_hdr(pkt);
	HyperOpt* hyperopt = net_pkt_get_hyperopt(pkt);

	if(!hyperopt)
	{
		LOG_ERR("no hyperopt");
		return NET_DROP;
	}

	if(hdr->dst.s6_addr[15] == 1)
	{
		__asm("nop");
	}

	/* Set source coordinate */
	hyperopt->src     = hyperspace.coord;
	hyperopt->src_seq = hyperspace.coord_seq;
	HyperRoute* route = hyperspace_route_find(&hdr->dst);

	/* No route to destination. Create a blank hyperspace routing entry to the destination. */
	if(!route)
	{
		hyperspace_route_clean();

		route = hyperspace_route_alloc(&hdr->dst, net_pkt_iface(pkt));

		if(!route)
		{
			LOG_ERR("DROP: hyperspace_route_alloc failed");
			return NET_DROP;
		}

		/* Send a hyperspace coordinate request */
		req = create_coord_req(route->iface, &hdr->dst, 0, route->requests);
		if(!req)
		{
			LOG_ERR("DROP: could not allocate coordinate request");
			return NET_DROP;
		}

		/* Enqueue coord request and start a timeout */
		net_if_queue_tx(route->iface, req);
		k_work_init_delayable(&route->retry_timer, coord_req_timeout);
		k_work_schedule(&route->retry_timer, K_MSEC(COORD_REQUEST_TIMEOUT_MS));
	}

	Neighbor* nbr = 0;

	/* If unknown dest coordinates, broadcast the packet */
	if(!isfinite(route->coord.r) || !isfinite(route->coord.t))
	{
		net_pkt_lladdr_dst(pkt)->addr = tsch_bcast_addr();
		net_pkt_lladdr_dst(pkt)->type = NET_LINK_IEEE802154;
		net_pkt_lladdr_dst(pkt)->len  = 8;

		LOG_DBG("tx to bcast");
		return NET_OK;
	}
	/* Otherwise, setup the packet to be forwarded to the closest neighbor */
	else
	{
		hyperopt->dest     = route->coord;
		hyperopt->dest_seq = route->coord_seq;
		nbr = hyperspace_closest(&route->coord);
	}

	/* If a neighbor is closer, forward to neighbor */
	if(nbr)
	{
		net_pkt_lladdr_dst(pkt)->addr = nbr->address;
		net_pkt_lladdr_dst(pkt)->type = NET_LINK_IEEE802154;
		net_pkt_lladdr_dst(pkt)->len  = 8;

		LOG_DBG("tx to %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
			nbr->address[0], nbr->address[1], nbr->address[2], nbr->address[3],
			nbr->address[4], nbr->address[5], nbr->address[6], nbr->address[7]);
	}
	/* Otherwise, this node is the closest. The destination node is within range of this node. */
	else
	{
		net_pkt_lladdr_dst(pkt)->addr = 0;
		net_pkt_lladdr_dst(pkt)->type = 0;
		net_pkt_lladdr_dst(pkt)->len  = 0;

		LOG_DBG("tx to local");
	}

	return NET_OK;
}


/* hyperspace_recv ******************************************************************************//**
 * @brief		Receives a packet routed using hyperspace routing. */
int hyperspace_recv(struct net_pkt* pkt)
{
	/* Update hyperspace coordinates. For now, we are just flooding packets so no routes need to be
	 * updated. Do not unref the packet if returning NET_OK or NET_DROP. The function call sequence
	 * is:
	 *
	 * 		net_recv_data():
	 * 		net_queue_rx():
	 * 		process_rx_packet():
	 * 		net_rx():
	 * 		processing_data():
	 * 			switch (process_data(pkt, is_loopback)) {
	 * 			case NET_OK:
	 * 				NET_DBG("Consumed pkt %p", pkt);
	 * 				break;
	 * 			case NET_DROP:
	 * 			default:
	 * 				NET_DBG("Dropping pkt %p", pkt);
	 * 				net_pkt_unref(pkt);
	 * 				break;
	 * 			}
	 *
	 * 			process_data():
	 * 			net_ipv6_input():
	 * 				if(!net_ipv6_is_my_addr(pkt)) {
	 * 					return route_packet();
	 * 				}
	 *
	 * 				if(net_if_flag_is_set(net_pkt_iface(pkt), NET_IF_HYPERSPACE)) {
	 * 					if(hyperspace_recv(pkt) == NET_DROP) {
	 * 						return NET_DROP;
	 * 					}
	 * 				}
	 */
	uint16_t fragoffset;

	struct net_ipv6_hdr* hdr = net_pkt_get_ipv6_hdr   (pkt);
	HyperOpt* hyperopt       = net_pkt_get_hyperopt   (pkt);
	bool      fragmented     = net_pkt_get_frag_offset(pkt, &fragoffset);

	if(!hyperopt)
	{
		LOG_ERR("DROP: no hyperopt");
		return NET_DROP;	/* -EIO */
	}

	if(hdr->dst.s6_addr[15] == 1)
	{
		__asm("nop");
	}

	/* Update an existing hyperspace routing entry if it exists */
	HyperRoute* route = hyperspace_route_find(&hdr->src);

	/* Create a new hyperspace routing entry if none exists */
	if(!route)
	{
		hyperspace_route_clean();

		route = hyperspace_route_alloc(&hdr->src, net_pkt_iface(pkt));

		if(!route)
		{
			LOG_ERR("DROP: hyperspace_route_alloc failed");
			return NET_DROP;	/* -ENOBUFS */
		}
	}

	/* Update route back to the packet source */
	if((!route->valid || (int)(hyperopt->src_seq - route->coord_seq) > 0) &&
	   (isfinite(hyperopt->src.r) && isfinite(hyperopt->src.t)))
	{
		LOG_DBG("update hyperspace route");
		route->coord     = hyperopt->src;
		route->coord_seq = hyperopt->src_seq;
		route->last_used = k_uptime_get();
		route->valid     = true;
		k_work_cancel_delayable(&route->retry_timer);
	}

	/* Check the hyperspace HBH option to make sure we haven't already received the packet. */
	if(!hyperspace_pkt_cache_put(hdr, hyperopt, fragmented, fragoffset))
	{
		LOG_DBG("DROP: packet is a duplicate");
		return NET_DROP;
	}

	/* TODO: set packet source lladdr? */
	LOG_DBG("recv");
	return NET_OK;
}


/* hyperspace_route *****************************************************************************//**
 * @brief		Routes a packet through this node using hyperspace routing. */
int hyperspace_route(struct net_pkt* pkt)
{
	/* Update old coordinate if it exists: if hyperspace routing table is newer, update packet's
	 * coordinates. If packet's coordinates are newer, update routing table. For now, we are just
	 * flooding packets so no routes need to be updated.
	 *
	 * hyperspace_route needs to set the packet's link layer address for the next hop. If returning
	 * NET_OK, hyperspace_route() must queue the packet in the net_if's tx queue by calling
	 * net_if_queue_tx. If returning NET_DROP, do not unref the packet. The function call sequence
	 * is:
	 *
	 * 		net_recv_data():
	 * 		net_queue_rx():
	 * 		process_rx_packet():
	 * 		net_rx():
	 * 		processing_data():
	 * 			switch (process_data(pkt, is_loopback)) {
	 * 			case NET_OK:
	 * 				NET_DBG("Consumed pkt %p", pkt);
	 * 				break;
	 * 			case NET_DROP:
	 * 			default:
	 * 				NET_DBG("Dropping pkt %p", pkt);
	 * 				net_pkt_unref(pkt);
	 * 				break;
	 * 			}
	 *
	 * 			process_data():
	 * 			net_ipv6_input():
	 * 				if (!net_ipv6_is_addr_mcast(&hdr->dst)) {
	 * 					if (!net_ipv6_is_my_addr(&hdr->dst)) {
	 * 						if(hyperspace_route(pkt) == NET_OK) {
	 * 							return NET_OK;
	 * 						} else if(ipv6_route_packet(pkt, hdr) == NET_OK) {
	 * 							return NET_OK;
	 * 						}
	 *
	 * 						goto drop;
	 * 					}
	 * 				}
	 */
	uint16_t fragoffset;

	struct net_if* iface = net_if_get_first_by_type(&NET_L2_GET_NAME(TSCH_L2));

	/* TODO: net_pkt_set_iface? */
	net_pkt_set_forwarding(pkt, true);

	/* TODO: (debug) check that lladdr_src is set from the last hop */
	struct net_ipv6_hdr* hdr = net_pkt_get_ipv6_hdr(pkt);
	HyperOpt* hyperopt       = net_pkt_get_hyperopt(pkt);
	bool      fragmented     = net_pkt_get_frag_offset(pkt, &fragoffset);

	if(!hyperopt)
	{
		return -EIO;
	}

	/* Todo: this is terrible. Need ND across SPIS_IF to get rid of this. This code is here to
	 * prevent packets addressed to the other side of the SPIS_IF from being transmitted to the
	 * local mesh.
	 *
	 * This also prevents routing multicast packets that match the lower 8 bytes (which is actually
	 * desirable). */
	if(hdr->dst.s6_addr[8]  == 0 &&
	   hdr->dst.s6_addr[9]  == 0 &&
	   hdr->dst.s6_addr[10] == 0 &&
	   hdr->dst.s6_addr[11] == 0 &&
	   hdr->dst.s6_addr[12] == 0 &&
	   hdr->dst.s6_addr[13] == 0 &&
	   hdr->dst.s6_addr[14] == 0 &&
	   hdr->dst.s6_addr[15] == 1)
	{
		return NET_DROP;
	}

	/* Update an existing hyperspace routing entry if it exists */
	HyperRoute* dest_route = hyperspace_route_find(&hdr->dst);
	HyperRoute* src_route  = hyperspace_route_find(&hdr->src);

	if(dest_route)
	{
		/* Update packet's destination coordinate if route's coordinate is newer */
		if(dest_route->valid && (((int)(hyperopt->dest_seq - dest_route->coord_seq) < 0) ||
		   !isfinite(hyperopt->dest.r) || !isfinite(hyperopt->dest.t)))
		{
			hyperopt->dest     = dest_route->coord;
			hyperopt->dest_seq = dest_route->coord_seq;
		}
		/* Update destination route's coordinate if the packet's destination coordinate is newer */
		else if((!dest_route->valid || (int)(hyperopt->dest_seq - dest_route->coord_seq) > 0) &&
		        (isfinite(hyperopt->dest.r) && isfinite(hyperopt->dest.t)))
		{
			dest_route->coord     = hyperopt->dest;
			dest_route->coord_seq = hyperopt->dest_seq;
			dest_route->valid     = true;
			k_work_cancel_delayable(&dest_route->retry_timer);
		}
	}

	if(src_route)
	{
		/* Update packet's source coordinate if route's coordinate is newer */
		if(src_route->valid && (((int)(hyperopt->src_seq - src_route->coord_seq) < 0) ||
		   !isfinite(hyperopt->src.r) || !isfinite(hyperopt->src.t)))
		{
			hyperopt->src     = src_route->coord;
			hyperopt->src_seq = src_route->coord_seq;
		}
		/* Update source route's coordinate if the packet's source coordinate is newer */
		else if((!src_route->valid || (int)(hyperopt->src_seq - src_route->coord_seq) > 0) &&
		        (isfinite(hyperopt->src.r) && isfinite(hyperopt->src.t)))
		{
			src_route->coord     = hyperopt->src;
			src_route->coord_seq = hyperopt->src_seq;
			src_route->valid     = true;
			k_work_cancel_delayable(&src_route->retry_timer);
		}
	}

	/* Do not route packets that we have sent. Do not route packets that we have seen already. */
	if(net_ipv6_is_my_addr(&hdr->src) ||
	   !hyperspace_pkt_cache_put(hdr, hyperopt, fragmented, fragoffset))
	{
		LOG_DBG("DROP: packet is a duplicate");
		return NET_DROP;
	}

	/* Note: fix because Zephyr doesn't actually decrement the hop limit */
	if(--hdr->hop_limit == 0)
	{
		LOG_DBG("DROP: hop limit reached");
		return NET_DROP;
	}

	/* Send to all connected nodes if no destination coordinates */
	if(!isfinite(hyperopt->dest.r) || !isfinite(hyperopt->dest.t))
	{
		/* Forward to all connected nodes */
		net_pkt_lladdr_dst(pkt)->addr = tsch_bcast_addr();
		net_pkt_lladdr_dst(pkt)->type = NET_LINK_IEEE802154;
		net_pkt_lladdr_dst(pkt)->len  = 8;

		pkt->iface = iface;
		net_if_queue_tx(iface, pkt);
		return NET_OK;
	}

	/* TODO: forward packet to spis_if if opt->dest.r == 0.0f && opt->dest.t == 0.0f
	 * Set packet iface = spis_if. */

	/* Search for the hyperspace neighbor closest to the destination */
	/* Todo: what if nbr is 0? */
	Neighbor* nbr = hyperspace_closest(&hyperopt->dest);

	if(nbr)
	{
		net_pkt_lladdr_dst(pkt)->addr = nbr->address;
		net_pkt_lladdr_dst(pkt)->type = NET_LINK_IEEE802154;
		net_pkt_lladdr_dst(pkt)->len  = 8;

		LOG_DBG("route to %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
			nbr->address[0], nbr->address[1], nbr->address[2], nbr->address[3],
			nbr->address[4], nbr->address[5], nbr->address[6], nbr->address[7]);
	}
	else
	{
		net_pkt_lladdr_dst(pkt)->addr = 0;
		net_pkt_lladdr_dst(pkt)->type = 0;
		net_pkt_lladdr_dst(pkt)->len  = 0;

		LOG_DBG("route to local");
	}

	pkt->iface = iface;
	net_if_queue_tx(iface, pkt);
	return NET_OK;
}


/* net_pkt_get_ipv6_hdr *************************************************************************//**
 * @brief		Returns the ipv6 header from a packet. */
struct net_ipv6_hdr* net_pkt_get_ipv6_hdr(struct net_pkt* pkt)
{
	NET_PKT_DATA_ACCESS_CONTIGUOUS_DEFINE(ipv6_access, struct net_ipv6_hdr);
	struct net_pkt_cursor cursor;

	net_pkt_cursor_backup(pkt, &cursor);
	net_pkt_cursor_init(pkt);
	struct net_ipv6_hdr* hdr = net_pkt_get_data(pkt, &ipv6_access);
	net_pkt_cursor_restore(pkt, &cursor);

	return hdr;
}


/* net_pkt_get_hyperopt *************************************************************************//**
 * @brief		Returns the hyperspace hop-by-hop option from the packet if it exists. */
HyperOpt* net_pkt_get_hyperopt(struct net_pkt* pkt)
{
	NET_PKT_DATA_ACCESS_CONTIGUOUS_DEFINE(ipv6_access, struct net_ipv6_hdr);
	NET_PKT_DATA_ACCESS_CONTIGUOUS_DEFINE(hyperopt_access, HyperOpt);
	struct net_pkt_cursor cursor;
	struct net_ipv6_hdr* hdr;
	HyperOpt* hyperopt = 0;

	net_pkt_cursor_backup(pkt, &cursor);
	net_pkt_set_overwrite(pkt, true);
	net_pkt_cursor_init(pkt);
	hdr = net_pkt_get_data(pkt, &ipv6_access);
	net_pkt_acknowledge_data(pkt, &ipv6_access);

	if(hdr->nexthdr == NET_IPV6_NEXTHDR_HBHO)
	{
		uint8_t nexthdr;
		uint8_t hdrlen;

		net_pkt_read_u8(pkt, &nexthdr);
		net_pkt_read_u8(pkt, &hdrlen);

		uint16_t i = 2;
		while(i < hdrlen * 8 + 8 && !hyperopt)
		{
			uint8_t opt_type;
			uint8_t opt_len;

			/* Read option type */
			if(net_pkt_read_u8(pkt, &opt_type))
			{
				goto exit;	/* -ENOBUFS */
			}

			/* Read option length if not pad1 */
			if(opt_type != NET_IPV6_EXT_HDR_OPT_PAD1 && net_pkt_read_u8(pkt, &opt_len))
			{
				goto exit;	/* -ENOBUFS */
			}

			switch(opt_type) {
			case NET_IPV6_EXT_HDR_OPT_PAD1:
				i++;
				break;

			case HYPERSPACE_COORD_OPT_TYPE:
				hyperopt = net_pkt_get_data(pkt, &hyperopt_access);
				net_pkt_acknowledge_data(pkt, &hyperopt_access);
				break;

			default:
				i += opt_len + 2;
				if(net_pkt_skip(pkt, opt_len))
				{
					goto exit;	/* -ENOBUFS */
				}
				break;
			}
		}
	}

	exit:
		net_pkt_cursor_restore(pkt, &cursor);
		return hyperopt;
}


/* net_pkt_get_frag_offset **********************************************************************//**
 * @brief		Returns the fragment offset if the packet is fragmented.
 * @retval		true if the packet is fragmented.
 * @retval		false if the packet is not fragmented. */
static bool net_pkt_get_frag_offset(struct net_pkt* pkt, uint16_t* offset)
{
	struct net_pkt_cursor backup;
	net_pkt_cursor_backup(pkt, &backup);
	net_pkt_set_overwrite(pkt, true);
	net_pkt_cursor_init(pkt);

	NET_PKT_DATA_ACCESS_CONTIGUOUS_DEFINE(ipv6_access, struct net_ipv6_hdr);
	struct net_ipv6_hdr* ipv6_hdr = net_pkt_get_data(pkt, &ipv6_access);

	net_pkt_acknowledge_data(pkt, &ipv6_access);

	uint8_t  hdr = ipv6_hdr->nexthdr;
	uint8_t  next_hdr;
	uint16_t length;

	while(1)
	{
		uint8_t temp;

		if(net_ipv6_is_nexthdr_upper_layer(hdr))
		{
			break;
		}

		/* Read next header */
		net_pkt_read_u8(pkt, &next_hdr);

		/* Read length */
		net_pkt_read_u8(pkt, &temp);
		length = temp * 8 + 8;

		if(hdr == NET_IPV6_NEXTHDR_FRAG)
		{
			net_pkt_read_be16(pkt, offset);
			*offset &= 0xFFF8;

			// net_pkt_skip(pkt, 2);	/* skip over fragment offset */
			// net_pkt_read_be32(pkt, id);
			net_pkt_cursor_restore(pkt, &backup);
			return true;
		}

		net_pkt_skip(pkt, length-2);

		hdr = next_hdr;
	}

	net_pkt_cursor_restore(pkt, &backup);
	return false;
}


/* create_coord_req *****************************************************************************//**
 * @brief		*/
static struct net_pkt* create_coord_req(
	struct net_if* iface,
	struct in6_addr* dest,
	uint16_t identifier,
	uint16_t sequence)
{
	NET_PKT_DATA_ACCESS_CONTIGUOUS_DEFINE(icmpv6_access, struct net_icmpv6_echo_req);

	const struct in6_addr* src = net_if_ipv6_select_src_addr(iface, dest);

	struct net_pkt* pkt = net_pkt_alloc_with_buffer(
		iface,
		sizeof(struct net_icmpv6_echo_req),
		AF_INET6,
		IPPROTO_ICMPV6,
		K_SECONDS(1));

	if(!pkt)
	{
		LOG_ERR("ENOMEM");
		return 0;	/* -ENOMEM */
	}

	if(net_ipv6_create(pkt, src, dest) != 0)
	{
		goto drop;
	}
	else if(net_icmpv6_create(pkt, NET_ICMPV6_ECHO_REQUEST, 0) != 0)
	{
		goto drop;
	}

	struct net_icmpv6_echo_req* echo_req = net_pkt_get_data(pkt, &icmpv6_access);
	if(!echo_req)
	{
		goto drop;
	}

	echo_req->identifier = htons(identifier);
	echo_req->sequence   = htons(sequence);
	net_pkt_set_data(pkt, &icmpv6_access);

	net_pkt_lladdr_src(pkt)->addr = net_pkt_lladdr_if(pkt)->addr;
	net_pkt_lladdr_src(pkt)->type = net_pkt_lladdr_if(pkt)->type;
	net_pkt_lladdr_src(pkt)->len  = net_pkt_lladdr_if(pkt)->len;

	/* Todo: forward to beacons */
	net_pkt_lladdr_dst(pkt)->addr = tsch_bcast_addr();
	net_pkt_lladdr_dst(pkt)->type = NET_LINK_IEEE802154;
	net_pkt_lladdr_dst(pkt)->len  = 8;

	net_pkt_cursor_init(pkt);
	net_ipv6_finalize(pkt, IPPROTO_ICMPV6);

	/* Set coordinates after net_ipv6_finalize as calling net_pkt_get_hyperopt somehow messes up
	 * setting the packet length when called before net_ipv6_finalize. Need to set coordinates here
	 * because a hyperspace coord request may be directly appended to the net_if instead of passing
	 * through net_send_data -> net_ipv6_prepare_for_send (which is where the coordinates are
	 * normally set). */
	HyperOpt* opt = net_pkt_get_hyperopt(pkt);
	if(!opt)
	{
		goto drop;
	}

	opt->src.r  = hyperspace.coord.r;
	opt->src.t  = hyperspace.coord.t;
	opt->dest.r = NAN;
	opt->dest.t = NAN;
	return pkt;

	drop:
		net_pkt_unref(pkt);
		return 0;
}


/* coord_req_timeout ****************************************************************************//**
 * @brief		*/
static void coord_req_timeout(struct k_work* work)
{
	LOG_DBG("coord req timeout");

	k_mutex_lock(&hyperspace.route_mutex, K_FOREVER);

	HyperRoute*     route = CONTAINER_OF(work, HyperRoute, retry_timer);
	struct net_pkt* req   = 0;

	if(++route->requests < MAX_HYPER_COORD_REQUESTS)
	{
		req = create_coord_req(route->iface, &route->addr, 0, route->requests);
		if(req)
		{
			net_if_queue_tx(route->iface, req);
			k_work_schedule(&route->retry_timer, K_MSEC(COORD_REQUEST_TIMEOUT_MS));
			k_mutex_unlock(&hyperspace.route_mutex);
			return;
		}
		else
		{
			LOG_ERR("failed allocating hyperspace coord req");
			hyperspace_route_remove(route);
		}
	}
	else
	{
		LOG_DBG("route timeout");
		hyperspace_route_remove(route);
	}

	k_mutex_unlock(&hyperspace.route_mutex);
}


/* hypernbr_closest *****************************************************************************//**
 * @brief		Returns the hyperspace neighbors closest to the specified coordinates.
 * @TODO:		return the closest 'connected' neighbor. */
static Neighbor* hyperspace_closest(const Hypercoord* coord)
{
	/* Find the neighbor closest to the destination */
	float     min_dist = hyperspace_dist(hyperspace.coord.r, hyperspace.coord.t, coord->r, coord->t);
	Neighbor* min_nbr  = 0;
	unsigned  i;

	for(i = 0; i < loc_nbrs_size(); i++)
	{
		Neighbor* ptr = loc_nbrs(i);

		if(ptr)
		{
			float dist = hyperspace_dist(ptr->r, ptr->t, coord->r, coord->t);

			if(dist < min_dist)
			{
				min_dist = dist;
				min_nbr  = ptr;
			}
		}
	}

	return min_nbr;
}





// ----------------------------------------------------------------------------------------------- //
// Hyperspace Packet Cache                                                                         //
// ----------------------------------------------------------------------------------------------- //
/* hyperspace_pkt_cache_init ********************************************************************//**
 * @brief		Initializes the hyperspace packet cache table. */
static void hyperspace_pkt_cache_init(void)
{
	rb_init(&packet_rb, packet_caches, PACKET_CACHE_TABLE_SIZE, sizeof(packet_caches[0]));

	k_work_init_delayable(&packet_cache_work, hyperspace_pkt_cache_timeout);
}


/* hyperspace_pkt_cache *************************************************************************//**
 * @brief		Caches a packet by inserting the packet source address and packet id into the rx
 * 				packet cache. The packet must have a hyperspace HBH option to retrieve the packet id.
 * 				Cache entries are time limited.
 * @retval		true if no duplicate packet was found and the packet was inserted into the cache.
 * @retval		false if the packet already exists in the cache. */
static bool hyperspace_pkt_cache_put(
	struct net_ipv6_hdr* hdr,
	HyperOpt* opt,
	bool      fragmented,
	uint16_t  fragoffset)
{
	HyperCache cache;

	/* Check if the packet has already been received */
	if(hyperspace_pkt_cache_find(hdr, opt, fragmented, fragoffset))
	{
		LOG_DBG("duplicate");
		return false;
	}

	cache.timestamp  = k_uptime_get();
	cache.src        = hdr->src;
	cache.packet_id  = opt->packet_id;
	cache.fragmented = fragmented;
	cache.fragoffset = fragmented ? fragoffset : 0;

	LOG_DBG("\r\n"
		"\tpacket_id  %d\r\n"
		"\tfragmented %d\r\n"
		"\tfragoffset %d\r\n"
		"\trb count   %d",
		cache.packet_id,
		cache.fragmented,
		cache.fragoffset,
		rb_count(&packet_rb));

	if(rb_empty(&packet_rb))
	{
		k_work_schedule(&packet_cache_work, K_MSEC(PACKET_CACHE_ENTRY_TIMEOUT_MS));
	}
	else if(rb_full(&packet_rb))
	{
		/* Make room for the new entry */
		rb_pop(&packet_rb);

		hyperspace_pkt_cache_update();
	}

	return rb_push(&packet_rb, &cache);
}


/* pkt_cache_find *******************************************************************************//**
 * @brief		Checks if the packet has been received already.
 * @retval		true if the packet has been received recently.
 * @retval		false if this if the first time receiving the packet. */
static bool hyperspace_pkt_cache_find(
	struct net_ipv6_hdr* hdr,
	HyperOpt* opt,
	bool      fragmented,
	uint32_t  fragoffset)
{
	unsigned i;
	for(i = 0; i < rb_count(&packet_rb); i++)
	{
		const HyperCache* ptr = rb_entry(&packet_rb, i);

		if(net_ipv6_addr_cmp(&ptr->src, &hdr->src) &&
		   ptr->packet_id  == opt->packet_id &&
		   ptr->fragmented == fragmented &&
		   ptr->fragoffset == fragoffset)
		{
			LOG_DBG("\r\n"
				"\tpacket_id  %d, %d\r\n"
				"\tfragmented %d, %d\r\n"
				"\tfragoffset %d, %d",
				ptr->packet_id, opt->packet_id,
				ptr->fragmented, fragmented,
				ptr->fragoffset, fragoffset);

			return true;
		}
	}

	return false;
}


/* hyperspace_pkt_cache_timeout *****************************************************************//**
 * @brief		Packet cache timeout handler. Removes expired packet caches and sets the timeout for
 * 				the next timeout. */
static void hyperspace_pkt_cache_timeout(struct k_work* work)
{
	LOG_DBG("timeout: rb_count = %d", rb_count(&packet_rb));

	// int64_t now = k_uptime_get();	/* Uptime in ms */

	rb_pop(&packet_rb);

	hyperspace_pkt_cache_update();

	// while(!rb_empty(&packet_rb))
	// {
	// 	HyperCache* ptr       = rb_entry(&packet_rb, 0);
	// 	int64_t     remaining = ptr->timestamp + PACKET_CACHE_ENTRY_TIMEOUT_MS - now;

	// 	LOG_DBG("ptr->timestamp = %d", (uint32_t)ptr->timestamp);
	// 	LOG_DBG("remaining = %d", (int32_t)remaining);

	// 	if(remaining <= 0)
	// 	{
	// 		rb_pop(&packet_rb);
	// 	}
	// 	else
	// 	{
	// 		k_work_schedule(&packet_cache_work, K_MSEC(remaining));
	// 		break;
	// 	}
	// }

	LOG_DBG("done. rb_count = %d", rb_count(&packet_rb));
}


/* hyperspace_pkt_cache_update ******************************************************************//**
 * @brief		*/
static void hyperspace_pkt_cache_update(void)
{
	int64_t now = k_uptime_get();

	while(!rb_empty(&packet_rb))
	{
		HyperCache* ptr       = rb_entry(&packet_rb, 0);
		int64_t     remaining = ptr->timestamp + PACKET_CACHE_ENTRY_TIMEOUT_MS - now;

		LOG_DBG("ptr->timestamp = %d", (uint32_t)ptr->timestamp);
		LOG_DBG("remaining = %d", (int32_t)remaining);

		if(remaining <= 0)
		{
			rb_pop(&packet_rb);
		}
		else
		{
			k_work_schedule(&packet_cache_work, K_MSEC(remaining));
			break;
		}
	}
}




// ----------------------------------------------------------------------------------------------- //
// Hyperspace Routing Table                                                                        //
// ----------------------------------------------------------------------------------------------- //
/* hyperspace_route_alloc ***********************************************************************//**
 * @brief		Reserves a hyperspace routing entry for the specified address. */
static HyperRoute* hyperspace_route_alloc(struct in6_addr* addr, struct net_if* iface)
{
	k_mutex_lock(&hyperspace.route_mutex, K_FOREVER);

	HyperRoute* route = pool_reserve(&hyperroute_pool);

	if(route)
	{
		memmove(&route->addr, addr, sizeof(struct in6_addr));
		route->coord.r   = NAN;
		route->coord.t   = NAN;
		route->requests  = 0;
		route->last_used = k_uptime_get();
		route->iface     = iface;
		route->valid     = false;
	}

	k_mutex_unlock(&hyperspace.route_mutex);
	return route;
}


/* hyperspace_route_remove **********************************************************************//**
 * @brief		Removes the specified hyperspace routing entry. */
static void hyperspace_route_remove(HyperRoute* route)
{
	k_mutex_lock(&hyperspace.route_mutex, K_FOREVER);
	pool_release(&hyperroute_pool, route);
	k_mutex_unlock(&hyperspace.route_mutex);
}


/* hyperspace_route_clean ***********************************************************************//**
 * @brief		Removes old unused hyperspace routing entries from the routing table. */
static void hyperspace_route_clean(void)
{
	k_mutex_lock(&hyperspace.route_mutex, K_FOREVER);

	/* Remove all routes older than the specified maximum life time */
	int64_t current = k_uptime_get();

	unsigned i;
	for(i = 0; i < NUM_HYPERROUTES; i++)
	{
		if(pool_idx_is_reserved(&hyperroute_pool, i))
		{
			HyperRoute* ptr = pool_entry(&hyperroute_pool, i);

			if(current - ptr->last_used >= HYPER_ROUTE_TIMEOUT_MS)
			{
				LOG_DBG("clean %p", ptr);
				hyperspace_route_remove(ptr);
			}
		}
	}

	k_mutex_unlock(&hyperspace.route_mutex);
}


/* hyperspace_route_find ************************************************************************//**
 * @brief		Attempts to find a hyperspace route to the specified address. */
static HyperRoute* hyperspace_route_find(struct in6_addr* addr)
{
	k_mutex_lock(&hyperspace.route_mutex, K_FOREVER);

	unsigned i;

	for(i = 0; i < NUM_HYPERROUTES; i++)
	{
		if(pool_idx_is_reserved(&hyperroute_pool, i))
		{
			HyperRoute* ptr = pool_entry(&hyperroute_pool, i);

			if(memcmp(addr, &ptr->addr, sizeof(struct in6_addr)) == 0)
			{
				k_mutex_unlock(&hyperspace.route_mutex);
				return ptr;
			}
		}
	}

	k_mutex_unlock(&hyperspace.route_mutex);
	return 0;
}


/* hyperspace_dist ******************************************************************************//**
 * @brief		Computes the hyperbolic distance between two hyperbolic coordinates. */
static float hyperspace_dist(float r1, float t1, float r2, float t2)
{
	return acosh(cosh(r1)*cosh(r2) - sinh(r1)*sinh(r2)*cos(t2 - t1));
}


/* hyperspace_translate *************************************************************************//**
 * @brief		Translates the vector v = [r, theta] 'a' units in the 't0' angle. */
static void hyperspace_translate(double v[2], double a, double t0)
{
	if(a == 0)
	{
		return;
	}

	/* 	Translate 'a' units along the x axis
	 *
	 * 		    | cosh(a) 0 sinh(a) |
	 * 		M = | 0       1 0       |
	 * 		    | sinh(a) 0 cosh(a) |
	 *
	 * 	Parameter conversion between (r, theta) and (x,y,z)
	 *
	 * 		    | sinh(r)*cos(theta) | (x)
	 * 		v = | sinh(r)*sin(theta) | (y)
	 * 		    | cosh(r)            | (z)
	 *
	 * 		r = acosh(z)
	 * 		t = atan(y/x)
	 *
	 * 	Translation and conversion:
	 *
	 * 		        | cosh(a)*sinh(r)*cos(theta) + sinh(a)*cosh(r) |
	 * 		M * v = | sinh(r)*sin(theta)                           |
	 * 		        | sinh(a)*sinh(r)*cos(theta) + cosh(a)*cosh(r) |
	 *
	 * 		r = acosh(sinh(a)*sinh(r)*cos(theta) + cosh(a)*cosh(r))
	 * 		t = atan(sinh(r)*sin(theta) / cosh(a)*sinh(r)*cos(theta) + sinh(a)*cosh(r))
	 */
	double r = acosh(sinh(a) * sinh(v[0]) * cos(v[1] - t0) + cosh(a) * cosh(v[0]));
	double y = sinh(v[0]) * sin(v[1] - t0);
	double x = cosh(a) * sinh(v[0]) * cos(v[1] - t0) + sinh(a) * cosh(v[0]);
	double theta = calc_mapd_2pi(atan2(y, x) + t0);

	v[0] = r;
	v[1] = theta;
}


/******************************************* END OF FILE *******************************************/
