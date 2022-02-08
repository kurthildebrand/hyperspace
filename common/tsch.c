/************************************************************************************************//**
 * @file		tsch.c
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
#include "logging/log.h"
// LOG_MODULE_REGISTER(tsch, LOG_LEVEL_ERR);
LOG_MODULE_REGISTER(tsch, LOG_LEVEL_INF);
// LOG_MODULE_REGISTER(tsch, LOG_LEVEL_DBG);

#include <net/net_pkt.h>
#include <net/net_core.h>
#include <net/net_l2.h>
#include <net/net_if.h>
#include <ipv6.h>
#include <icmpv6.h>
#include <nrfx/hal/nrf_ppi.h>
#include <nrfx/hal/nrf_gpiote.h>
#include <nrfx/hal/nrf_timer.h>
#include <random/rand32.h>
#include <stdlib.h>
#include <zephyr.h>

#include "bayesian.h"
#include "calc.h"
#include "config.h"
#include "dw1000.h"
#include "hyperspace.h"
#include "ieee_802_15_4.h"
#include "location.h"
#include "lowpan.h"
#include "net_private.h"
#include "pool.h"
#include "timeslot.h"
#include "tsch.h"


/* Inline Function Instances --------------------------------------------------------------------- */
/* Private Macros -------------------------------------------------------------------------------- */
// #define TSCH_NUM_FRAMES             (8)
#define TSCH_NUM_FRAMES             (16)
#define TSCH_ADV_RETRANS_TIMEOUT    (5000)	/* The time in ms between router advertisements */
#define TSCH_SYNC_LOST_TIMEOUT      (5000)	/* The time in ms before time sync is lost */

#define TSCH_SF_PRIO_0              (0)
#define TSCH_SF_SCAN                (10)

#define TSCH_TX_OFFSET_US           (1000)
#define TSCH_RX_OFFSET_US           (500)
#define TSCH_RX_TIMEOUT_US          (1000)
#define TSCH_TX_ACK_OFFSET_US       (1700)
#define TSCH_RX_ACK_OFFSET_US       (1550)
#define TSCH_RX_ACK_TIMEOUT_US      (300)


/* Private Types --------------------------------------------------------------------------------- */
typedef enum {
	TSCH_IDLE_STATE,
	TSCH_SCANNING_STATE,
	TSCH_SYNCED_STATE,
	TSCH_CONNECTING_STATE,
	TSCH_CONNECTED_STATE,
	TSCH_RECONNECTING_STATE,
	TSCH_DISCONNECTED_STATE,
} Tsch_State;

typedef enum {
	TSCH_TIMEOUT_EVENT,
	TSCH_START_NETWORK_EVENT,
	TSCH_START_SCAN_EVENT,
	TSCH_STOP_SCAN_EVENT,
	TSCH_SYNC_EVENT,
	TSCH_CONNECT_EVENT,
	TSCH_DISCONNECT_EVENT,
} Tsch_Event;


/* Private Functions ----------------------------------------------------------------------------- */
static int               tsch_dev_init (const struct device*);
static void              tsch_if_init  (struct net_if*);
static enum net_verdict  tsch_if_recv  (struct net_if*, struct net_pkt*);
static int               tsch_if_send  (struct net_if*, struct net_pkt*);
static int               tsch_if_enable(struct net_if*, bool);
static enum net_l2_flags tsch_if_flags (struct net_if*);
static int               tsch_tx_pkt   (struct net_pkt*, TsSlot*, k_timeout_t, bool);

static void              tsch_handle_prefix    (struct net_mgmt_event_callback*, uint32_t, struct net_if*);
static void              tsch_send_ra_timeout  (struct k_work*);
static enum net_verdict  handle_rs_input       (struct net_pkt*, struct net_ipv6_hdr*, struct net_icmp_hdr*);
static void              tsch_handle_timeout   (struct k_work*);
static void              tsch_handle_sync_lost (struct k_work*);
static void              tsch_handle_do_nothing(struct k_work*);

static void     tsch_handle_event  (Tsch_Event);

static void     tsch_scan_slot     (TsSlot*);
static void     tsch_adv_slot      (TsSlot*);
static void     tsch_shared_slot   (TsSlot*);
static void     tsch_shared_adv    (TsSlot*, uint64_t, uint64_t);
static void     tsch_shared_tx     (TsSlot*, uint64_t, uint64_t);
static void     tsch_shared_rx     (TsSlot*, uint64_t, uint64_t);

static int32_t  tsch_radio_start_tx(Ieee154_Frame*, uint64_t);
static void     tsch_radio_wait_tx (uint32_t*);
static int32_t  tsch_radio_rx      (Ieee154_Frame*, uint64_t, uint32_t, uint32_t*);

static void     tsch_handle_rx     (TsSlot*, Ieee154_Frame*);
static void     tsch_handle_rx_data(TsSlot*, Ieee154_Frame*);
static void     tsch_handle_ack    (TsSlot*, Ieee154_Frame*, Ieee154_Frame*);
static bool     tsch_valid_addr    (TsSlot*, const Ieee154_Frame*);

static Ieee154_Frame* tsch_reserve_frame(void);
static void           tsch_release_frame(Ieee154_Frame*);


/* Private Variables ----------------------------------------------------------------------------- */
static struct net_icmpv6_handler rs_input_handler = {
	.type = NET_ICMPV6_RS, .code = 0, .handler = handle_rs_input,
};

static tsch_api tsch_if_api = {
	.iface_api.init = tsch_if_init,
};

NET_L2_INIT(TSCH_L2, tsch_if_recv, tsch_if_send, tsch_if_enable, tsch_if_flags);

NET_DEVICE_INIT(
	net_tsch,                            /* dev_name */
	"TSCH UWB Radio",                    /* drv_name */
	tsch_dev_init,                       /* init_fn */
	0,                                   /* pm_control_fn */
	0,                                   /* data */
	0,                                   /* cfg_info */
	CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, /* prio */
	&tsch_if_api,                        /* api */
	TSCH_L2,                             /* l2 */
	void*,                               /* l2_ctx_type */
	125);                                /* mtu */

static struct net_if* tsch_iface;	/* TODO: utilize device struct */
uint8_t       tsch_frame_data[TSCH_NUM_FRAMES][IEEE154_STD_PACKET_LENGTH];
// uint8_t       tsch_frame_data[TSCH_NUM_FRAMES][255];
Ieee154_Frame tsch_frames[TSCH_NUM_FRAMES];
Pool          tsch_frame_pool;
uint8_t       tsch_adv_frame_data[IEEE154_STD_PACKET_LENGTH];
Ieee154_Frame tsch_adv_frame;

DW1000 dw;
DW1000_Config dwcfg = {
	.channel         = 5,
	.data_rate       = DW1000_DR_6800KBPS,
	.prf             = DW1000_PRF_64MHZ,
	.pac             = DW1000_PAC_SIZE_8,
	.preamble_length = DW1000_PLEN_64,
	.tx_code         = 10,
	.rx_code         = 10,
	.sfd_timeout     = (64 + 1 + 8 - 8),
};

Tsch tsch;


// ----------------------------------------------------------------------------------------------- //
// TSCH IF                                                                                         //
// ----------------------------------------------------------------------------------------------- //
static int tsch_dev_init(const struct device* dev)
{
	return 0;
}


static void tsch_if_init(struct net_if* iface)
{
	tsch_iface = iface;
	net_if_flag_set(iface, NET_IF_HYPERSPACE);
	net_if_flag_set(iface, NET_IF_NO_AUTO_START);

	hyperspace_init();
	lowpan_ctx_init();

	// const struct device* dev = net_if_get_device(iface);

	memset(tsch.bcast, 0xFF, 8);
	memmove(tsch.addr, (const void*)&NRF_FICR->DEVICEID, 8);
	net_if_set_link_addr(iface, tsch.addr, sizeof(tsch.addr), NET_LINK_IEEE802154);
	LOG_DBG("%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
		tsch.addr[0], tsch.addr[1], tsch.addr[2], tsch.addr[3],
		tsch.addr[4], tsch.addr[5], tsch.addr[6], tsch.addr[7]);

	loc_init(&dw, tsch.addr);

	/* Configure interface with link local address */
	struct net_if_addr* ifaddr;
	struct net_if_ipv6* ipv6;
	struct in6_addr     addr = { 0 };

	if(net_if_config_ipv6_get(iface, &ipv6) < 0) {
		LOG_ERR("cannot assign link local addr: IPv6 config not valid");
		return;
	}

	if(!ipv6) {
		return;
	}

	net_ipv6_addr_create_iid(&addr, net_if_get_link_addr(iface));

	ifaddr = net_if_ipv6_addr_add(iface, &addr, NET_ADDR_AUTOCONF, 0);
	if(!ifaddr) {
		LOG_ERR("cannot add link local address to tsch");
	}

	// LOG_DBG("tsch interface initialized");
	// net_if_flag_set(iface, NET_IF_HYPERSPACE);
	// net_if_flag_set(iface, NET_IF_NO_AUTO_START);

	tsch_init();
}


static enum net_verdict tsch_if_recv(struct net_if* iface, struct net_pkt* pkt)
{
	return NET_CONTINUE;
}


