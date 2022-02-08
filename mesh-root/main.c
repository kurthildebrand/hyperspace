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
 * @brief
 * @desc		RPI Header Pin-out
 *
 * 				NRF        | DWM   | RPI Function     | RPI   | RPI Function    | DWM   | NRF
 * 				-----------|-------|------------------|-------|-----------------|-------|-----------
 * 				           |       |           +3.3v  |  1 2  | +5v             |       |
 * 				P0.15      | J1.23 | 12C1 SDA  GPIO2  |  3 4  | +5v             |       |
 * 				P0.08      | J1.25 | 12C1 SCL  GPIO3  |  5 6  | GND             |       |
 * 				           |       |           GPIO4  |  7 8  | GPIO14          | J1.18 | P0.11
 * 				           |       |           GND    |  9 10 | GPIO15          | J1.20 | P0.05/AIN3
 * 				           |       |           GPIO17 | 11 12 | GPIO18 RESET    | J1.33 | P0.21
 * 				           |       |           GPIO27 | 13 14 | GND             |       |
 * 				P0.26      | J1.19 |           GPIO22 | 15 16 | GPIO23          |       |
 * 				           |       |           +3.3v  | 17 18 | GPIO24          |       |
 * 				P0.06      | J1.27 | SPI0 MOSI GPIO10 | 19 20 | GND             |       |
 * 				P0.07      | J1.26 | SPI0 MISO GPIO9  | 21 22 | GPIO25 SPI0 CS0 |       |
 * 				P0.04/AIN2 | J1.28 | SPI0 SCLK GPIO11 | 23 24 | GPIO8  SPI0 CS1 | J1.29 | P0.03/AIN1
 * 				           |       |           GND    | 25 26 | GPIO7           |       |
 *
 * 				J7 Header Pin-out
 *
 * 					NRF    | DWM   | J7 Header   | DWM   | NRF
 * 					-------|-------|-------------|-------|-------
 * 					DW_GP0 | J1.21 |    1   2    | J1.22 | DW_GP0
 * 					P0.12  | J1.6  |    3   4    | J1.13 | P0.27
 * 					P0.29  | J1.14 |    5   6    | J1.15 | P0.28
 * 					P0.23  | J1.16 |    7   8    | J1.17 | P0.13
 *
 * 				NRF Pin-out
 *
 * 					P0.00/XL1       |           |   32.768 kHz  |
 * 					P0.01/XL2       |           |   32.768 kHz  |
 * 					P0.02/AIN0      |   J1.32   |   BT_WAKE_UP  |
 * 					P0.03/AIN1      |   J1.29   |   CS_RPI      |   RPi Chip Select
 * 					P0.04/AIN2      |   J1.28   |   SPI1_CLK    |   RPi SPI CLK
 * 					P0.05/AIN3      |   J1.20   |   RXD         |   UART TXD
 * 					P0.06           |   J1.27   |   SPI1_MOSI   |   RPi SPI MOSI
 * 					P0.07           |   J1.26   |   SPI1_MISO   |   RPi SPI MISO
 * 					P0.08           |   J1.25   |   SCL_RPI     |   RPi I2C SCL
 * 					P0.09           |   J1.5    |               |
 * 					P0.10           |   J1.4    |               |
 * 					P0.11           |   J1.18   |   TXD         |   UART RXD
 * 					P0.12           |   J1.6    |   M_PIN6      |
 * 					P0.13           |   J1.17   |   M_PIN17     |
 * 					P0.14           |   J1.7    |   RED LED     |
 * 					P0.15           |   J1.23   |   SDA_RPI     |   RPi I2C SDA
 * 					P0.16           |           |   SPI1_CLK    |   DW1000 SPI CLK
 * 					P0.17           |           |   DW_CS       |   DW1000 Chip Select
 * 					P0.18           |           |   SPI1_MISO   |   DW1000 SPI MISO
 * 					P0.19           |           |   DW_IRQ      |
 * 					P0.20           |           |   SPI1_MOSI   |   DW1000 SPI MOSI
 * 					P0.21/RESET     |   J1.33   |               |
 * 					P0.22           |   J1.8    |   RED LED     |
 * 					P0.23           |   J1.16   |               |   M_PIN16
 * 					P0.24           |           |   DW_RST      |
 * 					P0.25           |           |   IRQ_ACC     |
 * 					P0.26           |   J1.19   |   READY       |   GPIO_RPI
 * 					P0.27           |   J1.13   |               |   M_PIN13
 * 					P0.28/AIN4      |   J1.15   |   I2C_SCL     |   M_PIN15
 * 					P0.29/AIN5      |   J1.14   |   I2C_SDA     |   M_PIN14
 * 					P0.30/AIN6      |   J1.10   |   GREEN LED   |
 * 					P0.31/AIN7      |   J1.9    |   BLUE LED    |
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

