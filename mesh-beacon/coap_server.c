/************************************************************************************************//**
 * @file		coap_server.c
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
#include <logging/log.h>
// LOG_MODULE_REGISTER(net_coap_server_sample, LOG_LEVEL_DBG);
LOG_MODULE_REGISTER(net_coap_server_sample, LOG_LEVEL_ERR);

#include <drivers/gpio.h>
#include <errno.h>
#include <power/reboot.h>
#include <sys/byteorder.h>
#include <sys/crc.h>
#include <sys/printk.h>
#include <sys/reboot.h>
#include <zephyr.h>

#include <dfu/flash_img.h>
#include <dfu/mcuboot.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>

#include <net/socket.h>
#include <net/net_mgmt.h>
#include <net/net_ip.h>
#include <net/udp.h>
#include <net/coap.h>
#include <net/coap_link_format.h>

#include "buffer.h"
#include "fw_version.h"
#include "ipv6.h"
#include "net_private.h"


/* Inline Function Instances --------------------------------------------------------------------- */
/* Private Macros -------------------------------------------------------------------------------- */
NET_L2_DECLARE_PUBLIC(TSCH_L2);

#define MAX_COAP_MSG_LEN				(256)
#define MY_COAP_PORT					(5683)
#define BLOCK_WISE_TRANSFER_SIZE_GET	(2048)
#define NUM_OBSERVERS					(3)
#define NUM_PENDINGS					(3)
#define ALL_NODES_LOCAL_COAP_MCAST		{{{ 0xFF,0x02,0,0,0,0,0,0,0,0,0,0,0,0,0,0xFD }}}

#define GET_BLOCK_NUM(v)				((v) >> 4)
#define GET_BLOCK_SIZE(v)				(((v) & 0x7))
#define GET_MORE(v)						(!!((v) & 0x08))


/* Private Types --------------------------------------------------------------------------------- */
typedef struct {
	char     vstr[16];	/* Version string */
	unsigned vlen;		/* Version string len */
} Ota;


/* Private Functions ----------------------------------------------------------------------------- */
static int  coap_join_mcast_group(void);
static int  coap_start_server    (void);
static void retransmit_request   (struct k_work*);
static void update_counter       (struct k_work*);
static void process_coap_request (uint8_t*, uint16_t,struct sockaddr*,socklen_t);
static int  send_coap_reply      (struct coap_packet*, const struct sockaddr*, socklen_t);
static struct coap_resource* find_resource_by_observer(struct coap_resource*,struct coap_observer*);

static int  fw_put    (struct coap_resource*, struct coap_packet*, struct sockaddr*, socklen_t);
static int  fw_get    (struct coap_resource*, struct coap_packet*, struct sockaddr*, socklen_t);
static int  reboot_put(struct coap_resource*, struct coap_packet*, struct sockaddr*, socklen_t);
static void ota_handle(Ota*, struct coap_packet*, struct coap_block_context*);
static int  led_get   (struct coap_resource*, struct coap_packet*, struct sockaddr*, socklen_t);
static int  led_put   (struct coap_resource*, struct coap_packet*, struct sockaddr*, socklen_t);

static int well_known_core_get(
	struct coap_resource *resource,
	struct coap_packet *request,
	struct sockaddr *addr,
	socklen_t addr_len);


/* Private Variables ----------------------------------------------------------------------------- */
static int sock;
static int obs_counter;
static const uint8_t plain_text_format;
static struct coap_observer    observers[NUM_OBSERVERS];
static struct coap_pending     pendings[NUM_PENDINGS];
static struct k_work_delayable observer_work;
static struct coap_resource*   resource_to_notify;
static struct k_work_delayable retransmit_work;
static Ota ota;
static bool green_led;

static const char* fw_path[]     = { "firmware", 0 };
static const char* reboot_path[] = { "reboot",   0 };
static const char* led_path[]    = { "led",      0 };

static struct coap_resource resources[] = {
	{
		.path = COAP_WELL_KNOWN_CORE_PATH,
		.get  = well_known_core_get,
	},
	{
		.path = fw_path,
		.put  = fw_put,
		.get  = fw_get,
	},
	{
		.path = reboot_path,
		.put  = reboot_put,
	},
	{
		.path = led_path,
		.get  = led_get,
		.put  = led_put,
	},
	{ },
};


/* coap_server **********************************************************************************//**
 * @brief		COAP server thread. Listens for any COAP packet on port 5683. */