static int tsch_if_send(struct net_if* iface, struct net_pkt* pkt)
{
	LOG_INF("tsch if send: %p", pkt);

	/* Leave the lladdr_dest null as tsch_tx_pkt will use part of the IPv6 dest address as the
	 * link-layer dest address, which is kind of backwards to how it's supposed to work. The proper
	 * way to get a lladdr assigned to an IPv6 address is via ICMPv6 NDP. However, Zephyr's
	 * networking stack is so painful to use that I don't want to figure it out. Also, how to get
	 * ICMPv6 NDP to work with hyperspace routing?
	 *
	 * In any case, just using part of the IPv6 dest address as the IEEE 802.15.4 dest address will
	 * work fine as long as nodes check a frame's dest address against IPv6 addresses assigned to
	 * them. */
	// // net_pkt_lladdr_dst(pkt)->type = NET_LINK_IEEE802154;
	// // net_pkt_lladdr_dst(pkt)->addr = tsch.bcast;
	// // net_pkt_lladdr_dst(pkt)->len  = 8;

	// if(!net_pkt_lladdr_dst(pkt)->addr)
	// {
	// 	net_pkt_lladdr_dst(pkt)->type = NET_LINK_IEEE802154;
	// 	net_pkt_lladdr_dst(pkt)->addr = tsch.bcast;
	// 	net_pkt_lladdr_dst(pkt)->len  = 8;
	// }

	TsSlotframe* sf   = ts_slotframe_find(TSCH_SF_PRIO_0);
	TsSlot*      slot = ts_slot_find(sf, 1);
	if(tsch_tx_pkt(pkt, slot, K_MSEC(20000), false) != 0)
	{
		LOG_ERR("fail transmitting packet");
		return 0;
	}

	size_t len = net_pkt_get_len(pkt);
	net_pkt_unref(pkt);
	return len;
}


static int tsch_tx_pkt(struct net_pkt* pkt, TsSlot* slot, k_timeout_t timeout, bool beacon)
{
	if(!pkt)
	{
		LOG_WRN("invalid pkt");
		return 0;
	}

	if(!slot)
	{
		LOG_WRN("invalid slot");
		return 0;
	}

	LOG_DBG("start");

	NET_PKT_DATA_ACCESS_CONTIGUOUS_DEFINE(ipv6_access, struct net_ipv6_hdr);
	struct net_pkt_cursor cursor;

	net_pkt_cursor_backup(pkt, &cursor);
	net_pkt_cursor_init(pkt);
	struct net_ipv6_hdr* hdr = net_pkt_get_data(pkt, &ipv6_access);
	net_pkt_cursor_restore(pkt, &cursor);

	Ieee154_Frame* frame = 0;
	unsigned sent = 0;
	uint8_t frags_bitmap[1280/64] = { 0 };
	Bits frags = make_bits(frags_bitmap, (net_pkt_get_len(pkt) + 7) / 8);
	uint32_t fragid = sys_rand32_get();

	// if(net_pkt_lladdr_dst(pkt)->addr != 0)
	// {
	// 	uint8_t* dest = net_pkt_lladdr_dst(pkt)->addr;
	// 	LOG_DBG("pkt lladdr dst = %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
	// 		dest[0], dest[1], dest[2], dest[3], dest[4], dest[5], dest[6], dest[7]);
	// }
	// else
	// {
	// 	LOG_DBG("pkt lladdr dst = 0");
	// }

	do {
		frame = tsch_reserve_frame();
		if(!frame)
		{
			LOG_ERR("fail allocating frame");
			goto error;
		}

		if(beacon)
		{
			ieee154_beacon_frame_init(frame, ieee154_ptr_start(frame), ieee154_size(frame));
		}
		else
		{
			ieee154_data_frame_init(frame, ieee154_ptr_start(frame), ieee154_size(frame));
		}

		ieee154_set_seqnum(frame, tsch.dsn++);

		if(net_pkt_lladdr_dst(pkt)->addr == 0)
		{
			ieee154_set_addr(frame, 0, &hdr->dst.s6_addr[8], 8, 0, tsch.addr, 8);

			LOG_DBG("dest addr = %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
				hdr->dst.s6_addr[8],  hdr->dst.s6_addr[9],
				hdr->dst.s6_addr[10], hdr->dst.s6_addr[11],
				hdr->dst.s6_addr[12], hdr->dst.s6_addr[13],
				hdr->dst.s6_addr[14], hdr->dst.s6_addr[15]);
		}
		else if(net_ipv6_is_addr_mcast(&hdr->dst))
		{
			ieee154_set_addr(frame, 0, tsch.bcast, 8, 0, tsch.addr, 8);
		}
		else
		{
			ieee154_set_addr(frame, 0, net_pkt_lladdr_dst(pkt)->addr, 8, 0, tsch.addr, 8);
		}

		sent = lowpan_compress(pkt, &frags, fragid, frame);

		if(!sent)
		{
			LOG_ERR("fail compressing frame");
			break;
		}
		else
		{
			LOG_INF("frame %p sent %d of %d", frame, sent, net_pkt_get_len(pkt));
			k_queue_append(&slot->tx_queue, frame);
		}
	} while(sent < net_pkt_get_len(pkt));

	LOG_DBG("done");

	return 0;

	error:
		LOG_DBG("error");
		tsch_release_frame(frame);
		return -1;
}


static int tsch_if_enable(struct net_if* iface, bool enable)
{
	return 0;
}


static enum net_l2_flags tsch_if_flags(struct net_if* iface)
{
	return NET_L2_MULTICAST;
}




// ----------------------------------------------------------------------------------------------- //
// TSCH                                                                                            //
// ----------------------------------------------------------------------------------------------- //
/* tsch_bcast_addr ******************************************************************************//**
 * @brief		Returns a pointer to the broadcast link-layer address. */
void* tsch_bcast_addr(void)
{
	return tsch.bcast;
}


/* tsch_init ************************************************************************************//**
 * @brief		*/
void tsch_init(void)
{
	pool_init(&tsch_frame_pool, tsch_frames, TSCH_NUM_FRAMES, sizeof(tsch_frames[0]));

	/* Initialize Tsch struct */
	sys_rand_get(&tsch.dsn, sizeof(tsch.dsn));
	sys_rand_get(&tsch.ebsn, sizeof(tsch.ebsn));
	tsch.state             = TSCH_IDLE_STATE;
	tsch.next_state        = TSCH_IDLE_STATE;
	tsch.shared_cell_state = TSCH_CELL_IDLE_STATE;

	// backoff_init(&tsch.backoff, 2, 32);
	bayes_init(&tsch.bayes_bcast, 10.0f);

	net_mgmt_init_event_callback(&tsch.prefix_cb, tsch_handle_prefix, NET_EVENT_IPV6_PREFIX_ADD);
	net_mgmt_add_event_callback(&tsch.prefix_cb);
	net_icmpv6_register_handler(&rs_input_handler);

	k_work_init_delayable(&tsch.timeout_work, tsch_handle_timeout);
	k_work_init_delayable(&tsch.ra_work, tsch_send_ra_timeout);

	/* Capture DW1000 interrupt to timer 0 */
	/* GPIOTE CONFIG[0]: Generate event on GPIO DW1000 IRQ */
	nrf_gpiote_event_configure(NRF_GPIOTE, 0, DW1000_IRQ_PIN, NRF_GPIOTE_POLARITY_LOTOHI);
	nrf_gpiote_event_enable(NRF_GPIOTE, 0);

	ts_init();

	/* Initialize DW1000 */
	dw1000_init            (&dw, DW1000_IRQ_PIN);
	dw1000_reconfig        (&dw, &dwcfg);
	dw1000_set_tx_ant_delay(&dw, 0);
	dw1000_set_rx_ant_delay(&dw, 0);

	dw1000_int_enable(&dw,
		DW1000_SYS_MASK_MTXFRS  | DW1000_SYS_MASK_MRXSFDD  | DW1000_SYS_MASK_MRXFCG   |
		DW1000_SYS_MASK_MRXRFTO | DW1000_SYS_MASK_MRXPTO   | DW1000_SYS_MASK_MRXPHE   |
		DW1000_SYS_MASK_MRXFCE  | DW1000_SYS_MASK_MRXRFSL  | DW1000_SYS_MASK_MRXSFDTO |
		DW1000_SYS_MASK_MAFFREJ | DW1000_SYS_MASK_MLDEERR  | DW1000_SYS_MASK_MHPDWARN);

	dw1000_set_drxb(&dw, true);

	dw1000_config_sleep(&dw,
		DW1000_AON_WCFG_PRES_SLEEP | DW1000_AON_WCFG_ONW_LLD0 | DW1000_AON_WCFG_ONW_LDC,
		DW1000_AON_CFG0_WAKE_SPI);

	dw1000_enter_sleep(&dw);

	// /* Debugging power issues. Immediately wake up the DW1000. */
	// dw1000_wakeup_by_cs(&dw);
	// usleep(2000);

	/* Setup PPI to capture DW1000 interrupt time. Also, automatically disable interrupt time capture
	 * so that multiple quick DW1000 interrupts are ignored.
	 * Ch 13: DW1000 Interrupt          -+--> TIMER0->TASKS_CAPTURE[3]
	 *                                    \-> NRF_PPI->TASKS_CHG[NRF_PPI_CHANNEL_GROUP2].DIS */
	nrf_ppi_channel_and_fork_endpoint_setup(
		NRF_PPI, NRF_PPI_CHANNEL13,
		nrf_gpiote_event_address_get(NRF_GPIOTE, NRF_GPIOTE_EVENT_IN_0),
		nrf_timer_task_address_get(NRF_TIMER0, NRF_TIMER_TASK_CAPTURE3),
		nrf_ppi_task_group_disable_address_get(NRF_PPI, NRF_PPI_CHANNEL_GROUP2));

	nrf_ppi_channel_include_in_group(NRF_PPI, NRF_PPI_CHANNEL13, NRF_PPI_CHANNEL_GROUP2);
	nrf_ppi_channel_enable(NRF_PPI, NRF_PPI_CHANNEL13);

	// volatile uint32_t id = dw1000_read_dev_id(&dw);
	// __asm("nop");
	// LOG_DBG("DW1000 = %x", id);

	// NRF_P0->PIN_CNF[6] =
	// 	(GPIO_PIN_CNF_DIR_Output       << GPIO_PIN_CNF_DIR_Pos)   |
	// 	(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
	// 	(GPIO_PIN_CNF_PULL_Disabled    << GPIO_PIN_CNF_PULL_Pos)  |
	// 	(GPIO_PIN_CNF_DRIVE_S0S1       << GPIO_PIN_CNF_DRIVE_Pos) |
	// 	(GPIO_PIN_CNF_SENSE_Disabled   << GPIO_PIN_CNF_SENSE_Pos);
}