#include <net/socket.h>
#include <nrfx/hal/nrf_ppi.h>
#include <nrfx/hal/nrf_gpiote.h>
#include <nrfx/hal/nrf_timer.h>
#include <stdint.h>
#include <stdio.h>

// #include "coap_test.h"
#include "coap_server.h"
#include "fw_version.h"
#include "hyperspace.h"
#include "location.h"
#include "spis_if.h"
#include "tsch.h"

#include "logging/log.h"
LOG_MODULE_REGISTER(mesh_root, LOG_LEVEL_INF);
// LOG_MODULE_REGISTER(mesh_root, LOG_LEVEL_DBG);


/* Inline Function Instances --------------------------------------------------------------------- */
/* Private Constants ----------------------------------------------------------------------------- */
// #define MCAST_IPV6_ADDR {{{ 0xff, 0x02, 0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0x20, 0, 0, 0x1 }}}
// #define MY_IPV6_ADDR    {{{ 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0x20, 0, 0, 0x2 }}}
// #define THEIR_IPV6_ADDR {{{ 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0x20, 0, 0, 0x1 }}}
// #define MY_PORT (4242)
#define MY_PORT (2200)


/* Private Types --------------------------------------------------------------------------------- */
/* Private Functions ----------------------------------------------------------------------------- */
void update_hyperspace_coords(void* p1, void* p2, void* p3);
void mcast_listener(void* p1, void* p2, void* p3);
void net_recv_test(void* p1, void* p2, void* p3);
void gpio_test    (void* p1, void* p2, void* p3);


/* Private Variables ----------------------------------------------------------------------------- */
NET_L2_DECLARE_PUBLIC(SPIS_L2);
NET_L2_DECLARE_PUBLIC(TSCH_L2);

// K_THREAD_DEFINE(tid_gpio_test, 1024, gpio_test, 0, 0, 0, 1, 0, K_NO_WAIT);
// struct in6_addr in6_addr_mcast = MCAST_IPV6_ADDR;
// struct in6_addr in6_addr_my    = MY_IPV6_ADDR;
// struct in6_addr in6_addr_them  = THEIR_IPV6_ADDR;

K_THREAD_STACK_DEFINE(tid_update_hyperspace_coords_stack, 2048);
struct k_thread tid_update_hyperspace_coords;

K_THREAD_STACK_DEFINE(tid_mcast_listener_stack, 2048);
struct k_thread tid_mcast_listener;

K_THREAD_STACK_DEFINE(coap_stack, 2048);
struct k_thread tid_coap;

// K_THREAD_STACK_DEFINE(tid_net_test_recv_stack, 2048);
// struct k_thread tid_net_test_recv;

