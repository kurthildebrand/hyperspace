/************************************************************************************************//**
 * @file		spis_if.c
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
#if defined(CONFIG_SPIS_IF)
#include "spis_if.h"

#include <drivers/gpio.h>
#include <drivers/spi.h>
#include <net/dummy.h>
#include <net/net_pkt.h>
#include <net/net_core.h>
#include <net/net_l2.h>
#include <net/net_if.h>
#include <zephyr.h>

#include "hyperspace.h"

#include "logging/log.h"
// LOG_MODULE_REGISTER(spis_if, LOG_LEVEL_ERR);
// LOG_MODULE_REGISTER(spis_if, LOG_LEVEL_INF);
LOG_MODULE_REGISTER(spis_if, LOG_LEVEL_DBG);


/* Inline Function Instances --------------------------------------------------------------------- */
/* Private Constants ----------------------------------------------------------------------------- */
// #define SPIS_L2				SPIS
// #define SPIS_L2_CTX_TYPE	void*
#define SPIS_MTU			1280
#define SPIS_READY_PIN		26
#define SPIS_RX_STACK_SIZE	2048


/* Private Types --------------------------------------------------------------------------------- */
struct net_spis_dev_config {

};

struct net_spis_dev_data {
	struct k_thread      rx_thread;
	const struct device* spi;
	struct spi_config    spi_cfg;
	struct k_poll_signal spi_done_signal;
	struct k_fifo        tx_fifo;
	const struct device* gpio;
	struct k_mutex       tx_lock;
	uint8_t mac_addr[8];

	// K_THREAD_STACK_MEMBER(rx_stack, CONFIG_NET_SPIS_STACK_SIZE);
	K_THREAD_STACK_MEMBER(rx_stack, SPIS_RX_STACK_SIZE);
	uint8_t rxbuf[SPIS_MTU];
	uint8_t txbuf[SPIS_MTU];
};

struct net_spis_dev_api {
	struct net_if_api iface_api;
};


/* Private Functions ----------------------------------------------------------------------------- */
static int               net_spis_dev_init (const struct device*);
// static int               net_spis_dev_send (struct device*, struct net_pkt*);
static void              net_spis_if_init  (struct net_if*);
static enum net_verdict  net_spis_if_recv  (struct net_if*, struct net_pkt*);
static int               net_spis_if_send  (struct net_if*, struct net_pkt*);
// static int               spis_if_enable    (struct net_if*, bool);
static enum net_l2_flags net_spis_if_flags (struct net_if*);
static void              net_spis_thread   (void*, void*, void*);
static bool              net_spis_handle_rx(struct net_if*, unsigned);


/* Private Variables ----------------------------------------------------------------------------- */
static struct net_spis_dev_config spis_config;
static struct net_spis_dev_data   spis_data;
static struct net_spis_dev_api    spis_dev_api = {
	.iface_api.init = net_spis_if_init,
};

NET_L2_DECLARE_PUBLIC(TSCH_L2);
NET_L2_INIT(SPIS_L2, net_spis_if_recv, net_spis_if_send, 0, net_spis_if_flags);
NET_DEVICE_INIT(
	net_spi,                                /* dev_name      */
	"SPI IP Driver",                        /* drv_name      */
	net_spis_dev_init,                      /* init_fn       */
	0,                                      /* pm_control_fn */
	&spis_data,                             /* data          */
	&spis_config,                           /* cfg_info      */
	CONFIG_APPLICATION_INIT_PRIORITY,
	// CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
	// CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,    /* prio          */
	// 50,                                  /* prio          */
	&spis_dev_api,                          /* api           */
	SPIS_L2,                                /* l2            */
	void*,                                  /* l2_ctx_type   */
	SPIS_MTU);                              /* mtu           */


/* net_spis_dev_init ****************************************************************************//**
 * @brief		Initializes the net SPI slave device.
 * @desc		Two hardware resources are used by the net SPI slave: 1. An SPI device with the alias
 * 				net-spi. 2. A GPIO pin SPIS_READY_PIN */