void coap_server(void* p1, void* p2, void* p3)
{
	int r;
	LOG_DBG("Start CoAP Server");

	struct mcuboot_img_header header;
	boot_read_bank_header(FLASH_AREA_ID(image_0), &header, sizeof(header));

	/* Init OTA */
	ota.vlen = string_copy(MAKE_STRING(FW_VERSION), ota.vstr, sizeof(ota.vstr));

	/* Setup led */
	const struct device* dev = device_get_binding(DT_GPIO_LABEL(DT_ALIAS(led0), gpios));
	gpio_pin_configure(dev, DT_GPIO_PIN(DT_ALIAS(led0), gpios), GPIO_OUTPUT_ACTIVE | DT_GPIO_FLAGS(LED0_NODE, gpios));
	gpio_pin_set(dev, DT_GPIO_PIN(DT_ALIAS(led0), gpios), 1);
	green_led = false;

	r = coap_join_mcast_group();
	if(r != 0)
	{
		goto quit;
	}

	r = coap_start_server();
	if(r != 0)
	{
		goto quit;
	}

	uint8_t request[MAX_COAP_MSG_LEN];

	while(1)
	{
		struct sockaddr client_addr;
		socklen_t client_addr_len = sizeof(client_addr);

		r = recvfrom(sock, request, sizeof(request), 0, &client_addr, &client_addr_len);
		if(r < 0)
		{
			LOG_ERR("connection error %d", errno);
			goto quit;
		}

		process_coap_request(request, r, &client_addr, client_addr_len);
	}

	LOG_DBG("done");
	return;

	quit:
		LOG_ERR("quit");
}


/* coap_join_mcast_group ************************************************************************//**
 * @brief		*/
static int coap_join_mcast_group(void)
{
	static struct sockaddr_in6 mcast_addr = {
		.sin6_family = AF_INET6,
		.sin6_addr   = ALL_NODES_LOCAL_COAP_MCAST,
		.sin6_port   = htons(MY_COAP_PORT),
	};

	struct net_if* iface = net_if_get_first_by_type(&NET_L2_GET_NAME(TSCH_L2));

	if(!iface)
	{
		LOG_ERR("could not get TSCH interface");
		return -1;
	}

	struct net_if_mcast_addr* mcast = net_if_ipv6_maddr_add(iface, &mcast_addr.sin6_addr);

	if(!mcast)
	{
		LOG_ERR("could not add multicast address to interface");
		return -1;
	}

	return 0;
}


/* coap_start_server ****************************************************************************//**
 * @brief		*/
static int coap_start_server(void)
{
	int r;
	struct sockaddr_in6 addr6;

	memset(&addr6, 0, sizeof(addr6));
	addr6.sin6_family = AF_INET6;
	addr6.sin6_port   = htons(MY_COAP_PORT);

	sock = socket(addr6.sin6_family, SOCK_DGRAM, IPPROTO_UDP);
	if(sock < 0)
	{
		LOG_ERR("failed to create udp socket %d", errno);
		return -errno;
	}

	r = bind(sock, (struct sockaddr *)&addr6, sizeof(addr6));
	if(r < 0)
	{
		LOG_ERR("failed to bind udp socket %d", errno);
		return -errno;
	}

	k_work_init_delayable(&retransmit_work, retransmit_request);
	k_work_init_delayable(&observer_work,   update_counter);

	return 0;
}


/* retransmit_request ***************************************************************************//**
 * @brief		*/
static void retransmit_request(struct k_work* work)
{
	struct coap_pending* pending = coap_pending_next_to_expire(pendings, NUM_PENDINGS);

	if(!pending)
	{
		return;
	}

	if(!coap_pending_cycle(pending))
	{
		k_free(pending->data);
		coap_pending_clear(pending);
		return;
	}

	k_work_schedule(&retransmit_work, K_MSEC(pending->timeout));
}


/* update_counter *******************************************************************************//**
 * @brief		*/
static void update_counter(struct k_work* work)
{
	obs_counter++;

	if(resource_to_notify)
	{
		coap_resource_notify(resource_to_notify);
	}

	k_work_schedule(&observer_work, K_SECONDS(5));
}


/* process_coap_request *************************************************************************//**
 * @brief		*/
