/************************************************************************************************//**
 * @file		main.c
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
#include <zephyr.h>
#include <device.h>
#include <dfu/mcuboot.h>
#include <drivers/clock_control.h>
#include <drivers/clock_control/nrf_clock_control.h>
#include <drivers/gpio.h>
#include <sys/reboot.h>
#include <sys/printk.h>

#include <net/net_pkt.h>
#include <net/net_if.h>
#include <net/net_core.h>
#include <net/net_context.h>
#include <net/udp.h>
#include <net/socket.h>

#include <nrfx/hal/nrf_ppi.h>
#include <nrfx/hal/nrf_gpiote.h>
#include <nrfx/hal/nrf_timer.h>
#include <stdint.h>
#include <stdio.h>

#include "coap_test.h"
#include "dw1000.h"
#include "ieee_802_15_4.h"
#include "hyperspace.h"
#include "location.h"
#include "tsch.h"

#include "logging/log.h"
LOG_MODULE_REGISTER(mesh_nonbeacon, LOG_LEVEL_INF);
// LOG_MODULE_REGISTER(mesh_nonbeacon, LOG_LEVEL_DBG);


/* Inline Function Instances --------------------------------------------------------------------- */
/* Private Constants ----------------------------------------------------------------------------- */
// #define MCAST_IPV6_ADDR {{{ 0xff, 0x02, 0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0x20, 0, 0, 0x1 }}}
// #define MY_IPV6_ADDR    {{{ 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0x20, 0, 0, 0x1 }}}
// #define THEIR_IPV6_ADDR {{{ 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0x20, 0, 0, 0x2 }}}
#define MY_PORT (2200)


/* Private Types --------------------------------------------------------------------------------- */
/* Private Functions ----------------------------------------------------------------------------- */
void update_hyperspace_coords(void* p1, void* p2, void* p3);
void mcast_listener(void* p1, void* p2, void* p3);
bool on_scan       (Ieee154_Frame*);
void on_connect    ();
void tsch_net_test (void* p1, void* p2, void* p3);
void spi_test      (void* p1, void* p2, void* p3);
void dw1000_test_rx(void* p1, void* p2, void* p3);
void tsch_handle_if_event(struct net_mgmt_event_callback*, uint32_t, struct net_if*);


/* Private Variables ----------------------------------------------------------------------------- */
NET_L2_DECLARE_PUBLIC(TSCH_L2);

K_THREAD_STACK_DEFINE(tid_update_hyperspace_coords_stack, 2048);
struct k_thread tid_update_hyperspace_coords;

K_THREAD_STACK_DEFINE(tid_mcast_listener_stack, 2048);
struct k_thread tid_mcast_listener;

K_THREAD_STACK_DEFINE(coap_stack, 2048);
bool tid_net_test_running = false;
struct k_thread tid_coap;
static struct net_mgmt_event_callback tsch_iface_down_cb;

const char msg[] = "This is a test.";