static int net_spis_dev_init(const struct device* dev)
{
	int ret;

	struct net_spis_dev_data* data = dev->data;
	data->spi = device_get_binding(DT_LABEL(DT_ALIAS(net_spi)));
	if(!data->spi)
	{
		LOG_ERR("could not get net_spi device");
	}

	data->spi_cfg = (struct spi_config){
		.operation =
			SPI_OP_MODE_SLAVE |
			SPI_WORD_SET(8) |
			SPI_TRANSFER_MSB |
			SPI_MODE_CPOL |
			SPI_MODE_CPHA,
		.frequency = 8000000,
		.slave     = 0,
		.cs        = 0,
	};

	/* TODO: convert to dts */
	data->gpio = device_get_binding("GPIO_0");
	if(!data->gpio)
	{
		LOG_ERR("failed to find net_spi ready gpio pin");
	}

	ret = gpio_pin_configure(data->gpio, SPIS_READY_PIN, GPIO_OUTPUT);
	if(ret)
	{
		LOG_ERR("failed configuring net_spi ready pin");
	}

	return 0;
}


/* net_spis_if_init *****************************************************************************//**
 * @brief		*/
static void net_spis_if_init(struct net_if* iface)
{
	/* @TODO: set if link address */
	struct net_spis_dev_data* data = net_if_get_device(iface)->data;

	// data->mac_addr[0] = 0x02;
	// data->mac_addr[1] = 0x00;
	// data->mac_addr[2] = 0x5E;
	// data->mac_addr[3] = 0x10;
	// data->mac_addr[4] = 0x00;
	// data->mac_addr[5] = 0x00;
	// data->mac_addr[6] = 0x00;
	// data->mac_addr[7] = sys_rand32_get();

	/* Not used but network interface expects one. 00-00-5E-00-53-xx Documentation RFC 7042. */
	// data->mac_addr[0] = 0x00;
	// data->mac_addr[1] = 0x00;
	// data->mac_addr[2] = 0x5E;
	// data->mac_addr[3] = 0x00;
	// data->mac_addr[4] = 0x53;
	// data->mac_addr[5] = sys_rand32_get();

	memmove(data->mac_addr, (const void*)&NRF_FICR->DEVICEID, 8);
	net_if_set_link_addr(iface, data->mac_addr, 8, NET_LINK_IEEE802154);
	net_if_flag_set(iface, NET_IF_POINTOPOINT);
	// net_if_flag_set(iface, NET_IF_NO_AUTO_START);

	/* Configure interface with link local address */
	struct net_if_addr*       ifaddr;
	struct net_if_ipv6*       ipv6;
	struct net_if_mcast_addr* maddr;
	struct in6_addr           addr = { 0 };

	if(net_if_config_ipv6_get(iface, &ipv6) < 0 || !ipv6) {
		LOG_ERR("cannot assign link local addr: IPv6 config not valid");
		return;
	}

	net_ipv6_addr_create_iid(&addr, net_if_get_link_addr(iface));

	ifaddr = net_if_ipv6_addr_add(iface, &addr, NET_ADDR_AUTOCONF, 0);
	if(!ifaddr) {
		LOG_ERR("cannot add link local address to spis_if");
	}

	net_ipv6_addr_create(&addr, 0xFF22, 0, 0, 0, 0, 0, 0, 0x0002);

	maddr = net_if_ipv6_maddr_add(iface, &addr);
	if(!maddr) {
		LOG_ERR("cannot add multicast addr to spis_if");
	}

	net_if_ipv6_maddr_join(maddr);

	k_mutex_init(&data->tx_lock);
	k_poll_signal_init(&data->spi_done_signal);
	k_fifo_init(&data->tx_fifo);
	// k_thread_create(&data->rx_thread, data->rx_stack,
	// 	SPIS_RX_STACK_SIZE,
	// 	net_spis_rx_thread, iface, 0, 0,
	// 	K_PRIO_COOP(1), 0, K_NO_WAIT);

	k_thread_create(&data->rx_thread, data->rx_stack,
		SPIS_RX_STACK_SIZE,
		net_spis_thread, iface, 0, 0,
		K_PRIO_COOP(1), 0, K_NO_WAIT);

	k_thread_name_set(&data->rx_thread, "NET SPIS RX");
}