static void process_coap_request(
	uint8_t* data,
	uint16_t data_len,
	struct sockaddr* client_addr,
	socklen_t client_addr_len)
{
	struct coap_packet   request;
	struct coap_pending* pending;
	struct  coap_option   options[8] = { 0 };
	uint8_t opt_num = sizeof(options) / sizeof(options[0]);
	uint8_t type;

	int r = coap_packet_parse(&request, data, data_len, options, opt_num);

	if(r < 0)
	{
		LOG_ERR("invalid data received %d", r);
		return;
	}

	type    = coap_header_get_type(&request);
	pending = coap_pending_received(&request, pendings, NUM_PENDINGS);

	if(!pending)
	{
		goto not_found;
	}

	/* Clear CoAP pending request */
	if(type == COAP_TYPE_ACK)
	{
		k_free(pending->data);
		coap_pending_clear(pending);
	}

	return;

	not_found:
		if(type == COAP_TYPE_RESET)
		{
			struct coap_observer* o = coap_find_observer_by_addr(
				observers, NUM_OBSERVERS, client_addr);

			if(!o)
			{
				LOG_ERR("observer not found");
				goto end;
			}

			struct coap_resource* r = find_resource_by_observer(resources, o);

			if(!r)
			{
				LOG_ERR("observer found but resource not found");
				goto end;
			}
		}

	end:
		r = coap_handle_request(&request, resources, options, opt_num, client_addr, client_addr_len);

		if(r < 0)
		{
			LOG_WRN("no handler for such request %d", r);
		}
}


/* find_resource_by_observer ********************************************************************//**
 * @brief		*/
static struct coap_resource* find_resource_by_observer(
	struct coap_resource* resources,
	struct coap_observer* o)
{
	struct coap_resource* r;

	for(r = resources; r && r->path; r++)
	{
		sys_snode_t *node;

		SYS_SLIST_FOR_EACH_NODE(&r->observers, node)
		{
			if(&o->list == node)
			{
				return r;
			}
		}
	}

	return 0;
}


/* send_coap_reply ******************************************************************************//**
 * @brief		Transmits a COAP packet to the specified address. */
static int send_coap_reply(struct coap_packet *cpkt, const struct sockaddr *addr, socklen_t addr_len)
{
	int r;

	net_hexdump("Response", cpkt->data, cpkt->offset);

	r = sendto(sock, cpkt->data, cpkt->offset, 0, addr, addr_len);
	if (r < 0) {
		LOG_ERR("Failed to send %d", errno);
		r = -errno;
	}

	return r;
}




// ----------------------------------------------------------------------------------------------- //
// CoAP Application                                                                                //
// ----------------------------------------------------------------------------------------------- //
static int well_known_core_get(
	struct coap_resource *resource,
	struct coap_packet *request,
	struct sockaddr *addr,
	socklen_t addr_len)
{
	struct coap_packet response;
	uint8_t data[MAX_COAP_MSG_LEN];

	int r = coap_well_known_core_get(resource, request, &response, data, MAX_COAP_MSG_LEN);
	if (r < 0) {
		goto end;
	}

	r = send_coap_reply(&response, addr, addr_len);

	end:
		return r;
}


/* fw_put ***************************************************************************************//**
 * @brief		*/
static int fw_put(
	struct coap_resource* resource,
	struct coap_packet* request,
	struct sockaddr* addr,
	socklen_t addr_len)
{
	static struct coap_block_context ctx;
	struct coap_packet response;
	uint8_t token[COAP_TOKEN_MAX_LEN];
	uint16_t len;

	int r = coap_get_option_int(request, COAP_OPTION_BLOCK1);
	if(r < 0) {
		return -EINVAL;
	}

	bool last_block = !GET_MORE(r);
	// unsigned num  = GET_BLOCK_NUM(r);
	// unsigned size = GET_BLOCK_SIZE(r);

	/* Initialize block context upon the arrival of first block */
	if(GET_BLOCK_NUM(r) == 0) {
		coap_block_transfer_init(&ctx, COAP_BLOCK_128, 0);
	}

	r = coap_update_from_block(request, &ctx);
	if(r < 0) {
		LOG_ERR("invalid block size option from request");
		return -EINVAL;
	}

	const uint8_t* payload = coap_packet_get_payload(request, &len);
	if(!last_block && payload == NULL) {
		LOG_ERR("packet without payload");
		return -EINVAL;
	}

	LOG_INF("[ctx] current %zu block_size %u total_size %zu",
		ctx.current, coap_block_size_to_bytes(ctx.block_size),
		ctx.total_size);

	uint8_t  code = coap_header_get_code(request);
	uint8_t  type = coap_header_get_type(request);
	uint16_t id   = coap_header_get_id(request);
	uint8_t  tkl  = coap_header_get_token(request, token);

	LOG_INF("type: %u code %u id %u", type, code, id);

	/* Handle the packet */
	ota_handle(&ota, request, &ctx);

	if (!last_block) {
		code = COAP_RESPONSE_CODE_CONTINUE;
	} else {
		code = COAP_RESPONSE_CODE_CHANGED;
	}

	uint8_t data[MAX_COAP_MSG_LEN];

	r = coap_packet_init(&response, data, MAX_COAP_MSG_LEN,
		COAP_VERSION_1, COAP_TYPE_ACK, tkl, token, code, id);
	if (r < 0) {
		goto end;
	}

	r = coap_append_block1_option(&response, &ctx);
	if (r < 0) {
		LOG_ERR("Could not add Block1 option to response");
		goto end;
	}

	r = send_coap_reply(&response, addr, addr_len);

	end:
		return r;
}