/* tsch_enable **********************************************************************************//**
 * @brief		*/
void tsch_enable(void)
{
	ts_init();
	ts_config_power_down(tsch_power_down, 100);
	ts_config_power_up(tsch_power_up, 2200);
}


/* tsch_handle_prefix ***************************************************************************//**
 * @brief		*/
static void tsch_handle_prefix(
	struct net_mgmt_event_callback* cb,
	uint32_t nm_event,
	struct net_if* iface)
{
	LOG_DBG("handle prefix");

	if(nm_event == NET_EVENT_IPV6_PREFIX_ADD && iface != tsch_iface)
	{
		LOG_INF("Add prefix to TSCH");

		/* Note: this is a modification to the Zephyr kernel: net_if_ipv6_prefix_add change
		 *
		 * 		net_mgmt_event_notify_with_info(
		 *			NET_EVENT_IPV6_PREFIX_ADD, iface,
		 *			&ipv6->prefix[i].prefix, sizeof(struct in6_addr));
		 *
		 * to this
		 *
		 * 		// net_mgmt_event_notify_with_info(
		 * 		// 	NET_EVENT_IPV6_PREFIX_ADD, iface,
		 * 		// 	&ipv6->prefix[i], sizeof(struct net_if_ipv6_prefix));
		 *
		 * 		void* ptr = &ipv6->prefix[i];
		 *
		 * 		net_mgmt_event_notify_with_info(NET_EVENT_IPV6_PREFIX_ADD, iface, &ptr, sizeof(ptr));
		 */
		unsigned i;
		struct net_if_ipv6_prefix* ptr;
		memmove(&ptr, cb->info, sizeof(ptr));
		uint32_t lifetime = NET_IPV6_ND_INFINITE_LIFETIME;

		lowpan_ctx_put(1, &ptr->prefix);

		/* TODO: how to get actual lifetime? For now, just set infinite lifetime. */
		/* Note: can't add network prefix in net_mgmt handler because of Zephyr's borked net_mgmt
		 * locking. */
		// net_if_ipv6_prefix_add(tsch_iface, &ptr->prefix, ptr->len, NET_IPV6_ND_INFINITE_LIFETIME);
		// k_delayed_work_submit(&tsch_ra_timeout, TSCH_ADV_RETRANS_TIMEOUT);

		// struct net_if_ipv6_prefix* ifprefix;
		struct net_if_ipv6* ipv6;

		if(net_if_config_ipv6_get(tsch_iface, &ipv6) < 0) {
			return;	/* No free prefixes */
		}

		// ifprefix = ipv6_prefix_find(iface, &ptr->prefix, ptr->len);
		// if(ifprefix) {
		// 	return;	/* Prefix is already added */
		// }

		/* Inline ipv6_prefix_find because it is a static function and not accessible here. Also,
		 * it might have a bug as it checks !ipv6->unicast[i].is_used instead of
		 * !ipv6->prefix[i].is_used. */
		for(i = 0; i < NET_IF_MAX_IPV6_PREFIX; i++) {
			if(tsch_iface->config.ip.ipv6->prefix[i].is_used &&
			   net_ipv6_addr_cmp(&ptr->prefix, &tsch_iface->config.ip.ipv6->prefix[i].prefix) &&
			   ptr->len == tsch_iface->config.ip.ipv6->prefix[i].len) {
				return; /* Prefix is already added */
			}
		}

		if(!ipv6) {
			return;
		}

		for(i = 0; i < NET_IF_MAX_IPV6_PREFIX; i++) {
			if(!ipv6->prefix[i].is_used) {
				/* Inline net_if_ipv6_prefix_init because it is a static function and not accesible
				 * here. Can't call net_if_ipv6_prefix_add because it calls
				 * net_mgmt_event_notify_with_info which pends on a lock which is already held
				 * resulting in the lock never being released and locking the net_mgmt event loop. */
				ipv6->prefix[i].is_used = true;
				ipv6->prefix[i].len     = ptr->len;
				ipv6->prefix[i].iface   = tsch_iface;
				net_ipaddr_copy(&ipv6->prefix[i].prefix, &ptr->prefix);

				if(lifetime == NET_IPV6_ND_INFINITE_LIFETIME) {
					ipv6->prefix[i].is_infinite = true;
				} else {
					ipv6->prefix[i].is_infinite = false;
				}

				break;
			}
		}

		k_work_schedule(&tsch.ra_work, K_MSEC(TSCH_ADV_RETRANS_TIMEOUT));
	}

	if(nm_event == NET_EVENT_IPV6_PREFIX_ADD && iface == tsch_iface)
	{
		struct net_if_ipv6_prefix* ptr;
		memmove(&ptr, cb->info, sizeof(ptr));

		lowpan_ctx_put(1, &ptr->prefix);
	}

	/* TODO: handle NET_EVENT_IPV6_PREFIX_DEL from spis_if */
}


/* tsch_send_ra *********************************************************************************//**
 * @brief		*/
static void tsch_send_ra_timeout(struct k_work* work)
{
	LOG_DBG("timeout");

	/* Send RA */
	k_work_schedule(&tsch.ra_work, K_MSEC(TSCH_ADV_RETRANS_TIMEOUT));

	struct net_pkt* ra = net_pkt_alloc_with_buffer(tsch_iface,
		sizeof(struct net_icmpv6_ra_hdr) + 64,
		AF_INET6,
		IPPROTO_ICMPV6,
		K_MSEC(100));

	// net_pkt_print();
	if(!ra)
	{
		LOG_DBG("drop");
		goto drop;
	}

	net_pkt_set_ipv6_hop_limit(ra, NET_IPV6_ND_HOP_LIMIT);

	/* Dest: all nodes multicast address. Src: link local address */
	/* TODO: need to assign link local address to tsch iface */
	const struct in6_addr dest = {{{
		0xFF, 0x02, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x01 }}};

	const struct in6_addr* src = net_if_ipv6_get_ll(tsch_iface, NET_ADDR_ANY_STATE);
	if(!src) {
		LOG_WRN("no link local address from which to send RA");
		goto drop;
	}

	if(net_ipv6_create(ra, src, &dest) || net_icmpv6_create(ra, NET_ICMPV6_RA, 0))
	{
		goto drop;
	}

	NET_PKT_DATA_ACCESS_CONTIGUOUS_DEFINE(ra_access, struct net_icmpv6_ra_hdr);
	struct net_icmpv6_ra_hdr* ra_hdr = net_pkt_get_data(ra, &ra_access);
	if(!ra_hdr)
	{
		goto drop;
	}

	/* Clear reserved fields */
	memset(&ra_hdr->flags, 0, sizeof(ra_hdr->flags));

	/* Fill in RA header */
	ra_hdr->cur_hop_limit   = 0;
	ra_hdr->flags           = 0;
	ra_hdr->router_lifetime = htons(300);		/* Router lifetime in seconds */
	ra_hdr->reachable_time  = htons(5*60*1000);	/* Reachable for 5 min */
	ra_hdr->retrans_timer   = htons(TSCH_ADV_RETRANS_TIMEOUT);

	if(net_pkt_set_data(ra, &ra_access))
	{
		goto drop;
	}

	// /* Set Link-Layer Address Option */
	// net_pkt_write_u8(ra, NET_ICMPV6_ND_OPT_SLLAO);          /* Set LLAO type */
	// net_pkt_write_u8(ra, 2);                                /* Set LLAO length */
	// net_pkt_write   (ra, net_if_get_link_addr(tsch_iface)); /* Set link layer address (8 bytes) */
	// net_pkt_write_u8(ra, 0);                                /* Write padding bytes */
	// net_pkt_write_u8(ra, 0);
	// net_pkt_write_u8(ra, 0);
	// net_pkt_write_u8(ra, 0);
	// net_pkt_write_u8(ra, 0);
	// net_pkt_write_u8(ra, 0);

	/* Set Prefix Information Option */
	struct net_if_ipv6_prefix* prefix = 0;
	unsigned i;
	for(i = 0; i < NET_IF_MAX_IPV6_PREFIX; i++) {
		prefix = &tsch_iface->config.ip.ipv6->prefix[i];
		if(prefix->is_used) {
			break;
		}
	}

	if(!prefix)
	{
		LOG_DBG("no prefix info");
		goto drop;
	}

	net_pkt_write_u8  (ra, NET_ICMPV6_ND_OPT_PREFIX_INFO);  /* Append PIO Type */
	net_pkt_write_u8  (ra, 4);                              /* Append PIO Length */
	net_pkt_write_u8  (ra, prefix->len);                    /* Append prefix length */
	net_pkt_write_u8  (ra, 0xC0);                           /* Append L and A flags */
	net_pkt_write_be32(ra, NET_IPV6_ND_INFINITE_LIFETIME);  /* Append valid lifetime */
	net_pkt_write_be32(ra, NET_IPV6_ND_INFINITE_LIFETIME);  /* Append preferred lifetime */
	net_pkt_write_be32(ra, 0);                              /* Append reserved bits */
	net_pkt_write     (ra, prefix->prefix.s6_addr, 16);     /* Append prefix */

	net_pkt_cursor_init(ra);
	net_ipv6_finalize(ra, IPPROTO_ICMPV6);

	/* Set pkt lladdr */
	net_pkt_lladdr_src(ra)->addr = net_pkt_lladdr_if(ra)->addr;
	net_pkt_lladdr_src(ra)->type = net_pkt_lladdr_if(ra)->type;
	net_pkt_lladdr_src(ra)->len  = net_pkt_lladdr_if(ra)->len;

	net_pkt_lladdr_dst(ra)->addr = tsch.bcast;
	net_pkt_lladdr_dst(ra)->type = NET_LINK_IEEE802154;
	net_pkt_lladdr_dst(ra)->len  = 8;

	net_pkt_hexdump(ra, "ra");

	/* Queue RA via shared slot */
	TsSlotframe* sf   = ts_slotframe_find(TSCH_SF_PRIO_0);
	TsSlot*      slot = ts_slot_find(sf, 1);

	if(tsch_tx_pkt(ra, slot, K_MSEC(20000), true) != 0)
	{
		NET_DBG("DROP: Cannot send RA");
		goto drop;
	}

	LOG_DBG("done");

	drop:
		if(ra)
		{
			net_pkt_unref(ra);
		}
}