void main(void)
{
	#if defined(CONFIG_BOOTLOADER_MCUBOOT)
	boot_write_img_confirmed();
	#endif

	NRF_P0->PIN_CNF[12] =
		(GPIO_PIN_CNF_DIR_Output       << GPIO_PIN_CNF_DIR_Pos)   |
		(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
		(GPIO_PIN_CNF_PULL_Disabled    << GPIO_PIN_CNF_PULL_Pos)  |
		(GPIO_PIN_CNF_DRIVE_S0S1       << GPIO_PIN_CNF_DRIVE_Pos) |
		(GPIO_PIN_CNF_SENSE_Disabled   << GPIO_PIN_CNF_SENSE_Pos);

	const struct device* clock = device_get_binding(DT_LABEL(DT_INST(0, nordic_nrf_clock)));

	if(clock)
	{
		clock_control_on(clock, CLOCK_CONTROL_NRF_SUBSYS_HF);
	}

	NRF_TIMER0->TASKS_STOP  = 1;
	NRF_TIMER0->TASKS_CLEAR = 1;
	NRF_TIMER0->MODE        = TIMER_MODE_MODE_Timer;
	NRF_TIMER0->BITMODE     = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;
	NRF_TIMER0->PRESCALER   = 4;	/* Timer frequency = 16 MHz / (2 ^ 4) = 1 MHz */
	NRF_TIMER0->TASKS_START = 1;

	net_mgmt_init_event_callback(&tsch_iface_down_cb, tsch_handle_if_event, NET_EVENT_IF_DOWN);
	net_mgmt_add_event_callback(&tsch_iface_down_cb);

	loc_allow_beaconing(false);

	// tsch_init();
	tsch_enable();
	// tsch_notify_on_connect(on_connect);
	tsch_start_scan(on_scan);

	struct in6_addr addr = { 0 };
	struct net_if* iface = net_if_get_first_by_type(&NET_L2_GET_NAME(TSCH_L2));
	net_ipv6_addr_create_iid(&addr, net_if_get_link_addr(iface));

	k_thread_create(&tid_update_hyperspace_coords,
		tid_update_hyperspace_coords_stack,
		K_THREAD_STACK_SIZEOF(tid_update_hyperspace_coords_stack),
		update_hyperspace_coords,
		NULL, NULL, NULL,
		K_PRIO_PREEMPT(10), 0, K_NO_WAIT);

	k_thread_name_set(&tid_update_hyperspace_coords, "Send Hyperspace Coord Updates");

	k_thread_create(&tid_mcast_listener,
		tid_mcast_listener_stack,
		K_THREAD_STACK_SIZEOF(tid_mcast_listener_stack),
		mcast_listener,
		NULL, NULL, NULL,
		K_PRIO_PREEMPT(10), 0, K_NO_WAIT);

	k_thread_name_set(&tid_mcast_listener, "Multicast Listener");

	k_thread_create(&tid_coap,
		coap_stack,
		K_THREAD_STACK_SIZEOF(coap_stack),
		coap_test,
		NULL, NULL, NULL,
		1, 0, K_NO_WAIT);

	k_thread_name_set(&tid_coap, "Coap Test");

	while(1)
	{
		k_sleep(K_MSEC(1000));
	}
}


void update_hyperspace_coords(void* p1, void* p2, void* p3)
{
	char json_str[128];

	struct sockaddr_in6 addr6 = { 0 };
	addr6.sin6_family = AF_INET6;
	addr6.sin6_port   = htons(MY_PORT);
	inet_pton(AF_INET6, "fd00::1", &addr6.sin6_addr);

	int s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

	while(1)
	{
		k_sleep(K_MSEC(5000));
		Vec3 loc = loc_current();

		unsigned len = snprintf(json_str, sizeof(json_str), "{\"loc\":[%f,%f,%f],\"bindex\":%d}",
			loc.x, loc.y, loc.z, loc_beacon_index());

		int ret = sendto(s, json_str, len, 0, (struct sockaddr*)&addr6, sizeof(addr6));
		LOG_INF("sent udp HYPR update %d", ret);
	}
}


void mcast_listener(void* p1, void* p2, void* p3)
{
	struct sockaddr_in6 addr6;

	memset(&addr6, 0, sizeof(addr6));
	addr6.sin6_family = AF_INET6;
	addr6.sin6_port   = htons(2201);

	int s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

	if(s < 0)
	{
		LOG_ERR("failed to create mcast UDP socket %d", errno);
	}

	int r = bind(s, (struct sockaddr*)&addr6, sizeof(addr6));

	if(r < 0)
	{
		LOG_ERR("failed to bind mcast UDP socket %d", errno);
	}

	struct sockaddr client_addr;
	socklen_t       client_addr_len;
	uint8_t         request[256];

	while(1)
	{
		client_addr_len = sizeof(client_addr);

		r = recvfrom(s, request, sizeof(request), 0, &client_addr, &client_addr_len);

		if(r < 0)
		{
			LOG_ERR("connection error");
		}
		else
		{
			LOG_INF("RX MCAST");
		}
	}
}


void tsch_handle_if_event(struct net_mgmt_event_callback* cb, uint32_t nm_event, struct net_if* iface)
{
	LOG_INF("TSCH if down");

	tsch_start_scan(on_scan);
}


bool on_scan(Ieee154_Frame* frame)
{
	// const static uint8_t lladdr[] = { 0x02, 0xCF, 0xE5, 0xA0, 0x23, 0x0D, 0x67, 0xAF, };

	const char ssid[] = { 'H','y','p','e','r','s','p','a','c','e' };

	LOG_INF("on scan cb");

	Ieee154_IE ie = ieee154_ie_first(frame);

	for(ie = ieee154_ie_first(frame); ieee154_ie_is_valid(&ie); ieee154_ie_next(&ie))
	{
		if(ieee154_ie_is_hie(&ie) && ieee154_ie_type(&ie) == TSCH_SSID_IE)
		{
			if(ieee154_ie_length(&ie) > 0 && 0 == memcmp(
				ssid,
				ieee154_ie_ptr_content(&ie),
				calc_min_uint(sizeof(ssid), ieee154_ie_length(&ie))))
			{
				return true;
			}
		}
	}

	return false;

	// return memcmp(ieee154_src_addr(frame), lladdr, 8) == 0;
}


void tsch_net_test(void* p1, void* p2, void* p3)
{
	/* Net testing */
	int s, ret;
	// struct sockaddr_in6 bind_addr = { 0 };
	// bind_addr.sin6_family = AF_INET6;
	// bind_addr.sin6_addr = in6_addr_my;
	// bind_addr.sin6_port = htons(MY_PORT);

	// struct net_if* iface = net_if_get_first_by_type(&NET_L2_GET_NAME(TSCH_L2));
	// net_if_ipv6_addr_add(iface, &in6_addr_my, NET_ADDR_MANUAL, 0);
	// net_if_ipv6_maddr_add(iface, &in6_addr_mcast);

	s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if(s < 0)
	{
		LOG_ERR("Error: %d", s);
	}

	struct sockaddr_in6 addr6 = { 0 };
	addr6.sin6_family = AF_INET6;
	addr6.sin6_port   = htons(MY_PORT);
	inet_pton(AF_INET6, "fd00::cf:e5a0:230d:67af", &addr6.sin6_addr);

	ret = connect(s, (struct sockaddr*)&addr6, sizeof(addr6));
	if(ret < 0)
	{
		LOG_ERR("connect error: %d", ret);
	}

	while(1)
	{
		k_sleep(K_MSEC(2000));
		// ret = sendto(s, msg, sizeof(msg), 0, (struct sockaddr*)&addr6, sizeof(addr6));
		LOG_DBG("send udp %d", ret);
	}

	close(s);



	// // struct sockaddr_in6 their_addr = { 0 };
	// // their_addr.sin6_family = AF_INET6;
	// // their_addr.sin6_addr = in6_addr_them;
	// // their_addr.sin6_port = htons(MY_PORT);
	// // ret = connect(s, (struct sockaddr*)&in6_addr_them, sizeof(in6_addr_them));
	// ret = connect(s, (struct sockaddr*)&their_addr, sizeof(their_addr));
	// if(ret < 0)
	// {
	// 	LOG_ERR("Error: %d", ret);
	// }

	// ret = sendto(s, large_text, sizeof(large_text), 0, &their_addr, sizeof(their_addr));

	// // __syscall ssize_t zsock_sendto(int sock, const void *buf, size_t len,
	// // 		       int flags, const struct sockaddr *dest_addr,
	// // 		       socklen_t addrlen);
	// if(ret < 0)
	// {
	// 	LOG_ERR("Error: %d", ret);
	// }

	// close(s);

	// ret = bind(s, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
	// if(ret < 0)
	// {
	// 	LOG_ERR("Error: %d", ret);
	// }

	// ret = sendto(s, )

	// struct net_contet* udp_recv;
	// ret = net_context_get(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, &udp_recv);
	// if(ret < 0)
	// {
	// 	LOG_ERR("Cannot get network contet for IPv6 UDP (%d)", ret);
	// }

	// ret = net_context_bind(udp_recv, (struct sockaddr*)&addr6, sizeof(struct sockaddr_in6));
	// if(ret < 0)
	// {
	// 	LOG_ERR("Cannot bind IPv6 UDP port %d (%d)", ntohs(addr6.sin6_port), ret);
	// }
}


/******************************************* END OF FILE *******************************************/