/* fw_get ***************************************************************************************//**
 * @brief		*/
static int fw_get(
	struct coap_resource* resource,
	struct coap_packet* request,
	struct sockaddr* addr,
	socklen_t addr_len)
{
	struct coap_packet response;
	uint8_t  payload[40];
	uint8_t  token[8];
	uint8_t  code = coap_header_get_code (request);
	uint8_t  type = coap_header_get_type (request);
	uint16_t id   = coap_header_get_id   (request);
	uint8_t  tkl  = coap_header_get_token(request, token);
	int r;

	if(type == COAP_TYPE_CON) {
		type = COAP_TYPE_ACK;
	} else {
		type = COAP_TYPE_NON_CON;
	}

	uint8_t data[MAX_COAP_MSG_LEN];

	r = coap_packet_init(&response, data, MAX_COAP_MSG_LEN, 1, type, tkl, token,
		COAP_RESPONSE_CODE_CONTENT, id);
	if(r < 0) {
		goto end;
	}

	r = coap_packet_append_option(&response, COAP_OPTION_CONTENT_FORMAT,
		&plain_text_format, sizeof(plain_text_format));
	if(r < 0) {
		goto end;
	}

	r = coap_packet_append_payload_marker(&response);
	if(r < 0) {
		goto end;
	}

	r = snprintk(payload, sizeof(payload), "%s,%s,%s", FW_MANUF, FW_BOARD, FW_VERSION);
	if(r < 0) {
		goto end;
	}

	r = coap_packet_append_payload(&response, payload, strlen(payload));
	if(r < 0) {
		goto end;
	}

	r = send_coap_reply(&response, addr, addr_len);

	end:
		return r;
}


/* ota_handle ***********************************************************************************//**
 * @brief		Handles an OTA update packet. */
static void ota_handle(Ota* ota, struct coap_packet* request, struct coap_block_context* ctx)
{
	struct coap_option opts[1];

	int r = coap_find_options(request, COAP_OPTION_URI_QUERY, opts, sizeof(opts) / sizeof(opts[0]));
	if(r < 0)
	{
		return;
	}

	String it         = make_string_len(opts[0].value, opts[0].len);
	String manuf      = string_token(&it, ",");
	String board      = string_token(&it, ",");
	String version    = string_token(&it, ",");
	bool   last_block = false;

	r = coap_get_option_int(request, COAP_OPTION_BLOCK1);

	if(r >= 0)
	{
		last_block = !GET_MORE(r);
	}

	if(!string_equal(manuf, MAKE_STRING(FW_MANUF)) || !string_equal(board, MAKE_STRING(FW_BOARD)))
	{
		return;
	}

	/* Get payload */
	uint16_t len;
	const uint8_t* payload = coap_packet_get_payload(request, &len);

	if(!last_block && !payload)
	{
		LOG_ERR("packet without payload");
		return;
	}

	const struct flash_area* fa;
	flash_area_open(FLASH_AREA_ID(image_1), &fa);

	r = string_cmp(version, MAKE_STRING(ota->vstr, ota->vlen));

	/* Erase flash if packet's version is newer than image 1's firmware version */
	if(r > 0)
	{
		flash_area_erase(fa, 0, fa->fa_size);
		ota->vlen = string_copy(version, ota->vstr, sizeof(ota->vstr));
	}

	/* Write flash if packet's version is newer or equal to image 1's firmware version (r >= 0) and
	 * greater than the current image's firmware (FW_VERSION) */
	if(r >= 0 && string_cmp(version, MAKE_STRING(FW_VERSION)) > 0)
	{
		flash_area_write(fa, ctx->current, payload, len);
	}

	flash_area_close(fa);
}


/* reboot_put ***********************************************************************************//**
 * @brief		*/