/* tsch_handle_rs_input *************************************************************************//**
 * @brief		*/
static enum net_verdict handle_rs_input(
	struct net_pkt* rs,
	struct net_ipv6_hdr* ip_hdr,
	struct net_icmp_hdr* icmp_hdr)
{
	/* Validate Router Solicitation Message:
	 * 1.	Hosts MUST silently discard any received Router Solicitation Messages.
	 * 2.	Discard if the IP Hop Limit field does not have a value of 255, i.e., the packet could
	 * 		not possibly have been forwarded by a router.
	 * 3.	Discard if ICMP Checksum is not valid.
	 * 4.	Discard if ICMP code is not 0.
	 * 5.	Discard if ICMP length (derived from the IP length) is less than 8.
	 * 6.	Discard if all included options have a lengths equal to zero.
	 * 7.	Discard if the IP source address is the unspecified address and there is no source
	 * 		link-layer address option in the message. */

	/* Create RA */

	/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * | Type (134)    | Code (0)      |          Checksum             | ICMP Router Advertisement
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * | Cur Hop Limit |M|O|  Reserved |       Router Lifetime         |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |                         Reachable Time                        |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |                          Retrans Timer                        |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |     Type      |    Length     |    Link-Layer Address ...     | Src Link-layer Address
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ Option
	 * |                                                               |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |     Type      |    Length     | Prefix Length |L|A| Reserved1 | Prefix Information Option
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |                         Valid Lifetime                        |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |                       Preferred Lifetime                      |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |                           Reserved2                           |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |                                                               |
	 * +                                                               +
	 * |                                                               |
	 * +                            Prefix                             +
	 * |                                                               |
	 * +                                                               +
	 * |                                                               |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
	// NET_PKT_DATA_ACCESS_CONTIGUOUS_DEFINE(rs_access, struct net_icmpv6_rs_hdr);
	// struct net_icmpv6_rs_hdr* rs_hdr = net_pkt_get_data(rs, &rs_access);
	struct net_pkt* ra = net_pkt_alloc_with_buffer(tsch_iface,
		sizeof(struct net_icmpv6_ra_hdr) + 64,
		AF_INET6,
		IPPROTO_ICMPV6,
		K_MSEC(100));

	// net_pkt_print();
	if(!ra)
	{
		goto drop; // -ENOMEM
	}

	net_pkt_set_ipv6_hop_limit(ra, NET_IPV6_ND_HOP_LIMIT);

	const struct in6_addr* dest = &ip_hdr->src;
	const struct in6_addr* src  = net_if_ipv6_select_src_addr(tsch_iface, dest);
	if (!src) {
		// LOG_DBG("DROP: No interface address for dst %s iface %p",
		// 	log_strdup(net_sprint_ipv6_addr(&ip_hdr->src)), net_pkt_iface(ra));
		goto drop;
	}

	if(net_ipv6_create(ra, src, dest) || net_icmpv6_create(ra, NET_ICMPV6_RA, 0))
	{
		goto drop;
	}

	NET_PKT_DATA_ACCESS_CONTIGUOUS_DEFINE(ra_access, struct net_icmpv6_ra_hdr);
	struct net_icmpv6_ra_hdr* ra_hdr = net_pkt_get_data(ra, &ra_access);
	if(!ra_hdr)
	{
		goto drop;
	}

	/* Clear reserved fields */
	memset(&ra_hdr->flags, 0, sizeof(ra_hdr->flags));

	/* Fill in RA header */
	ra_hdr->cur_hop_limit   = 0;
	ra_hdr->flags           = 0;
	ra_hdr->router_lifetime = htons(300);		/* Router lifetime in seconds */
	ra_hdr->reachable_time  = htons(5*60*1000);	/* Reachable for 5 min */
	ra_hdr->retrans_timer   = htons(TSCH_ADV_RETRANS_TIMEOUT);

	if(net_pkt_set_data(ra, &ra_access))
	{
		goto drop;
	}

	// /* Set Link-Layer Address Option */
	// net_pkt_write_u8(ra, NET_ICMPV6_ND_OPT_SLLAO);          /* Set LLAO type */
	// net_pkt_write_u8(ra, 2);                                /* Set LLAO length */
	// net_pkt_write   (ra, net_if_get_link_addr(tsch_iface)); /* Set link layer address (8 bytes) */
	// net_pkt_write_u8(ra, 0);                                /* Write padding bytes */
	// net_pkt_write_u8(ra, 0);
	// net_pkt_write_u8(ra, 0);
	// net_pkt_write_u8(ra, 0);
	// net_pkt_write_u8(ra, 0);
	// net_pkt_write_u8(ra, 0);

	/* Set Prefix Information Option */
	struct net_if_ipv6_prefix* prefix = 0;
	unsigned i;
	for(i = 0; i < NET_IF_MAX_IPV6_PREFIX; i++) {
		prefix = &tsch_iface->config.ip.ipv6->prefix[i];
		if(prefix->is_used) {
			break;
		}
	}

	net_pkt_write_u8  (ra, NET_ICMPV6_ND_OPT_PREFIX_INFO);  /* Append PIO Type */
	net_pkt_write_u8  (ra, 4);                              /* Append PIO Length */
	net_pkt_write_u8  (ra, prefix->len);                    /* Append prefix length */
	net_pkt_write_u8  (ra, 0xC0);                           /* Append L and A flags */
	net_pkt_write_be32(ra, NET_IPV6_ND_INFINITE_LIFETIME);  /* Append valid lifetime */
	net_pkt_write_be32(ra, NET_IPV6_ND_INFINITE_LIFETIME);  /* Append preferred lifetime */
	net_pkt_write_be32(ra, 0);                              /* Append reserved bits */
	net_pkt_write     (ra, prefix->prefix.s6_addr, 16);     /* Append prefix */

	/* Queue RA via shared slot */
	TsSlotframe* sf   = ts_slotframe_find(TSCH_SF_PRIO_0);
	TsSlot*      slot = ts_slot_find(sf, 1);

	// net_pkt_lladdr_src(ra)->addr = net_pkt_lladdr_if(ra)->addr;
	// net_pkt_lladdr_src(ra)->type = net_pkt_lladdr_if(ra)->type;
	// net_pkt_lladdr_src(ra)->len  = net_pkt_lladdr_if(ra)->len;

	net_pkt_lladdr_dst(ra)->addr = net_pkt_lladdr_src(rs)->addr;
	net_pkt_lladdr_dst(ra)->type = net_pkt_lladdr_src(rs)->type;
	net_pkt_lladdr_dst(ra)->len  = net_pkt_lladdr_src(rs)->len;
	// net_pkt_lladdr_dst(ra)->type = NET_LINK_IEEE802154;
	// net_pkt_lladdr_dst(ra)->len  = 8;

	/* Todo: set net_pkt_lladdr_dst */
	if(tsch_tx_pkt(ra, slot, K_MSEC(20000), false) != 0)
	{
		NET_DBG("DROP: Cannot send RA");
		goto drop;
	}

	net_pkt_unref(ra);
	net_pkt_unref(rs);
	// net_pkt_print();
	return NET_OK;

	drop:
		if(ra)
		{
			net_pkt_unref(ra);
		}

		// net_pkt_print();
		return NET_DROP;
}


/* tsch_create_network **************************************************************************//**
 * @brief		Starts a new mesh network. */
void tsch_create_network(void)
{
	tsch_handle_event(TSCH_START_NETWORK_EVENT);
}


/* tsch_power_up *********************************************************************************//**
 * @brief		*/
void tsch_power_up(void)
{
	LOG_DBG("tsch power up");

	dw1000_wakeup_by_cs(&dw);
}


/* tsch_power_down *******************************************************************************//**
 * @brief		*/
void tsch_power_down(void)
{
	LOG_DBG("tsch power down");

	/* Requires that sleep has been already configured via dw1000_config_sleep */
	dw1000_enter_sleep(&dw);

	// NRF_TIMER0->TASKS_SHUTDOWN = 1;
	// NRF_TIMER1->TASKS_SHUTDOWN = 1;
}


/* tsch_start_scan ******************************************************************************//**
 * @brief		Starts scanning for a mesh network. */
void tsch_start_scan(bool (*on_scan_cb)(Ieee154_Frame*))
{
	tsch.on_scan_cb = on_scan_cb;
	tsch_handle_event(TSCH_START_SCAN_EVENT);
}


/* tsch_stop_scan *******************************************************************************//**
 * @brief		Stops scanning for a mesh network. */
void tsch_stop_scan(void)
{
	tsch_handle_event(TSCH_STOP_SCAN_EVENT);
}


/* tsch_meas_dist *******************************************************************************//**
 * @brief		Starts a distance measurement between this node and the destination node. The
 *				destination node is assumed to be in the local neighborhood. */