static void net_spis_thread(void* arg1, void* arg2, void* arg3)
{
	struct net_if* iface = arg1;
	const struct device* dev = net_if_get_device(iface);
	struct net_spis_dev_data* data = dev->data;

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(
			K_POLL_TYPE_SIGNAL,
			K_POLL_MODE_NOTIFY_ONLY,
			&data->spi_done_signal),

		K_POLL_EVENT_INITIALIZER(
			K_POLL_TYPE_FIFO_DATA_AVAILABLE,
			K_POLL_MODE_NOTIFY_ONLY,
			&data->tx_fifo),
	};

	struct spi_buf tx_bufs[] = {
		{ .buf = data->txbuf, .len = 0 },
	};

	struct spi_buf rx_bufs[] = {
		{ .buf = data->rxbuf, .len = SPIS_MTU },
	};

	struct spi_buf_set tx_buf_set = {
		.buffers = tx_bufs,
		.count   = ARRAY_SIZE(tx_bufs),
	};

	struct spi_buf_set rx_buf_set = {
		.buffers = rx_bufs,
		.count   = ARRAY_SIZE(rx_bufs),
	};

	while(1)
	{
		while(!k_fifo_is_empty(&data->tx_fifo))
		{
			struct net_pkt* pkt = k_fifo_get(&data->tx_fifo, K_NO_WAIT);

			int length = net_pkt_get_len(pkt);

			if(length > SPIS_MTU)
			{
				LOG_WRN("DROP: packet to large");
				net_pkt_unref(pkt);
				continue;
			}

			tx_bufs[0].len = length;

			LOG_INF("tx %p len %d", pkt, length);

			net_buf_linearize(&data->txbuf, SPIS_MTU, pkt->frags, 0, length);
			spi_release(data->spi, &data->spi_cfg);

			/* Transmit data to the base station. There is a race between setting the ready pin and
			 * setting up the spi peripheral for the transaction so the master must allow enough time
			 * before starting the read. */
			LOG_DBG("tx start");
			gpio_pin_set(data->gpio, SPIS_READY_PIN, 1);
			int rxlen = spi_transceive(data->spi, &data->spi_cfg, &tx_buf_set, &rx_buf_set);
			gpio_pin_set(data->gpio, SPIS_READY_PIN, 0);
			LOG_DBG("tx done");

			net_spis_handle_rx(iface, rxlen);

			net_pkt_unref(pkt);
		}

		LOG_DBG("rx async");

		/* SPI read async */
		spi_read_async(data->spi, &data->spi_cfg, &rx_buf_set, &data->spi_done_signal);

		/* Wait for signal */
		k_poll(events, ARRAY_SIZE(events), K_FOREVER);

		/* If rx done and packet is valid, handle packet */
		if(events[0].signal->signaled)
		{
			LOG_DBG("rx signalled");
			net_spis_handle_rx(iface, (unsigned)events[0].signal->result);
			LOG_DBG("rx done");

			events[0].signal->signaled = 0;
			events[0].state = K_POLL_STATE_NOT_READY;
		}

		if(events[1].state == K_POLL_STATE_FIFO_DATA_AVAILABLE)
		{
			LOG_DBG("tx new pkt");
			events[1].state = K_POLL_STATE_NOT_READY;
		}
	}
}


/* net_spis_handle_rx ***************************************************************************//**
 * @brief		Handles receiving packets. */