static int reboot_put(
	struct coap_resource* resource,
	struct coap_packet* request,
	struct sockaddr* addr,
	socklen_t addr_len)
{
	struct coap_option opts[1] = { 0 };

	int r = coap_find_options(request, COAP_OPTION_URI_QUERY, opts, sizeof(opts) / sizeof(opts[0]));
	if(r < 0) {
		return r;
	}

	String param = make_string_len(opts[0].value, opts[0].len);

	if(string_equal(param, MAKE_STRING("upgrade=1")))
	{
		boot_request_upgrade(BOOT_UPGRADE_TEST);
	}

	sys_reboot(SYS_REBOOT_COLD);
}


/* led_get **************************************************************************************//**
 * @brief		*/
static int led_get(
	struct coap_resource* resource,
	struct coap_packet* request,
	struct sockaddr* addr,
	socklen_t addr_len)
{
	struct coap_packet response;
	uint8_t  payload[40];
	uint8_t  token[8];
	uint8_t  code = coap_header_get_code (request);
	uint8_t  type = coap_header_get_type (request);
	uint16_t id   = coap_header_get_id   (request);
	uint8_t  tkl  = coap_header_get_token(request, token);
	uint8_t* data;
	int r;

	LOG_INF("*******");
	LOG_INF("type: %u code %u id %u", type, code, id);
	LOG_INF("*******");

	if(type == COAP_TYPE_CON) {
		type = COAP_TYPE_ACK;
	} else {
		type = COAP_TYPE_NON_CON;
	}

	data = k_malloc(MAX_COAP_MSG_LEN);
	if(!data) {
		return -ENOMEM;
	}

	r = coap_packet_init(&response, data, MAX_COAP_MSG_LEN, 1, type, tkl, token,
		COAP_RESPONSE_CODE_CONTENT, id);
	if(r < 0) {
		goto end;
	}

	r = coap_packet_append_option(&response, COAP_OPTION_CONTENT_FORMAT,
		&plain_text_format, sizeof(plain_text_format));
	if(r < 0) {
		goto end;
	}

	r = coap_packet_append_payload_marker(&response);
	if(r < 0) {
		goto end;
	}

	if(green_led) {
		r = snprintk(payload, sizeof(payload), "1");
	} else {
		r = snprintk(payload, sizeof(payload), "0");
	}

	if(r < 0) {
		goto end;
	}

	r = coap_packet_append_payload(&response, payload, strlen(payload));
	if(r < 0) {
		goto end;
	}

	r = send_coap_reply(&response, addr, addr_len);

	end:
		k_free(data);
		return r;
}


/* led_put **************************************************************************************//**
 * @brief		*/
static int led_put(
	struct coap_resource* resource,
	struct coap_packet* request,
	struct sockaddr* addr,
	socklen_t addr_len)
{
	struct coap_packet response;
	const uint8_t* payload;
	uint16_t payload_len;
	uint8_t  token[8];
	uint8_t  code = coap_header_get_code (request);
	uint8_t  type = coap_header_get_type (request);
	uint16_t id   = coap_header_get_id   (request);
	uint8_t  tkl  = coap_header_get_token(request, token);
	int r;

	LOG_INF("*******");
	LOG_INF("type: %u code %u id %u", type, code, id);
	LOG_INF("*******");

	payload = coap_packet_get_payload(request, &payload_len);
	if(payload) {
		const struct device* dev = device_get_binding(DT_GPIO_LABEL(DT_ALIAS(led0), gpios));

		if(strncmp(payload, "1", payload_len) == 0) {
			green_led = true;
			gpio_pin_set(dev, DT_GPIO_PIN(DT_ALIAS(led0), gpios), 1);
		} else if(strncmp(payload, "0", payload_len) == 0) {
			green_led = false;
			gpio_pin_set(dev, DT_GPIO_PIN(DT_ALIAS(led0), gpios), 0);
		}

		net_hexdump("PUT Payload", payload, payload_len);
	}

	if(type == COAP_TYPE_CON) {
		type = COAP_TYPE_ACK;
	} else {
		type = COAP_TYPE_NON_CON;
	}

	uint8_t* data = k_malloc(MAX_COAP_MSG_LEN);
	if(!data) {
		return -ENOMEM;
	}

	r = coap_packet_init(&response, data, MAX_COAP_MSG_LEN, 1, type, tkl, token,
		COAP_RESPONSE_CODE_CHANGED, id);
	if(r < 0) {
		goto end;
	}

	r = send_coap_reply(&response, addr, addr_len);

	end:
		k_free(data);
		return r;
}


/******************************************* END OF FILE *******************************************/