void tsch_meas_dist(const uint8_t* dest)
{
	LOG_DBG("start");

	Ieee154_Frame* tx = tsch_reserve_frame();

	if(!tx)
	{
		LOG_ERR("could not allocate frame");
		return;
	}

	TsSlotframe* sf   = ts_slotframe_find(TSCH_SF_PRIO_0);
	TsSlot*      slot = ts_slot_find(sf, 1);

	if(!slot)
	{
		LOG_ERR("could not find shared slot");
		return;
	}

	ieee154_data_frame_init(tx, ieee154_ptr_start(tx), ieee154_size(tx));
	ieee154_set_seqnum     (tx, tsch.dsn++);
	ieee154_set_addr       (tx, 0, dest, 8, 0, tsch.addr, 8);

	LOG_DBG("tx dist meas: %p", tx);

	k_queue_append(&slot->tx_queue, tx);
}


/* tsch_handle_timeout **************************************************************************//**
 * @brief		Work item which raises TSCH_TIMEOUT_EVENT. */
static void tsch_handle_timeout(struct k_work* work)
{
	LOG_DBG("timeout event");
	tsch_handle_event(TSCH_TIMEOUT_EVENT);
}


/* tsch_handle_sync_lost ************************************************************************//**
 * @brief		Work item which raises TSCH_DISCONNECTED_EVENT. */
static void tsch_handle_sync_lost(struct k_work* work)
{
	LOG_DBG("sync lost");
	tsch_handle_event(TSCH_DISCONNECT_EVENT);
}


/* tsch_handle_do_nothing ***********************************************************************//**
 * @brief		Work item which does nothing. */
static void tsch_handle_do_nothing(struct k_work* work)
{

}


/* tsch_handle_event ****************************************************************************//**
 * @brief		*/
static void tsch_handle_event(Tsch_Event id)
{
	k_mutex_lock(&tsch.state_mutex, K_FOREVER);

	/* State transition logic */
	switch(tsch.state)
	{
		case TSCH_IDLE_STATE:
			/* Todo: rejoin mcast addr */
			if(id == TSCH_START_SCAN_EVENT)
			{
				LOG_INF("TSCH_IDLE_STATE. Got TSCH_START_SCAN_EVENT -> TSCH_SCANNING_STATE");
				tsch.next_state = TSCH_SCANNING_STATE;
				/* Todo: start scan timeout here */
			}
			else if(id == TSCH_START_NETWORK_EVENT)
			{
				LOG_INF("TSCH_IDLE_STATE. Got TSCH_START_NETWORK_EVENT -> TSCH_CONNECTED_STATE");
				tsch.next_state = TSCH_CONNECTED_STATE;

				ts_slotframe_remove(ts_slotframe_find(TSCH_SF_PRIO_0));
				ts_slotframe_remove(ts_slotframe_find(TSCH_SF_SCAN));

				TsSlotframe* sf = ts_slotframe_add(TSCH_SF_PRIO_0, TSCH_DEFAULT_NUM_SLOTS);
				ts_slot_add(sf,
					TSCH_OPT_TX_LINK | TSCH_OPT_RX_LINK | TSCH_OPT_SHARED_LINK,
					0,
					tsch_adv_slot);

				ts_slot_add(sf,
					TSCH_OPT_TX_LINK | TSCH_OPT_RX_LINK | TSCH_OPT_SHARED_LINK,
					1,
					tsch_shared_slot);

				loc_start_root();
				k_work_cancel_delayable(&tsch.timeout_work);
				k_work_init_delayable(&tsch.sync_lost_work, tsch_handle_do_nothing);
			}
			break;

		case TSCH_SCANNING_STATE:
			if(id == TSCH_TIMEOUT_EVENT || id == TSCH_STOP_SCAN_EVENT)
			{
				LOG_INF("TSCH_SCANNING_STATE. Got TSCH_TIMEOUT_EVENT/TSCH_STOP_SCAN_EVENT -> "
					"TSCH_IDLE_STATE");
				tsch.next_state = TSCH_IDLE_STATE;
			}
			else if(id == TSCH_SYNC_EVENT)
			{
				LOG_INF("TSCH_SCANNING_STATE. Got TSCH_SYNC_EVENT -> TSCH_SYNCED_STATE");
				tsch.next_state = TSCH_SYNCED_STATE;
				k_work_init_delayable(&tsch.sync_lost_work, tsch_handle_sync_lost);
			}
			break;

		case TSCH_SYNCED_STATE:
			if(id == TSCH_TIMEOUT_EVENT)
			{
				LOG_INF("TSCH_SYNCED_STATE. Got TSCH_TIMEOUT_EVENT -> TSCH_CONNECTED_STATE");
				tsch.next_state = TSCH_CONNECTED_STATE;
			}
			if(id == TSCH_DISCONNECT_EVENT)
			{
				LOG_INF("TSCH_SYNCED_STATE. Got TSCH_DISCONNECT_EVENT -> TSCH_IDLE_STATE");
				tsch.next_state = TSCH_IDLE_STATE;
			}
			break;

		case TSCH_CONNECTED_STATE:
			if(id == TSCH_DISCONNECT_EVENT)
			{
				LOG_INF("TSCH_CONNECTED_STATE. Got TSCH_DISCONNECT_EVENT -> TSCH_IDLE_STATE");
				tsch.next_state = TSCH_IDLE_STATE;
			}

		default: break;
	}

	if(tsch.next_state == tsch.state)
	{
		goto cleanup;
	}

	tsch.state = tsch.next_state;

	/* Next state logic */
	switch(tsch.next_state)
	{
		case TSCH_IDLE_STATE: {
			net_if_carrier_down(tsch_iface);
			loc_stop();
			ts_slotframe_remove(ts_slotframe_find(TSCH_SF_PRIO_0));
			ts_slotframe_remove(ts_slotframe_find(TSCH_SF_SCAN));
			k_work_cancel_delayable(&tsch.timeout_work);
			k_work_cancel_delayable(&tsch.sync_lost_work);
			break;
		}

		case TSCH_SCANNING_STATE: {
			ts_slotframe_remove(ts_slotframe_find(TSCH_SF_PRIO_0));
			ts_slotframe_remove(ts_slotframe_find(TSCH_SF_SCAN));

			TsSlotframe* sf = ts_slotframe_add(TSCH_SF_SCAN, TSCH_DEFAULT_NUM_SLOTS - 3);
			ts_slot_add(sf, TSCH_SCAN, 0, tsch_scan_slot);
			k_work_cancel_delayable(&tsch.timeout_work);
			break;
		}

		case TSCH_SYNCED_STATE: {
			net_if_up(tsch_iface);

			/* Stop all scanning cells */
			ts_slotframe_remove(ts_slotframe_find(TSCH_SF_SCAN));

			/* Add shared slot to timeslot */
			TsSlotframe* sf = ts_slotframe_add(TSCH_SF_PRIO_0, TSCH_DEFAULT_NUM_SLOTS);
			ts_slot_add(sf,
				TSCH_OPT_TX_LINK | TSCH_OPT_RX_LINK | TSCH_OPT_SHARED_LINK,
				0,
				tsch_adv_slot);

			ts_slot_add(sf,
				TSCH_OPT_TX_LINK | TSCH_OPT_RX_LINK | TSCH_OPT_SHARED_LINK,
				1,
				tsch_shared_slot);

			loc_start();

			/* Timeout immediately to transition to the CONNECTED state. */
			k_work_schedule(&tsch.timeout_work, K_MSEC(0));
			break;
		}

		case TSCH_CONNECTED_STATE: {
			struct net_if_mcast_addr* maddr;
			struct in6_addr           addr = { 0 };
			net_if_up(tsch_iface);
			net_ipv6_addr_create(&addr, 0xFF22, 0, 0, 0, 0, 0, 0, 0x0002);
			maddr = net_if_ipv6_maddr_add(tsch_iface, &addr);
			if(maddr) {
				net_if_ipv6_maddr_join(maddr);
			} else {
				LOG_ERR("could not add multicast addr to tsch");
			}
			break;
		}

		default: break;
	}

	cleanup:
		k_mutex_unlock(&tsch.state_mutex);
}





// ----------------------------------------------------------------------------------------------- //
// TSCH Slot Functions                                                                             //
// ----------------------------------------------------------------------------------------------- //
/* tsch_scan_slot *******************************************************************************//**
 * @brief		A scan slot receives beacon frames and passes the frame to a user supplied callback
 * 				which returns true if this node should join the mesh advertised by the beacon. */
static void tsch_scan_slot(TsSlot* slot)
{
	LOG_DBG("scan started");

	dw1000_lock(&dw);
	dw1000_set_rx_timeout(&dw, 0);

	Ieee154_Frame* rx = tsch_reserve_frame();
	Ieee154_IE     ie;

	uint32_t status       = dw1000_read_status(&dw);
	uint64_t asn          = 0;
	int32_t  local_tstamp = 0;
	uint64_t toffset      = 0;
	int64_t  duration     = 60000;

	while(duration > 0)
	{
		int64_t now  = k_uptime_get();
		local_tstamp = tsch_radio_rx(rx, -1ull, duration, &status);
		duration    -= k_uptime_delta(&now);

		if((status & DW1000_SYS_STATUS_RXFCG) && ieee154_frame_type(rx) == IEEE154_FRAME_TYPE_BEACON)
		{
			ie = ieee154_ie_first(rx);

			while(ieee154_ie_is_valid(&ie))
			{
				if(ieee154_ie_is_hie(&ie) && ieee154_ie_type(&ie) == TSCH_SYNC_IE)
				{
					if(tsch.on_scan_cb && tsch.on_scan_cb(rx))
					{
						Buffer* b = ieee154_ie_reset_buffer(&ie);
						asn     = le_get_u64(buffer_pop_u64(b));
						// asn     = le_get_u64(ieee154_ie_ptr_content(&ie));
						toffset = ts_current_toffset(local_tstamp);
						LOG_DBG("asn = %d. toffset = %d", (uint32_t)asn, (uint32_t)toffset);
						goto sync;
					}
				}

				ieee154_ie_next(&ie);
			}
		}
	}

	tsch_release_frame(rx);
	dw1000_unlock(&dw);
	LOG_DBG("scan timeout");
	return;

	sync:
		/* Synchronize to the network */
		ts_sync(asn, toffset - TSCH_TX_OFFSET_US);

		tsch_handle_event(TSCH_SYNC_EVENT);
		tsch_release_frame(rx);
		dw1000_unlock(&dw);
		LOG_DBG("sync");
		return;
}