static bool net_spis_handle_rx(struct net_if* iface, unsigned rxcount)
{
	const struct device* dev = net_if_get_device(iface);
	struct net_spis_dev_data* data = dev->data;

	unsigned version = data->rxbuf[0] & 0xF0;

	if(version != 0x60)
	{
		return false;
	}

	/* Leave as AF_UNSPEC so that the header size isn't added to the passed in size */
	struct net_pkt* pkt = net_pkt_alloc_with_buffer(iface, rxcount, AF_UNSPEC, 0, K_NO_WAIT);
	// net_pkt_print();

	if(!pkt)
	{
		LOG_ERR("failed allocating net_pkt");
		return false;
	}

	net_pkt_write(pkt, data->rxbuf, rxcount);
	LOG_INF("rx %p len %d", pkt, rxcount);

	// net_pkt_cursor_init(pkt);

	// NET_PKT_DATA_ACCESS_CONTIGUOUS_DEFINE(ipv6_access, struct net_ipv6_hdr);
	// struct net_ipv6_hdr* hdr = (struct net_ipv6_hdr *)net_pkt_get_data(pkt, &ipv6_access);

	// if(hdr && !net_ipv6_is_my_addr(&hdr->dst) && !net_ipv6_is_my_maddr(&hdr->dst) &&
	//    !net_ipv6_is_addr_mcast(&hdr->dst))
	// {
	// 	struct net_if* tsch_if = net_if_get_first_by_type(&NET_L2_GET_NAME(TSCH_L2));
	// 	if(tsch_if)
	// 	{
	// 		/* TODO: decrement hop limit */
	// 		struct net_pkt* copy = net_pkt_clone(pkt, K_NO_WAIT);
	// 		if(!copy)
	// 		{
	// 			LOG_ERR("could not copy packet to tsch");
	// 		}
	// 		else
	// 		{
	// 			LOG_DBG("forward packet to tsch");
	// 			net_pkt_set_iface(copy, tsch_if);
	// 			net_send_data(copy);
	// 		}
	// 	}
	// }

	/* Set hyperspace source coordinate to this node if not set. Responses will be send back to this
	 * node and forwarded back to the SPI host. */
	HyperOpt* hyperopt = net_pkt_get_hyperopt(pkt);

	if(hyperopt && (!isfinite(hyperopt->src.r) || !isfinite(hyperopt->src.t)))
	{
		hyperopt->src_seq = hyperspace_coord_seq();
		hyperopt->src.r   = hyperspace_coord_r();
		hyperopt->src.t   = hyperspace_coord_t();
	}

	if(net_recv_data(iface, pkt) < 0)
	{
		net_pkt_unref(pkt);
	}

	// net_pkt_print();
	return true;
}


/* spis_if_recv *********************************************************************************//**
 * @brief		*/
static enum net_verdict net_spis_if_recv(struct net_if* iface, struct net_pkt* pkt)
{
	return NET_CONTINUE;
}


/* spis_if_send *********************************************************************************//**
 * @brief		Sends a packet over the spi slave.
 * @ref			static bool net_if_tx(struct net_if *iface, struct net_pkt *pkt)
 * @return		The number of bytes sent. */
static int net_spis_if_send(struct net_if* iface, struct net_pkt* pkt)
{
	const struct device* dev = net_if_get_device(iface);
	struct net_spis_dev_data* data = dev->data;

	int length = net_pkt_get_len(pkt);

	if(length > SPIS_MTU)
	{
		return -ENOBUFS;
	}
	else
	{
		k_fifo_put(&data->tx_fifo, pkt);
		return length;
	}
}


// /* spis_if_enable *******************************************************************************//**
//  * @brief		*/
// static int spis_if_enable(struct net_if* iface, bool enable)
// {

// }


/* spis_if_flags ********************************************************************************//**
 * @brief		*/
static enum net_l2_flags net_spis_if_flags(struct net_if* iface)
{
	return NET_L2_MULTICAST | NET_L2_POINT_TO_POINT;
}


#endif
/******************************************* END OF FILE *******************************************/