void main(void)
{
	LOG_INF("Firmware Version: " FW_VERSION);

	#if defined(CONFIG_BOOTLOADER_MCUBOOT)
	boot_write_img_confirmed();
	#endif

	NRF_P0->PIN_CNF[12] =
		(GPIO_PIN_CNF_DIR_Output       << GPIO_PIN_CNF_DIR_Pos)   |
		(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
		(GPIO_PIN_CNF_PULL_Disabled    << GPIO_PIN_CNF_PULL_Pos)  |
		(GPIO_PIN_CNF_DRIVE_S0S1       << GPIO_PIN_CNF_DRIVE_Pos) |
		(GPIO_PIN_CNF_SENSE_Disabled   << GPIO_PIN_CNF_SENSE_Pos);

	int ret = 0;
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

	loc_allow_beaconing(true);

	// tsch_init();
	tsch_enable();
	tsch_create_network();

	// struct net_if* iface = net_if_get_first_by_type(&NET_L2_GET_NAME(SPIS_L2));
	struct sockaddr_in6 addr6 = { 0 };
	addr6.sin6_family = AF_INET6;
	addr6.sin6_port   = htons(MY_PORT);
	inet_pton(AF_INET6, "fd00::1", &addr6.sin6_addr);

	// struct net_if* iface = net_if_get_first_by_type(&NET_L2_GET_NAME(SPIS_L2));
	// net_if_ipv6_addr_add(iface, &in6_addr_my, NET_ADDR_MANUAL, 0);
	// net_if_ipv6_maddr_add(iface, &in6_addr_mcast);

	// struct in6_addr prefix;
	// prefix.s6_addr[0] = 0xFD;
	// prefix.s6_addr[1] = 0x00;
	// struct net_if* iface = net_if_get_first_by_type(&NET_L2_GET_NAME(TSCH_L2));
	// net_if_ipv6_prefix_add(iface, &prefix, 8, -1u);

	k_thread_create(&tid_update_hyperspace_coords,
		tid_update_hyperspace_coords_stack,
		K_THREAD_STACK_SIZEOF(tid_update_hyperspace_coords_stack),
		update_hyperspace_coords,
		NULL, NULL, NULL,
		K_PRIO_PREEMPT(1), 0, K_NO_WAIT);

	k_thread_name_set(&tid_update_hyperspace_coords, "Send Hyperspace Coord Updates");

	k_thread_create(&tid_mcast_listener,
		tid_mcast_listener_stack,
		K_THREAD_STACK_SIZEOF(tid_mcast_listener_stack),
		mcast_listener,
		NULL, NULL, NULL,
		K_PRIO_PREEMPT(1), 0, K_NO_WAIT);

	k_thread_name_set(&tid_mcast_listener, "Multicast Listener");

	k_thread_create(&tid_coap,
		coap_stack,
		K_THREAD_STACK_SIZEOF(coap_stack),
		// coap_test,
		coap_server,
		NULL, NULL, NULL,
		K_PRIO_PREEMPT(10), 0, K_NO_WAIT);

	k_thread_name_set(&tid_coap, "CoAP");

	// k_thread_create(&tid_net_test_recv,
	// 	tid_net_test_recv_stack,
	// 	K_THREAD_STACK_SIZEOF(tid_net_test_recv_stack),
	// 	net_recv_test,
	// 	NULL, NULL, NULL,
	// 	1, 0, K_NO_WAIT);

	// k_thread_name_set(&tid_net_test_recv, "Net Test Recv");

	// int s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	// // int ret = connect(s, (struct sockaddr *)&addr6, sizeof(addr6));
	// // int ret = bind(s, (struct sockaddr*)&addr6, sizeof(addr6));
	// if(ret < 0)
	// {
	// 	LOG_ERR("Error: %d", ret);
	// }

	// const char msg[] = "Hello world!";

	while(1)
	{
		k_sleep(K_MSEC(5000));
		// k_sleep(10000);
		// ret = sendto(s, msg, sizeof(msg), 0, (struct sockaddr*)&addr6, sizeof(addr6));
		// LOG_DBG("send udp %d", ret);
	}
}


void json_write_float(char* buf, unsigned size, float x)
{
	if(isnan(x))
	{
		snprintf(buf, size, "\"NaN\"");
	}
	else if(isinf(x))
	{
		snprintf(buf, size, "\"Infinity\"");
	}
	else
	{
		snprintf(buf, size, "%f", x);
	}
}


void update_hyperspace_coords(void* p1, void* p2, void* p3)
{
	char json_str[128];
	char xstr[16];
	char ystr[16];
	char zstr[16];

	struct sockaddr_in6 addr6 = { 0 };
	addr6.sin6_family = AF_INET6;
	addr6.sin6_port   = htons(MY_PORT);
	inet_pton(AF_INET6, "fd00::1", &addr6.sin6_addr);

	int s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

	while(1)
	{
		k_sleep(K_MSEC(5000));
		Vec3 loc = loc_current();

		json_write_float(xstr, sizeof(xstr), loc.x);
		json_write_float(ystr, sizeof(ystr), loc.y);
		json_write_float(zstr, sizeof(zstr), loc.z);

		// unsigned len = snprintf(json_str, sizeof(json_str), "{\"loc\":[%f,%f,%f],\"bindex\":%d}",
		// 	loc.x, loc.y, loc.z, loc_beacon_index());

		unsigned len = snprintf(json_str, sizeof(json_str), "{\"loc\":[%s,%s,%s],\"bindex\":%d}",
			xstr, ystr, zstr, loc_beacon_index());

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


void net_recv_test(void* p1, void* p2, void* p3)
{
	uint8_t buf[256];

	int s, ret;
	// struct sockaddr_in6 bind_addr = { 0 };
	// bind_addr.sin6_family = AF_INET6;
	// bind_addr.sin6_addr   = in6_addr_my;
	// bind_addr.sin6_port   = htons(MY_PORT);

	// struct net_if* iface = net_if_get_first_by_type(&NET_L2_GET_NAME(SPIS_L2));
	// net_if_ipv6_addr_add(iface, &in6_addr_my, NET_ADDR_MANUAL, 0);
	// net_if_ipv6_maddr_add(iface, &in6_addr_mcast);

	s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if(s < 0)
	{
		LOG_ERR("Error: %d", s);
	}

	struct sockaddr_in6 addr6;
	memset(&addr6.sin6_addr, 0, sizeof(addr6.sin6_addr));
	addr6.sin6_family = AF_INET6;
	addr6.sin6_port   = htons(MY_PORT);
	ret = bind(s, (struct sockaddr*)&addr6, sizeof(addr6));
	if(ret < 0)
	{
		LOG_ERR("Error: %d", ret);
	}

	int received;
	struct sockaddr client_addr;
	socklen_t client_addr_len;

	while(1)
	{
		client_addr_len = sizeof(client_addr);

		received = recvfrom(s, buf, sizeof(buf), 0, &client_addr, &client_addr_len);

		if(received < 0)
		{
			LOG_ERR("Connection error: %d", errno);
		}
		else
		{
			LOG_DBG("UDP packet received (len = %d)", received);
		}
	}
}


/******************************************* END OF FILE *******************************************/