/* tsch_adv_slot ********************************************************************************//**
 * @brief		The advertising slot is a dedicated slot where nodes send mesh advertisement
 *				beacons. Currently, only prime nodes send advertisments and in a specific order. For
 * 				example, prime node's with index 0 transmit first, then the next time the advertising
 * 				slot is active, prime node's with index 1 transmit. Likewise for prime node's 2 and
 * 				3.
 *
 * 				Conflict-free advertising can occur if advertising is based on a node's location.
 * 				Since prime nodes are the time reference for location updates and prime nodes
 * 				cover the mesh network, it makes sense that prime nodes should advertise. */
static void tsch_adv_slot(TsSlot* slot)
{
	dw1000_lock(&dw);

	LOG_DBG("start");

	Ieee154_Frame* frame = &tsch_adv_frame;

	int32_t  local_tstamp;
	uint32_t status = dw1000_read_status(&dw);
	uint64_t tstamp = dw1000_read_sys_tstamp(&dw);
	uint64_t asn    = ts_current_asn();
	unsigned idx    = loc_beacon_index();

	/* Prime beacons (index 0, 1, 2, 3) transmit advertisements */
	if(loc_is_beacon() && idx < 4)
	{
		if(idx == (asn / slot->slotframe->numslots) % 4)
		{
			LOG_DBG("start tx adv (%d). asn = %d", idx, (uint32_t)asn);

			const char ssid[] = "Hyperspace";

			ieee154_beacon_frame_init(frame, tsch_adv_frame_data, sizeof(tsch_adv_frame_data));
			ieee154_set_seqnum       (frame, tsch.ebsn++);
			ieee154_set_addr         (frame, 0, tsch.bcast, 8, 0, tsch.addr, 8);

			Ieee154_IE ie = ieee154_ie_first(frame);
			ieee154_hie_append(&ie, TSCH_SSID_IE, ssid, sizeof(ssid) - 1);
			ieee154_hie_append(&ie, TSCH_SYNC_IE, &asn, sizeof(asn));
			ieee154_hie_append(&ie, IEEE154_HT2_IE, 0, 0);

			tsch_radio_start_tx(frame, tstamp + dw1000_us_to_ticks(TSCH_TX_OFFSET_US));
			tsch_radio_wait_tx (&status);
		}
	}
	/* Receive advertisement */
	else
	{
		ieee154_frame_init(frame, tsch_adv_frame_data, 0, sizeof(tsch_adv_frame_data));

		LOG_DBG("start rx adv. asn = %d", (uint32_t)asn);

		dw1000_set_rx_timeout(&dw, TSCH_RX_TIMEOUT_US);

		local_tstamp = tsch_radio_rx(
			frame,
			tstamp + dw1000_us_to_ticks(TSCH_RX_OFFSET_US),
			-1u,
			&status);

		if(status & (
			DW1000_SYS_STATUS_RXRFTO  |	/* Receive Frame Wait Timeout */
			DW1000_SYS_STATUS_RXPTO   |	/* Preamble Detection Timeout */
			DW1000_SYS_STATUS_RXPHE   |	/* Receiver PHY Header Error */
			DW1000_SYS_STATUS_RXFCE   |	/* Receiver FCS Error */
			DW1000_SYS_STATUS_RXRFSL  |	/* Receiver Reed Solomon Frame Sync Loss */
			DW1000_SYS_STATUS_RXSFDTO |	/* Receive SFD Timeout */
			DW1000_SYS_STATUS_AFFREJ  |	/* Automatic Frame Filtering Rejection */
			DW1000_SYS_STATUS_LDEERR))	/* Leading edge detection processing error */
		{
			goto done;
		}

		if((status & DW1000_SYS_STATUS_RXFCG) == 0)
		{
			goto done;
		}

		k_work_reschedule(&tsch.sync_lost_work, K_MSEC(TSCH_SYNC_LOST_TIMEOUT));

		// ts_offset(local_tstamp - TSCH_TX_OFFSET_US);
		ts_offset((local_tstamp - TSCH_TX_OFFSET_US) / 2);

		if(tsch_valid_addr(slot, frame))
		{
			tsch_handle_rx(slot, frame);
		}
	}

	done:
		LOG_DBG("done");
		tsch_release_frame(frame);
		dw1000_unlock(&dw);
}


/* tsch_shared_slot *****************************************************************************//**
 * @brief		Shared cell where nodes contend for the slot. */
static void tsch_shared_slot(TsSlot* slot)
{
	dw1000_lock(&dw);

	uint64_t asn    = ts_current_asn();
	uint64_t tstamp = dw1000_read_sys_tstamp(&dw);

	nrf_ppi_group_enable(NRF_PPI, NRF_PPI_CHANNEL_GROUP2);

	LOG_DBG("start shared slot. asn = %d", (uint32_t)asn);

	/* Idle state logic. If this node is a beacon, transmit an advertisement in the shared slot 33%
	 * of the time. Transmitting an advertisement in the shared slot is required to synchronize the
	 * clocks of other nodes in the network. The goal with clock sync transmits is to try and sync
	 * clocks atleast once every 10s. */
	if(tsch.shared_cell_state == TSCH_CELL_IDLE_STATE)
	{
		if(tsch.state == TSCH_CONNECTED_STATE && loc_is_beacon() && calc_randf() <= 0.25f)
		{
			tsch.shared_cell_state = TSCH_CELL_ADV_STATE;
		}
		else if(!k_queue_is_empty(&slot->tx_queue))
		{
			tsch.shared_cell_state = TSCH_CELL_TX_STATE;
		}
	}
	/* Bayesian broadcast is stable only for packet arrival rates of less than (e-2)^-1 or ~0.3678.
	 * Enter the cool off state after every transmit before allowing this node to transmit again. */
	else if(tsch.shared_cell_state == TSCH_CELL_COOL_OFF_STATE)
	{
		if(slot->dropcount++ >= 2)
		{
			slot->dropcount = 0;
			tsch.shared_cell_state = TSCH_CELL_IDLE_STATE;
		}
	}

	if(tsch.shared_cell_state == TSCH_CELL_ADV_STATE && bayes_try(&tsch.bayes_bcast))
	{
		tsch_shared_adv(slot, tstamp, asn);
	}
	else if(tsch.shared_cell_state == TSCH_CELL_TX_STATE && bayes_try(&tsch.bayes_bcast))
	{
		tsch_shared_tx(slot, tstamp, asn);
	}
	else
	{
		tsch_shared_rx(slot, tstamp, asn);
	}

	bayes_update(&tsch.bayes_bcast);
	dw1000_unlock(&dw);
}


/* tsch_shared_adv ******************************************************************************//**
 * @brief		*/
static void tsch_shared_adv(TsSlot* slot, uint64_t tstamp, uint64_t asn)
{
	/* Todo: combine with tsch_adv_slot */
	uint32_t status;
	const char ssid[] = "Hyperspace";

	Ieee154_Frame* frame = &tsch_adv_frame;

	ieee154_beacon_frame_init(frame, tsch_adv_frame_data, sizeof(tsch_adv_frame_data));
	ieee154_set_seqnum       (frame, tsch.ebsn++);
	ieee154_set_addr         (frame, 0, tsch.bcast, 8, 0, tsch.addr, 8);

	Ieee154_IE ie = ieee154_ie_first(frame);
	ieee154_hie_append(&ie, TSCH_SSID_IE, ssid, sizeof(ssid) - 1);
	ieee154_hie_append(&ie, TSCH_SYNC_IE, &asn, sizeof(asn));
	ieee154_hie_append(&ie, IEEE154_HT2_IE, 0, 0);

	tsch_radio_start_tx(frame, tstamp + dw1000_us_to_ticks(TSCH_TX_OFFSET_US));
	tsch_radio_wait_tx (&status);

	slot->dropcount = 0;
	tsch.shared_cell_state = TSCH_CELL_COOL_OFF_STATE;
	bayes_success(&tsch.bayes_bcast);

	tsch_release_frame(frame);
}


/* tsch_shared_tx *******************************************************************************//**
 * @brief		Transmits a frame. Expects an ACK if the frame is addressed to a specific address.
 * 				The frame is retransmitted a certain number of times and then dropped if no ACK is
 * 				received. */
static void tsch_shared_tx(TsSlot* slot, uint64_t tstamp, uint64_t asn)
{
	uint32_t status;
	Ieee154_Frame* tx  = k_queue_peek_head(&slot->tx_queue);
	Ieee154_Frame* ack = 0;

	if(tx)
	{
		LOG_INF("asn = %d tx (%p)", (uint32_t)asn, tx);
	}
	else
	{
		LOG_DBG("asn = %d tx (0)", (uint32_t)asn);
	}

	uint64_t txtstamp;
	txtstamp = tstamp + dw1000_us_to_ticks(TSCH_TX_OFFSET_US);
	txtstamp = calc_addmod_u64(txtstamp, tsch_radio_start_tx(tx, txtstamp), DW1000_TSTAMP_PERIOD);

	tsch_radio_wait_tx(&status);

	/* Transmit beacon once */
	if(ieee154_frame_type(tx) == IEEE154_FRAME_TYPE_BEACON)
	{
		tx = k_queue_get(&slot->tx_queue, K_NO_WAIT);
		tsch_release_frame(tx);
	}
	/* Flood packet if dest addr is broadcast address. Flooding means retransmitting a frame
	 * dropcount number of times without expecting an ack. */
	/* Todo: only forward the packet if this node is a beacon. */
	else if(memcmp(ieee154_dest_addr(tx), tsch.bcast, ieee154_length_dest_addr(tx)) == 0)
	{
		goto flood;
	}
	/* Transmit packet and expect an ack. If no ack, retransmit dropcount number of times. */
	else
	{
		ack = tsch_reserve_frame();
		if(!ack)
		{
			LOG_ERR("failed allocating ack frame");
			goto drop;
		}

		LOG_DBG("start rx ack");

		dw1000_set_rx_timeout(&dw, TSCH_RX_ACK_TIMEOUT_US);

		tsch_radio_rx(ack, tstamp + dw1000_us_to_ticks(TSCH_RX_ACK_OFFSET_US), -1u, &status);

		/* RX timeout */
		if(status & (DW1000_SYS_STATUS_RXRFTO | DW1000_SYS_STATUS_RXPTO))
		{
			/* No ack, slot was a collision */
			LOG_DBG("rx timed out");
			goto collision;
		}

		/* RX error */
		if(status & (
			DW1000_SYS_STATUS_RXPHE   | DW1000_SYS_STATUS_RXFCE  | DW1000_SYS_STATUS_RXRFSL |
			DW1000_SYS_STATUS_RXSFDTO | DW1000_SYS_STATUS_AFFREJ | DW1000_SYS_STATUS_LDEERR))
		{
			/* Slot was a collision */
			LOG_DBG("collision");
			goto collision;
		}

		/* Finally check if rx frame is good */
		if((status & DW1000_SYS_STATUS_RXFCG) == 0)
		{
			goto collision;
		}

		if(ieee154_frame_type(ack) != IEEE154_FRAME_TYPE_ACK || !tsch_valid_addr(slot, ack))
		{
			LOG_INF("invalid dest addr");
			goto collision;
		}

		/* Todo: time sync to ACK packet */

		Ieee154_IE ie = ieee154_ie_first(ack);

		while(ieee154_ie_is_valid(&ie))
		{
			if(ieee154_ie_is_hie(&ie) && ieee154_ie_type(&ie) == TSCH_TRESP_IE)
			{
				uint64_t rxtstamp;
				rxtstamp = dw1000_read_rx_tstamp(&dw);
				rxtstamp = calc_submod_u64(rxtstamp, dw1000_ant_delay(&dw), DW1000_TSTAMP_PERIOD);

				float    rco  =  dw1000_rx_clk_offset(&dw);
				uint32_t dur  =  le_get_u32(ieee154_ie_ptr_content(&ie)) * (1.0f - rco);
				uint32_t dist = (calc_submod_u64(rxtstamp, txtstamp, DW1000_TSTAMP_PERIOD) - dur)/2;

				loc_dist_measured(ieee154_dest_addr(tx), dist);
			}

			ieee154_ie_next(&ie);
		}

		LOG_DBG("success");
		tsch_handle_ack(slot, tx, ack);
	}

	LOG_DBG("done");
	slot->dropcount = 0;
	tsch.shared_cell_state = TSCH_CELL_COOL_OFF_STATE;
	// backoff_success(&tsch.backoff);
	bayes_success(&tsch.bayes_bcast);
	return;

	flood:
		LOG_INF("flood %d/3", slot->dropcount + 1);
		bayes_fail(&tsch.bayes_bcast);
		if(++slot->dropcount >= 3)
		{
			tx = k_queue_get(&slot->tx_queue, K_NO_WAIT);
			tsch_release_frame(tx);
			slot->dropcount = 0;
			tsch.shared_cell_state = TSCH_CELL_COOL_OFF_STATE;
		}
		return;

	collision:
		LOG_INF("collision");
		// backoff_fail(&tsch.backoff);
		bayes_fail(&tsch.bayes_bcast);

	drop:
		LOG_INF("dropping (%d/5)", slot->dropcount + 1);

		if(++slot->dropcount >= 5)
		{
			tx = k_queue_get(&slot->tx_queue, K_NO_WAIT);
			tsch_release_frame(tx);
			slot->dropcount = 0;
			tsch.shared_cell_state = TSCH_CELL_COOL_OFF_STATE;
			// backoff_reset(&tsch.backoff);
		}

		tsch_release_frame(ack);
}


/* tsch_shared_rx *******************************************************************************//**
 * @brief		Receives a frame. An ack is transmitted if the frame is addressed to this node. */
static void tsch_shared_rx(TsSlot* slot, uint64_t tstamp, uint64_t asn)
{
	LOG_DBG("rx");

	uint64_t rxtstamp;
	uint64_t acktstamp;
	int32_t  local_tstamp;
	uint32_t status = dw1000_read_status(&dw);
	Ieee154_Frame* rx  = tsch_reserve_frame();
	Ieee154_Frame* ack = 0;

	if(!rx)
	{
		LOG_ERR("failed allocating rx frame");
		goto drop;
	}

	dw1000_set_rx_timeout(&dw, TSCH_RX_TIMEOUT_US);
	local_tstamp = tsch_radio_rx(rx, tstamp + dw1000_us_to_ticks(TSCH_RX_OFFSET_US), -1u, &status);
	rxtstamp     = dw1000_read_rx_tstamp(&dw);
	rxtstamp     = calc_submod_u64(rxtstamp, dw1000_ant_delay(&dw), DW1000_TSTAMP_PERIOD);

	/* RX timeout */
	if(status & (DW1000_SYS_STATUS_RXRFTO | DW1000_SYS_STATUS_RXPTO))
	{
		LOG_DBG("timed out");
		bayes_hole(&tsch.bayes_bcast);
		goto drop;
	}

	/* RX error */
	if(status & (
		DW1000_SYS_STATUS_RXPHE   | DW1000_SYS_STATUS_RXFCE  | DW1000_SYS_STATUS_RXRFSL |
		DW1000_SYS_STATUS_RXSFDTO | DW1000_SYS_STATUS_AFFREJ | DW1000_SYS_STATUS_LDEERR))
	{
		/* Slot was a collision */
		LOG_DBG("collision");
		goto collision;
	}

	/* Finally check if rx frame is good */
	if((status & DW1000_SYS_STATUS_RXFCG) == 0)
	{
		goto collision;
	}

	k_work_reschedule(&tsch.sync_lost_work, K_MSEC(TSCH_SYNC_LOST_TIMEOUT));

	if(!tsch_valid_addr(slot, rx))
	{
		LOG_DBG("invalid dest addr");

		uint8_t* src  = ieee154_src_addr(rx);
		uint8_t* dest = ieee154_dest_addr(rx);

		LOG_DBG("src  = %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
			src[0], src[1], src[2], src[3], src[4], src[5], src[6], src[7]);

		LOG_DBG("dest = %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
			dest[0], dest[1], dest[2], dest[3], dest[4], dest[5], dest[6], dest[7]);
	}
	else
	{
		// ts_offset(local_tstamp - TSCH_TX_OFFSET_US);
		ts_offset((local_tstamp - TSCH_TX_OFFSET_US) / 2);

		/* Transmit ack */
		if(ieee154_frame_type(rx) != IEEE154_FRAME_TYPE_BEACON &&
		   memcmp(ieee154_dest_addr(rx), tsch.bcast, ieee154_length_dest_addr(rx)) != 0)
		{
			ack = tsch_reserve_frame();
			if(!ack)
			{
				LOG_ERR("failed allocating ack frame");
				goto drop;
			}

			LOG_DBG("tx ack");
			ieee154_ack_frame_init(ack, ieee154_ptr_start(ack), ieee154_size(ack));

			if(ieee154_length_seqnum(rx))
			{
				ieee154_set_seqnum(ack, ieee154_seqnum(rx));
			}

			ieee154_set_addr(ack,
				0, ieee154_src_addr(rx), ieee154_length_src_addr(rx),
				0, tsch.addr, 8);

			acktstamp  = calc_addmod_u64(rxtstamp,
				dw1000_us_to_ticks(TSCH_TX_ACK_OFFSET_US - TSCH_TX_OFFSET_US), DW1000_TSTAMP_PERIOD);
			acktstamp += dw1000_set_trx_tstamp(&dw, acktstamp);
			acktstamp += dw1000_ant_delay     (&dw);

			Ieee154_IE ie  = ieee154_ie_first(ack);
			uint32_t   dur = calc_submod_u64(acktstamp, rxtstamp, DW1000_TSTAMP_PERIOD);
			ieee154_hie_append(&ie, TSCH_TRESP_IE, &dur, sizeof(dur));
			ieee154_hie_append(&ie, IEEE154_HT2_IE, 0, 0);

			dw1000_write_tx_fctrl(&dw, 0, ieee154_length(ack) + 2);
			dw1000_start_delayed_tx(&dw, false);
			dw1000_write_tx(&dw, ieee154_ptr_start(ack), 0, ieee154_length(ack));
			tsch_release_frame(ack);
		}

		tsch_handle_rx(slot, rx);

		if(ack)
		{
			tsch_radio_wait_tx(&status);
		}
	}

	LOG_DBG("done");

	/* Slot was a success */
	tsch_release_frame(rx);
	// backoff_success(&tsch.backoff);
	bayes_success(&tsch.bayes_bcast);
	return;

	collision:
		LOG_DBG("collision");
		// backoff_fail(&tsch.backoff);
		bayes_fail(&tsch.bayes_bcast);

	drop:
		LOG_DBG("drop");
		tsch_release_frame(rx);
		tsch_release_frame(ack);
}


/* tsch_radio_start_tx **************************************************************************//**
 * @brief		Common logic for starting a radio transmission. */
static int32_t tsch_radio_start_tx(Ieee154_Frame* tx, uint64_t tstamp)
{
	int32_t trx_offset = dw1000_set_trx_tstamp(&dw, tstamp) + dw1000_ant_delay(&dw);

	dw1000_write_tx_fctrl  (&dw, 0, ieee154_length(tx) + 2);
	dw1000_start_delayed_tx(&dw, false);
	dw1000_write_tx        (&dw, ieee154_ptr_start(tx), 0, ieee154_length(tx));

	return trx_offset;
}


/* tsch_radio_wait_tx ***************************************************************************//**
 * @brief		Waits for a perviously started transmission to complete. */
static void tsch_radio_wait_tx(uint32_t* status)
{
	dw1000_wait_for_irq(&dw, -1u);

	*status = dw1000_handle_irq(&dw);
}


/* tsch_radio_rx ********************************************************************************//**
 * @brief		Common logic for receiving a frame. */
static int32_t tsch_radio_rx(Ieee154_Frame* rx, uint64_t tstamp, uint32_t timeout, uint32_t* status)
{
	nrf_ppi_group_enable(NRF_PPI, NRF_PPI_CHANNEL_GROUP2);
	dw1000_sync_drxb    (&dw, *status);

	if(tstamp != -1ull)
	{
		dw1000_set_trx_tstamp  (&dw, tstamp);
		dw1000_start_delayed_rx(&dw);
	}
	else
	{
		dw1000_start_rx(&dw);
	}

	*status = 0;

	while(0 == (*status & (
		DW1000_SYS_STATUS_TXFRS   |	/* Tx Complete */
		DW1000_SYS_STATUS_RXFCG   |	/* Rx Complete */
		DW1000_SYS_STATUS_RXRFTO  |	/* Receive Frame Wait Timeout */
		DW1000_SYS_STATUS_RXPTO   |	/* Preamble Detection Timeout */
		DW1000_SYS_STATUS_RXPHE   |	/* Receiver PHY Header Error */
		DW1000_SYS_STATUS_RXFCE   |	/* Receiver FCS Error */
		DW1000_SYS_STATUS_RXRFSL  |	/* Receiver Reed Solomon Frame Sync Loss */
		DW1000_SYS_STATUS_RXSFDTO |	/* Receive SFD Timeout */
		DW1000_SYS_STATUS_AFFREJ  |	/* Automatic Frame Filtering Rejection */
		DW1000_SYS_STATUS_LDEERR)))	/* Leading edge detection processing error */
	{
		dw1000_wait_for_irq(&dw, timeout);

		*status = dw1000_handle_irq(&dw);
	}

	if(*status & DW1000_SYS_STATUS_RXFCG)
	{
		uint32_t finfo = dw1000_read_rx_finfo(&dw);
		uint32_t flen  = finfo & DW1000_RX_FINFO_RXFLEN_MASK;
		dw1000_read_rx(&dw, ieee154_set_length(rx, flen), 0, flen);
		ieee154_parse(rx);
	}

	return NRF_TIMER0->CC[3];
}





/* tsch_handle_rx *******************************************************************************//**
 * @brief		Handles receiving a frame. Currently only handles DATA frames. */
static void tsch_handle_rx(TsSlot* slot, Ieee154_Frame* rx)
{
	uint16_t type = ieee154_frame_type(rx);

	uint8_t* src  = ieee154_src_addr(rx);
	uint8_t* dest = ieee154_dest_addr(rx);

	LOG_DBG("src  = %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
		src[0], src[1], src[2], src[3], src[4], src[5], src[6], src[7]);

	LOG_DBG("dest = %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
		dest[0], dest[1], dest[2], dest[3], dest[4], dest[5], dest[6], dest[7]);

	/* Todo: add hypernbr and validate sequence numbers */

	/* Debug */
	if(type == IEEE154_FRAME_TYPE_BEACON) {
		LOG_DBG("BEACON");
	} else if(type == IEEE154_FRAME_TYPE_DATA) {
		LOG_DBG("DATA");
	} else if(type == IEEE154_FRAME_TYPE_MAC) {
		LOG_DBG("MAC");
	} else if(type == IEEE154_FRAME_TYPE_MULTI) {
		LOG_DBG("MULTI");
	} else if(type == IEEE154_FRAME_TYPE_FRAG) {
		LOG_DBG("FRAG");
	} else if(type == IEEE154_FRAME_TYPE_EXT) {
		LOG_DBG("EXT");
	}

	if(type == IEEE154_FRAME_TYPE_DATA || type == IEEE154_FRAME_TYPE_BEACON)
	{
		LOG_DBG("rx (%p)", rx);
		tsch_handle_rx_data(slot, rx);
	}
}


/* tsch_handle_rx_data **************************************************************************//**
 * @brief		Handles receiving a DATA frame. Currently expects all data frames to encode a
 * 				6LOWPAN compressed IPv6 packet. */
static void tsch_handle_rx_data(TsSlot* slot, Ieee154_Frame* frame)
{
	uint8_t src[8];
	uint8_t dest[8];

	memmove(src, ieee154_src_addr(frame), 8);
	memmove(dest, ieee154_dest_addr(frame), 8);

	/* Strip CRC */
	frame->buffer.write -= 2;

	struct net_pkt* pkt = lowpan_decompress(tsch_iface, frame);

	if(!pkt)
	{
		LOG_DBG("could not decompress");
		goto drop;
	}

	LOG_INF("asn  = %d rx (%p)", (uint32_t)ts_current_asn(), pkt);
	LOG_INF("src  = %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
		src[0], src[1], src[2], src[3], src[4], src[5], src[6], src[7]);
	LOG_INF("dest = %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
		dest[0], dest[1], dest[2], dest[3], dest[4], dest[5], dest[6], dest[7]);

	if(net_recv_data(tsch_iface, pkt) < 0)
	{
		LOG_DBG("could not recv");
		goto drop;
	}

	return;

	drop:
		LOG_DBG("drop");
		if(pkt)
		{
			net_pkt_unref(pkt);
		}
}


/* tsch_handle_ack ******************************************************************************//**
 * @brief		Handles receiving an ACK to a transmitted frame. If the ACK is valid for the
 * 				transmitted frame, removes the transmitted frame from the slot's tx queue. */
static void tsch_handle_ack(TsSlot* slot, Ieee154_Frame* tx, Ieee154_Frame* ack)
{
	slot->dropcount = 0;

	if(ieee154_seqnum(ack) != ieee154_seqnum(tx))
	{
		LOG_WRN("ack seqnum %d does not match tx seqnum %d",
			ieee154_seqnum(ack), ieee154_seqnum(tx));
		goto done;
	}

	/* TODO: handle ack. Should probably handle all IEs here */

	/* Remove the packet from the tx queue if the transmitted packet was the head of the queue.
	 * Otherwise leave the tx queue untouched. This could happen if transmitting an empty packet
	 * right as another packet is placed onto the tx queue. */
	if(tx == k_queue_peek_head(&slot->tx_queue))
	{
		tx = k_queue_get(&slot->tx_queue, K_NO_WAIT);
	}

	tsch_release_frame(tx);

	done:
		tsch_release_frame(ack);
}


/* tsch_valid_addr ******************************************************************************//**
 * @brief		Returns true if the frame should be received by this node. */
static bool tsch_valid_addr(TsSlot* slot, const Ieee154_Frame* frame)
{
	/* @TODO: refactor out beacon frame check */

	unsigned i;

	/* Check if the packet was addressed to this node */
	// if(ieee154_frame_type(frame) == IEEE154_FRAME_TYPE_BEACON)
	// {
	// 	return true;
	// }
	if(ieee154_length_dest_addr(frame) == IEEE154_DEST_EXTENDED_LENGTH)
	{
		if(memcmp(tsch.addr, ieee154_dest_addr(frame), IEEE154_DEST_EXTENDED_LENGTH) == 0)
		{
			return true;
		}
		else if(memcmp(tsch.bcast, ieee154_dest_addr(frame), IEEE154_DEST_EXTENDED_LENGTH) == 0)
		{
			return true;
		}

		for(i = 0; i < NET_IF_MAX_IPV6_ADDR; i++)
		{
			struct net_if_addr* addr = &tsch_iface->config.ip.ipv6->unicast[i];

			if(addr->is_used &&
				memcmp(
					&addr->address.in6_addr.s6_addr[8],
					ieee154_dest_addr(frame),
					ieee154_length_dest_addr(frame)) == 0)
			{
				return true;
			}
		}

		return false;
	}
	/* Check if the packet was sent to the broadcast address */
	else if(ieee154_length_dest_addr(frame) == IEEE154_DEST_SHORT_LENGTH)
	{
		if(memcmp(tsch.bcast, ieee154_dest_addr(frame), IEEE154_DEST_SHORT_LENGTH) == 0)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else if(ieee154_length_dest_addr(frame) == 0)
	{
		return true;
	}

	return false;
}


/* tsch_reserve_frame ***************************************************************************//**
 * @brief		Allocates a frame. */
static Ieee154_Frame* tsch_reserve_frame(void)
{
	Ieee154_Frame* frame = pool_reserve(&tsch_frame_pool);

	if(!frame)
	{
		LOG_DBG("failed reserving frame. dropping old frame.");

		TsSlotframe*   sf   = ts_slotframe_find(TSCH_SF_PRIO_0);
		TsSlot*        slot = ts_slot_find(sf, 1);
		Ieee154_Frame* old  = k_queue_get(&slot->tx_queue, K_NO_WAIT);

		tsch_release_frame(old);

		frame = pool_reserve(&tsch_frame_pool);
	}

	if(frame)
	{
		ieee154_frame_init(frame, tsch_frame_data[frame-tsch_frames], 0, sizeof(tsch_frame_data[0]));
		LOG_DBG("reserved %p. free = %d", frame, pool_free(&tsch_frame_pool));
	}
	else
	{
		LOG_DBG("failed reserving frame. free = %d", pool_free(&tsch_frame_pool));
	}

	return frame;
}


/* tsch_release_frame ***************************************************************************//**
 * @brief		Deallocates a frame. */
static void tsch_release_frame(Ieee154_Frame* frame)
{
	pool_release(&tsch_frame_pool, frame);

	LOG_DBG("release %p. free = %d", frame, pool_free(&tsch_frame_pool));
}


/******************************************* END OF FILE *******************************************/
