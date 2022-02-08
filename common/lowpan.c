/************************************************************************************************//**
 * @file		lowpan.c
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
#include <logging/log.h>
#include <string.h>
#include <zephyr.h>

#include "bits.h"
#include "calc.h"
#include "lowpan.h"

LOG_MODULE_REGISTER(lowpan, LOG_LEVEL_INF);
// LOG_MODULE_REGISTER(lowpan, LOG_LEVEL_DBG);


/* Private Functions ----------------------------------------------------------------------------- */
static Lowpan   lowpan_first         (Ieee154_Frame*);
static bool     lowpan_next          (Lowpan*);
static bool     lowpan_is_valid      (const Lowpan*);
static unsigned lowpan_length        (const Lowpan*);
static uint8_t  lowpan_type          (const Lowpan*);
static bool     lowpan_prepend_header(Lowpan*, uint8_t, const void*, unsigned);
static bool     lowpan_append_header (Lowpan*, uint8_t, const void*, unsigned);
static bool     lowpan_push          (Lowpan*, const void*, unsigned);
static bool     lowpan_get_many      (const Lowpan*, void*, uint8_t*, unsigned);
static bool     lowpan_replace_many  (Lowpan*, const void*, uint8_t*, unsigned);

// static bool     lowpan_is_frag            (const Lowpan*);
// static bool     lowpan_prepend_frag_header(Lowpan*, uint16_t, uint16_t, uint16_t);
// // static bool     lowpan_append_frag_header (Lowpan*, uint16_t, uint16_t, uint16_t);
// static void     lowpan_frag_header_init   (Lowpan*, uint16_t, uint16_t, uint16_t);
// static uint16_t lowpan_frag_length        (const Lowpan*);
// static uint16_t lowpan_frag_tag           (const Lowpan*);
// static uint16_t lowpan_frag_size          (const Lowpan*);
// static uint16_t lowpan_frag_offset        (const Lowpan*);
// static bool     lowpan_frag_set_tag       (Lowpan*, uint16_t);
// static bool     lowpan_frag_set_size      (Lowpan*, uint16_t);
// static bool     lowpan_frag_set_offset    (Lowpan*, uint16_t);

static bool     lowpan_is_iphc            (const Lowpan*);
// static bool     lowpan_prepend_iphc_header(Lowpan*, struct in6_addr*, struct in6_addr*);
static bool     lowpan_append_iphc_header (Lowpan*, struct in6_addr*, struct in6_addr*);
static void     lowpan_iphc_header_init   (Lowpan*, struct in6_addr*, struct in6_addr*);
static uint16_t lowpan_iphc_type          (const Lowpan*);
static uint16_t lowpan_iphc_length        (const Lowpan*);
static uint16_t lowpan_iphc_read_type     (Lowpan*);
static uint8_t  lowpan_iphc_read_cid      (Lowpan*, uint16_t);
static void     lowpan_iphc_read_tcfl     (struct net_ipv6_hdr*, Lowpan*, uint16_t);
static void     lowpan_iphc_read_nh       (struct net_ipv6_hdr*, Lowpan*, uint16_t);
static void     lowpan_iphc_read_hoplimit (struct net_ipv6_hdr*, Lowpan*, uint16_t);
static void     lowpan_iphc_read_src      (struct in6_addr*, Lowpan*, uint16_t, uint8_t);
static bool     lowpan_iphc_read_dest     (struct in6_addr*, Lowpan*, uint16_t, uint8_t);

static void     lowpan_iphc_set_type          (Lowpan*, uint16_t);
static void     lowpan_iphc_write_cid         (Lowpan*, uint8_t, uint8_t);
static void     lowpan_iphc_write_tcfl        (Lowpan*, const struct net_ipv6_hdr*);
static void     lowpan_iphc_write_next_header (Lowpan*, const struct net_ipv6_hdr*);
static void     lowpan_iphc_write_hop_limit   (Lowpan*, const struct net_ipv6_hdr*);

static void     lowpan_iphc_set_addr_mode     (Lowpan*, const struct in6_addr*, const struct in6_addr*);
static uint8_t  lowpan_iphc_set_src_addr_mode (Lowpan*, const struct in6_addr*);
static void     lowpan_iphc_write_src         (Lowpan*, const struct in6_addr*);
static unsigned lowpan_iphc_flen_src          (uint16_t);

static uint8_t  lowpan_iphc_set_dest_addr_mode(Lowpan*, const struct in6_addr*);
static void     lowpan_iphc_write_dest        (Lowpan*, const struct in6_addr*);
static unsigned lowpan_iphc_flen_dest         (uint16_t);



/* Private Variables ----------------------------------------------------------------------------- */
struct in6_addr lowpan_ctx_data[16];
_Atomic uint32_t lowpan_ctx_bitmask;


// =============================================================================================== //
// 6LOWPAN Address Context                                                                         //
// =============================================================================================== //
/* lowpan_ctx_init ******************************************************************************//**
 * @brief		Initializes the lowpan address context. */
void lowpan_ctx_init(void)
{
	struct in6_addr link_local = {{{
		0xFE, 0x80, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
	}}};

	lowpan_ctx_bitmask = 0;

	lowpan_ctx_put(0, &link_local);
}


/* lowpan_ctx_clear *****************************************************************************//**
 * @brief		Clears the lowpan address context. */
void lowpan_ctx_clear(void)
{
	lowpan_ctx_init();
}


/* lowpan_ctx_count *****************************************************************************//**
 * @brief		Returns the current number of entries in the lowpan address context. */
unsigned lowpan_ctx_count(void)
{
	return calc_popcount_u32(lowpan_ctx_bitmask);
}


/* lowpan_ctx_put *******************************************************************************//**
 * @brief		Inserts a new address into the lowpan address context. */
bool lowpan_ctx_put(uint8_t id, const struct in6_addr* in)
{
	if(id > 16)
	{
		LOG_DBG("invalid id");
		return false;
	}

	if(lowpan_ctx_bitmask & (1 << id))
	{
		LOG_DBG("id already in use");
		return false;
	}

	lowpan_ctx_bitmask |= (1 << id);
	memmove(&lowpan_ctx_data[id], in, sizeof(struct in6_addr));
	return true;
}


/* lowpan_ctx_get *******************************************************************************//**
 * @brief		Retrieves an address with the given id out of the lowpan address context. */
bool lowpan_ctx_get(uint8_t id, struct in6_addr* out)
{
	if(id > 16)
	{
		return false;
	}

	if((lowpan_ctx_bitmask & (1 << id)) == 0)
	{
		return false;
	}

	memmove(out, &lowpan_ctx_data[id], sizeof(struct in6_addr));
	return true;
}


/* lowpan_ctx_search_id *************************************************************************//**
 * @brief		Returns a pointer to the lowpan context with the given id. */
struct in6_addr* lowpan_ctx_search_id(uint8_t id)
{
	if(id > 16)
	{
		return 0;
	}
	else if((lowpan_ctx_bitmask & (1 << id)) == 0)
	{
		return 0;
	}
	else
	{
		return &lowpan_ctx_data[id];
	}
}


/* lowpan_ctx_search_addr ***********************************************************************//**
 * @brief		Returns a pointer to the lowpan context with the given address.
 * @param[in]	addr: the IPv6 address to find in the lowpan context.
 * @param[in]	start: the byte offset to start the search.
 * @param[in]	len: the number of bytes to search. */
uint8_t lowpan_ctx_search_addr(const struct in6_addr* addr, unsigned start, unsigned len)
{
	unsigned i;

	if(start       >= sizeof(struct in6_addr) ||
	   len         >  sizeof(struct in6_addr) ||
	   start + len >  sizeof(struct in6_addr))
	{
		return -1;
	}

	for(i = 0; i < 16; i++)
	{
		if((lowpan_ctx_bitmask & (1 << i)) == 0)
		{
			continue;
		}

		if(memcmp(&addr->s6_addr[start], &lowpan_ctx_data[i].s6_addr[start], len) == 0)
		{
			return i;
		}
	}

	return -1;
}


/* lowpan_ctx_remove ****************************************************************************//**
 * @brief		Removes the lowpan address with the given id. */
bool lowpan_ctx_remove(uint8_t id)
{
	if(id > 16)
	{
		return false;
	}
	else if((lowpan_ctx_bitmask & (1 << id)) == 0)
	{
		return false;
	}
	else
	{
		lowpan_ctx_bitmask &= ~(1 << id);
		return true;
	}
}





/* lowpan_compress ******************************************************************************//**
 * @brief		Compresses an IPv6 packet into a lowpan frame.
 * @TODO:		Fragments
 * @warning		Expects frame src and destination to be already set. */
unsigned lowpan_compress(struct net_pkt* pkt, Bits* frags, uint32_t fragid, Ieee154_Frame* frame)
{
	struct net_pkt_cursor backup;
	net_pkt_cursor_backup(pkt, &backup);
	net_pkt_set_overwrite(pkt, true);
	net_pkt_cursor_init(pkt);

	NET_PKT_DATA_ACCESS_CONTIGUOUS_DEFINE(ipv6_access, struct net_ipv6_hdr);
	struct net_ipv6_hdr* ipv6_hdr = net_pkt_get_data(pkt, &ipv6_access);
	if(!ipv6_hdr) {
		LOG_ERR("could not get ipv6 header");
		return false; // -ENOBUFS
	}

	size_t packet_length = net_pkt_get_len(pkt);

	/* LOWPAN_IPHC
	 * 	  0                                       1
	 * 	  0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5
	 * 	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
	 * 	| 0 | 1 | 1 |  TF   |NH | HLIM  |CID|SAC|  SAM  | M |DAC|  DAM  |
	 * 	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
	 * 	|      SCI      |      DCI      |
	 * 	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * 	|ECN|   DSCP    |  rsv  |             Flow Label                |
	 * 	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * 	|          Next Header          |           Hop Limit           |
	 *  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
	 * 	|                        Source Address                         |
	 * 	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
	 * 	|                      Destination Address                      |
	 * 	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+ */

	/* Append IPHC dispatch, addressing mode bits, and context identifiers (SCI, DCI). */
	Lowpan lowpan = lowpan_first(frame);
	lowpan_append_iphc_header    (&lowpan, &ipv6_hdr->src, &ipv6_hdr->dst);
	lowpan_iphc_write_tcfl       (&lowpan, ipv6_hdr);

	uint8_t* lowpan_this_nh = lowpan.end;
	lowpan_iphc_write_next_header(&lowpan, ipv6_hdr);
	lowpan_iphc_write_hop_limit  (&lowpan, ipv6_hdr);
	lowpan_iphc_write_src        (&lowpan, &ipv6_hdr->src);
	lowpan_iphc_write_dest       (&lowpan, &ipv6_hdr->dst);

	// uint16_t iphc = lowpan_iphc_type(&lowpan);
	// LOG_DBG("%X", iphc);

	net_pkt_acknowledge_data(pkt, &ipv6_access);

	/* First 40 bytes have been appended (40 bytes is the first 5 fragments) */
	bits_set_many(frags, 0, 5);

	/* Append unfragmentable headers */
	uint8_t  hdr = ipv6_hdr->nexthdr;
	uint8_t  next_hdr;
	uint16_t length;
	uint16_t unfrag_length = 40;
	uint16_t frag = 5;

	/* Keep track of the indices of the next header field in extension headers in the frame. These
	 * are used later for appending an IPv6 fragmentation header. */
	while(1)
	{
		uint8_t temp;

		/* Is the header unfragmentable? */
		if(hdr != NET_IPV6_NEXTHDR_HBHO && hdr != NET_IPV6_NEXTHDR_ROUTING)
		{
			break;
		}
		else if(net_ipv6_is_nexthdr_upper_layer(hdr))
		{
			break;
		}

		struct net_pkt_cursor header_cursor;
		net_pkt_cursor_backup(pkt, &header_cursor);

		/* Read next header */
		net_pkt_read_u8(pkt, &next_hdr);

		/* Read length */
		net_pkt_read_u8(pkt, &temp);
		length = temp * 8 + 8;
		unfrag_length += length;

		net_pkt_cursor_restore(pkt, &header_cursor);

		lowpan_this_nh = lowpan.end;

		if(!lowpan_push(&lowpan, net_pkt_cursor_get_pos(pkt), length))
		{
			return 0;
		}

		bits_set_many(frags, frag, (length + 7) / 8);
		frag += (length + 7) / 8;
		hdr = next_hdr;
	}

	/* Find the next unsent fragment */
	frag = bits_next_zero(frags, frag);

	/* Are all fragments indicated as being sent? */
	if(frag >= bits_count(frags))
	{
		return 0;
	}

	// LOG_DBG("\r\n\tpacket length = %d\r\n\tunfrag length = %d", packet_length, unfrag_length);

	/* Determine if the packet must be fragmented. The packet must be fragmented if the packet
	 * doesn't fit in a frame or if the packet has been partially sent (indicated by a 1 in the
	 * fragment bitmap beyond the unfragmentable headers). */
	if(packet_length - unfrag_length > ieee154_free(frame) ||
	   bits_next_one(frags, frag) < bits_count(frags))
	{
		/* Fragment at the IPv6 layer by appending an IPv6 frag extension header. Offset: 8-octet
		 * units, relative to the start of the fragmentable part of the original packet. */
		bool islast = ieee154_free(frame) >= packet_length - (frag * 8) +
			sizeof(struct net_ipv6_frag_hdr);

		LOG_DBG("fragmenting: (%d, %d). free: %d. more: %d", frag * 8, packet_length,
			ieee154_free(frame), !islast);

		struct net_ipv6_frag_hdr frag_hdr = {
			.nexthdr  = be_get_u8(buffer_peek_at(&lowpan.frame->buffer, lowpan_this_nh, 0)),
			.reserved = 0,
			.offset   = htons(((frag - unfrag_length/8) << 3) | !islast),
			.id       = htonl(fragid),
		};

		if(!lowpan_push(&lowpan, &frag_hdr, sizeof(frag_hdr)))
		{
			LOG_ERR("failed fragmenting packet");
			return 0;
		}

		be_set_u8(buffer_peek_at(&lowpan.frame->buffer, lowpan_this_nh, 0), NET_IPV6_NEXTHDR_FRAG);
	}

	/* Append as many contiguous, unsent fragments as possible */
	while(frag < bits_count(frags) && bits_value(frags, frag) == 0)
	{
		unsigned length = calc_min_uint(8, packet_length - (frag * 8));

		net_pkt_set_overwrite(pkt, true);
		net_pkt_cursor_init(pkt);
		net_pkt_skip(pkt, frag * 8);

		if(!lowpan_push(&lowpan, net_pkt_cursor_get_pos(pkt), length))
		{
			break;
		}

		bits_set(frags, frag++);
	}

	/* Compute the number of sent bytes */
	unsigned sent = bits_ones(frags) * 8;

	/* Each fragment bit represents 8 bytes expect for the last fragment. If the last fragment has
	 * been sent, then the number of sent bytes may be overestimated. Note: the bit array range is
	 * [start, end). Therefore, the last bit is located end-1. */
	if(bits_end(frags) && bits_value(frags, bits_end(frags)-1))
	{
		/* Subtract unused bytes in the last fragment. Example:
		 * packet length: 27 bytes
		 * fragment bitmask: 1111b
		 * fragment bitmask length: 4 bits (1 bit = 8 byte fragment)
		 * fragment bitmask range: start = bit  0, end = bit  4
		 *                         start = byte 0, end = byte 32
		 *
		 * the last frag bit overestimates the length of the fragment by: 32 - 27 = 5 bytes
		 *
		 * 		0          1          2          3
		 * 		01234567 89012345 67890123 45678901
		 * 		                              ^ 27 bytes long
		 * 		1        1        1        1 fragment bit mask */
		sent -= (bits_end(frags) * 8) - packet_length;
	}

	net_pkt_cursor_restore(pkt, &backup);

	return sent;
}


/* lowpan_decompress ****************************************************************************//**
 * @brief		Decompresses a lowpan header to a full IPv6 packet. */
struct net_pkt* lowpan_decompress(struct net_if* iface, Ieee154_Frame* frame)
{
	LOG_DBG("decompress");

	struct net_pkt* pkt = 0;
	uint8_t frags_bitmask[1280 / 64] = { 0 };
	Ieee154_Frame backup_frame = *frame;
	Lowpan iphc_hdr = (Lowpan){ .frame = 0, .start = 0, .end = 0 };
	Lowpan lowpan = lowpan_first(frame);

	while(lowpan_is_valid(&lowpan) && (lowpan_type(&lowpan) & LOWPAN_NALP_MASK) != LOWPAN_NALP)
	{
		if(lowpan_is_iphc(&lowpan))
		{
			iphc_hdr = lowpan;
		}

		lowpan_next(&lowpan);
	}

	if(!lowpan_is_valid(&iphc_hdr))
	{
		LOG_DBG("iphc not valid");
		goto error;
	}

	LOG_DBG("frame length: %d\r\n\tiphc start = %d, end = %d",
	        ieee154_length(frame),
	        buffer_offsetof(&iphc_hdr.frame->buffer, iphc_hdr.start),
	        buffer_offsetof(&iphc_hdr.frame->buffer, iphc_hdr.end));

	buffer_read_set(&frame->buffer, iphc_hdr.start);

	/* LOWPAN_IPHC
	 * 	  0                                       1
	 * 	  0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5
	 * 	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
	 * 	| 0 | 1 | 1 |  TF   |NH | HLIM  |CID|SAC|  SAM  | M |DAC|  DAM  |
	 * 	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
	 * 	|      SCI      |      DCI      |
	 * 	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * 	|ECN|   DSCP    |  rsv  |             Flow Label                |
	 * 	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * 	|          Next Header          |           Hop Limit           |
	 *  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
	 * 	|                        Source Address                         |
	 * 	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
	 * 	|                      Destination Address                      |
	 * 	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+ */
	struct net_ipv6_hdr temp_hdr;
	uint16_t iphc = lowpan_iphc_read_type(&iphc_hdr);
	uint8_t  cid  = lowpan_iphc_read_cid (&iphc_hdr, iphc);
	lowpan_iphc_read_tcfl    (&temp_hdr, &iphc_hdr, iphc);
	lowpan_iphc_read_nh      (&temp_hdr, &iphc_hdr, iphc);
	lowpan_iphc_read_hoplimit(&temp_hdr, &iphc_hdr, iphc);
	lowpan_iphc_read_src     (&temp_hdr.src, &iphc_hdr, iphc, cid);
	lowpan_iphc_read_dest    (&temp_hdr.dst, &iphc_hdr, iphc, cid);

	pkt = net_pkt_alloc_with_buffer(iface, 255-24, AF_UNSPEC, 0, K_NO_WAIT);
	if(!pkt)
	{
		LOG_ERR("could not allocate packet");
		goto error;
	}

	NET_PKT_DATA_ACCESS_CONTIGUOUS_DEFINE(ipv6_access, struct net_ipv6_hdr);
	struct net_ipv6_hdr* ipv6_hdr = net_pkt_get_data(pkt, &ipv6_access);
	if(!ipv6_hdr) {
		LOG_ERR("could not get ipv6_hdr");
		goto error;	// -ENOBUFS;
	}

	Bits frags = make_bits(frags_bitmask, 1280 / 8);

	/* Copy individual fields EXCEPT len */
	ipv6_hdr->vtc       = temp_hdr.vtc;
	ipv6_hdr->tcflow    = temp_hdr.tcflow;
	ipv6_hdr->flow      = temp_hdr.flow;
	ipv6_hdr->nexthdr   = temp_hdr.nexthdr;
	ipv6_hdr->hop_limit = temp_hdr.hop_limit;
	memmove(&ipv6_hdr->src, &temp_hdr.src, sizeof(struct in6_addr));
	memmove(&ipv6_hdr->dst, &temp_hdr.dst, sizeof(struct in6_addr));
	net_pkt_acknowledge_data(pkt, &ipv6_access);

	/* IPv6 header has been decompressed. IPv6 header is 40 bytes or 5 fragments long. */
	bits_set_many(&frags, 0, 5);

	/* Decompress unfragmentable headers */
	unsigned offset = 40;

	uint8_t hdr = ipv6_hdr->nexthdr;
	uint8_t next_hdr;
	uint16_t length;
	uint16_t unfrag_length = 40;
	uint16_t packet_length;

	net_pkt_set_ipv6_next_hdr(pkt, ipv6_hdr->nexthdr);
	net_pkt_set_ip_hdr_len(pkt, sizeof(struct net_ipv6_hdr));

	while(1)
	{
		uint8_t temp;

		/* Is the header unfragmentable? */
		if(hdr != NET_IPV6_NEXTHDR_HBHO && hdr != NET_IPV6_NEXTHDR_ROUTING)
		{
			break;
		}
		else if(net_ipv6_is_nexthdr_upper_layer(hdr))
		{
			break;
		}

		void* ptr = buffer_peek(&frame->buffer, 0);
		next_hdr  = be_get_u8(buffer_pop_u8(&frame->buffer));	/* Read next header */
		temp      = be_get_u8(buffer_pop_u8(&frame->buffer));	/* Read header length */
		length    = temp * 8 + 8;
		unfrag_length += length;
		buffer_pop(&frame->buffer, length-2);

		net_pkt_write(pkt, ptr, length);
		bits_set_many(&frags, offset / 8, length / 8);
		offset += length;

		hdr = next_hdr;
	}

	/* TODO: fix: actually parse ext len and hdr prev. */
	net_pkt_set_ipv6_ext_len(pkt, 24);
	net_pkt_set_ipv6_hdr_prev(pkt, 40);

	/* Decompress fragmentable headers */
	packet_length = unfrag_length + buffer_remaining(&frame->buffer);
	LOG_DBG("packet length %d", packet_length);

	frags.count = (packet_length + 7) / 8;
	ipv6_hdr->len = ntohs(packet_length - 40);

	net_pkt_cursor_init(pkt);
	net_pkt_set_overwrite(pkt, true);
	net_pkt_skip(pkt, offset);
	net_pkt_set_overwrite(pkt, false);
	net_pkt_write(pkt, buffer_peek(&frame->buffer, 0), buffer_remaining(&frame->buffer));

	bits_set_many(&frags, offset / 8, (buffer_remaining(&frame->buffer) + 7) / 8);

	/* Compute the number of received bytes */
	unsigned received = bits_ones(&frags) * 8;

	/* Each fragment bit represents 8 bytes expect for the last fragment. If the last fragment has
	 * been sent, then the number of sent bytes may be overestimated. Note: the bit array range is
	 * [start, end). Therefore, the last bit is located end-1. */
	if(bits_end(&frags) && bits_value(&frags, bits_end(&frags)-1))
	{
		/* Subtract unused bytes in the last fragment. Example:
		 * packet length: 27 bytes
		 * fragment bitmask: 1111b
		 * fragment bitmask length: 4 bits (1 bit = 8 byte fragment)
		 * fragment bitmask range: start = bit  0, end = bit  4
		 *                         start = byte 0, end = byte 32
		 *
		 * the last frag bit overestimates the length of the fragment by: 32 - 27 = 5 bytes
		 *
		 * 		0          1          2          3
		 * 		01234567 89012345 67890123 45678901
		 * 		                              ^ 27 bytes long
		 * 		1        1        1        1 fragment bit mask */
		received -= (bits_end(&frags) * 8) - net_pkt_get_len(pkt);
	}

	LOG_DBG("received %d of %d", received, packet_length);

	*frame = backup_frame;
	if(received == net_pkt_get_len(pkt))
	{
		LOG_DBG("decompress done");
		return pkt;
	}
	else
	{
		LOG_DBG("decompress continue");
		return 0;
	}

	error:
		// LOG_ERR("error");
		*frame = backup_frame;
		if(pkt)
		{
			net_pkt_unref(pkt);
		}

		return 0;
}


// /* lowpan_decompresseed_size ********************************************************************//**
//  * @brief		Returns the length of the decompressed lowpan frame. */
// unsigned lowpan_decompressed_size(const Ieee154_Frame* frame)
// {
// 	Lowpan lowpan = lowpan_first(frame);

// 	/* Find either the frag or the iphc header */
// 	while(lowpan_is_valid(&lowpan) && (lowpan_type(&lowpan) & LOWPAN_NALP_MASK) != LOWPAN_NALP)
// 	{
// 		if(lowpan_is_frag(&lowpan))
// 		{
// 			return lowpan_frag_size(&lowpan);
// 		}
// 		else if(lowpan_is_iphc(&lowpan))
// 		{
// 			// return lowpan_iphc_length_content(&lowpan) + 40;
// 			return lowpan->frame->len -
// 			lowpan_iphc_field_index(lowpan, LOWPAN_IPHC_CONTENT_FIELD);
// 		}

// 		lowpan_next(&lowpan);
// 	}

// 	return 0;
// }


// /* lowpan_decompress ****************************************************************************//**
//  * @brief		Decompresses a lowpan header to a full IPv6 packet. */
// bool lowpan_decompress(struct net_pkt* pkt, Bits* frags, Ieee154_Frame* frame)
// {
// 	/* TODO: allocate struct net_pkt here. Also, do defragmentation here. */

// 	// LOG_DBG("decompress");
// 	Ieee154_Frame backup_frame = *frame;
// 	Lowpan iphc_hdr = (Lowpan){ .frame = 0, .start = (uint16_t)-1u, .end = (uint16_t)-1u };
// 	Lowpan frag_hdr = (Lowpan){ .frame = 0, .start = (uint16_t)-1u, .end = (uint16_t)-1u };
// 	Lowpan lowpan = lowpan_first(frame);

// 	while(lowpan_is_valid(&lowpan) && (lowpan_type(&lowpan) & LOWPAN_NALP_MASK) != LOWPAN_NALP)
// 	{
// 		if(lowpan_is_frag(&lowpan))
// 		{
// 			frag_hdr = lowpan;
// 		}
// 		else if(lowpan_is_iphc(&lowpan))
// 		{
// 			iphc_hdr = lowpan;
// 		}

// 		lowpan_next(&lowpan);
// 	}

// 	if(!lowpan_is_valid(&iphc_hdr))
// 	{
// 		LOG_ERR("iphc not valid");
// 		return false;
// 	}



// 	NET_PKT_DATA_ACCESS_CONTIGUOUS_DEFINE(ipv6_access, struct net_ipv6_hdr);
// 	struct net_ipv6_hdr* ipv6_hdr = net_pkt_get_data(pkt, &ipv6_access);
// 	if(!ipv6_hdr) {
// 		LOG_ERR("could not get ipv6 header");
// 		return false; // -ENOBUFS
// 	}

// 	net_buf_pull(frame, iphc_hdr.start);


// 	/* LOWPAN_IPHC
// 	 * 	  0                                       1
// 	 * 	  0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5
// 	 * 	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// 	 * 	| 0 | 1 | 1 |  TF   |NH | HLIM  |CID|SAC|  SAM  | M |DAC|  DAM  |
// 	 * 	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// 	 * 	|      SCI      |      DCI      |
// 	 * 	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 	 * 	|ECN|   DSCP    |  rsv  |             Flow Label                |
// 	 * 	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 	 * 	|          Next Header          |           Hop Limit           |
// 	 *  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// 	 * 	|                        Source Address                         |
// 	 * 	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// 	 * 	|                      Destination Address                      |
// 	 * 	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+ */
// 	uint16_t iphc = lowpan_iphc_read_type(&iphc_hdr);
// 	uint8_t  cid  = lowpan_iphc_read_cid (&iphc_hdr, iphc);
// 	lowpan_iphc_read_tcfl    (ipv6_hdr, &iphc_hdr, iphc);
// 	lowpan_iphc_read_nh      (ipv6_hdr, &iphc_hdr, iphc);
// 	lowpan_iphc_read_hoplimit(ipv6_hdr, &iphc_hdr, iphc);
// 	lowpan_iphc_read_src     (&ipv6_hdr->src, &iphc_hdr, iphc, cid);
// 	lowpan_iphc_read_dest    (&ipv6_hdr->dst, &iphc_hdr, iphc, cid);
// 	net_pkt_acknowledge_data (pkt, &ipv6_access);

// 	/* IPv6 header has been decompressed. IPv6 header is 40 bytes or 5 fragments long. */
// 	bits_set_many(frags, 0, 5);

// 	/* Decompress unfragmentable headers */
// 	unsigned offset = 40;

// 	uint8_t hdr = ipv6_hdr->nexthdr;
// 	uint8_t next_hdr;
// 	uint16_t length;
// 	uint16_t unfrag_length = 40;
// 	uint16_t packet_length;

// 	net_pkt_set_ipv6_next_hdr(pkt, ipv6_hdr->nexthdr);
// 	net_pkt_set_ip_hdr_len(pkt, sizeof(struct net_ipv6_hdr));

// 	while(1)
// 	{
// 		uint8_t temp;

// 		/* Is the header unfragmentable? */
// 		if(hdr != NET_IPV6_NEXTHDR_HBHO && hdr != NET_IPV6_NEXTHDR_ROUTING)
// 		{
// 			break;
// 		}
// 		else if(net_ipv6_is_nexthdr_upper_layer(hdr))
// 		{
// 			break;
// 		}

// 		void* ptr = frame->data;
// 		next_hdr = net_buf_pull_u8(frame);	/* Read next header */
// 		temp     = net_buf_pull_u8(frame);	/* Read header length */
// 		length   = temp * 8 + 8;
// 		unfrag_length += length;
// 		net_buf_pull(frame, length-2);

// 		net_pkt_write(pkt, ptr, length);
// 		bits_set_many(frags, offset / 8, length / 8);
// 		offset += length;

// 		hdr = next_hdr;
// 	}

// 	/* TODO: fix this */
// 	net_pkt_set_ipv6_ext_len(pkt, 24);
// 	net_pkt_set_ipv6_hdr_prev(pkt, 40);

// 	/* Decompress fragmentable headers */
// 	if(lowpan_is_valid(&frag_hdr) && lowpan_is_frag(&frag_hdr))
// 	{
// 		if(lowpan_frag_offset(&frag_hdr) < offset) {
// 			goto error;
// 		} else {
// 			packet_length = 40 + lowpan_frag_size(&frag_hdr);
// 			offset = lowpan_frag_offset(&frag_hdr);
// 		}

// 		// if(ipv6_length(packet) != lowpan_frag_size(&frag)) {
// 		// 	goto error;
// 		// } else if(getkey(packet) != lowpan_frag_tag(&frag)) {
// 		// 	goto error;
// 		// } else if(lowpan_frag_offset(&frag) < offset) {
// 		// 	goto error;
// 		// } else {
// 		// 	offset = lowpan_frag_offset(&frag);
// 		// }
// 	}
// 	else
// 	{
// 		packet_length = unfrag_length + frame->len;
// 	}

// 	frags->count = (packet_length + 7) / 8;
// 	ipv6_hdr->len = ntohs(packet_length - 40);

// 	net_pkt_cursor_init(pkt);
// 	net_pkt_skip(pkt, offset);
// 	net_pkt_set_overwrite(pkt, 1);
// 	net_pkt_write(pkt, frame->data, frame->len);

// 	bits_set_many(frags, offset / 8, (frame->len + 7) / 8);

// 	// list_replace_many(
// 	// 	&packet->list,
// 	// 	list_entry(header.packet, header.start),
// 	// 	offset,
// 	// 	iphc.end - header.start);

// 	// bits_set_many(frags, offset / 8, (iphc.end - header.start + 7) / 8);

// 	/* Compute the number of received bytes */
// 	unsigned received = bits_ones(frags) * 8;

// 	/* Each fragment bit represents 8 bytes expect for the last fragment. If the last fragment has
// 	 * been sent, then the number of sent bytes may be overestimated. Note: the bit array range is
// 	 * [start, end). Therefore, the last bit is located end-1. */
// 	if(bits_end(frags) && bits_value(frags, bits_end(frags)-1))
// 	{
// 		/* Subtract unused bytes in the last fragment. Example:
// 		 * packet length: 27 bytes
// 		 * fragment bitmask: 1111b
// 		 * fragment bitmask length: 4 bits (1 bit = 8 byte fragment)
// 		 * fragment bitmask range: start = bit  0, end = bit  4
// 		 *                         start = byte 0, end = byte 32
// 		 *
// 		 * the last frag bit overestimates the length of the fragment by: 32 - 27 = 5 bytes
// 		 *
// 		 * 		0          1          2          3
// 		 * 		01234567 89012345 67890123 45678901
// 		 * 		                              ^ 27 bytes long
// 		 * 		1        1        1        1 fragment bit mask */
// 		received -= (bits_end(frags) * 8) - net_pkt_get_len(pkt);
// 	}

// 	*frame = backup_frame;
// 	return received;

// 	error:
// 		*frame = backup_frame;
// 		return 0;
// }




/* lowpan_first *********************************************************************************//**
 * @brief		Returns the first 6LOWPAN header in the IEEE 802.15.4 frame. */
static Lowpan lowpan_first(Ieee154_Frame* frame)
{
	// return (Lowpan){0};

	/* TODO: refactor this. Ieee154 needs to provide a better API */
	Ieee154_IE ie = ieee154_ie_first(frame);

	while(!ieee154_ie_is_last(&ie))
	{
		ieee154_ie_next(&ie);
	}

	Lowpan lowpan;
	lowpan.frame = frame;
	lowpan.start = ieee154_payload_ptr(frame);
	lowpan.end   = ieee154_payload_ptr(frame);

	if(frame->buffer.write - lowpan.start != 0)
	{
		lowpan.end += lowpan_length(&lowpan);
	}

	// if(ieee154_length(frame) - ie.start != 0)
	// {
	// 	lowpan.end += lowpan_length(&lowpan);
	// }

	return lowpan;
}


/* lowpan_next **********************************************************************************//**
 * @brief		Moves to the next header if possible. */
static bool lowpan_next(Lowpan* lowpan)
{
	if(!lowpan_is_valid(lowpan))
	{
		return false;
	}
	else
	{
		lowpan->start = lowpan->end;
		lowpan->end  += lowpan_length(lowpan);
		return true;
	}
}


/* lowpan_is_valid ******************************************************************************//**
 * @brief		Returns true if the header is a valid 6LOWPAN header. */
static bool lowpan_is_valid(const Lowpan* lowpan)
{
	if(!lowpan->frame) {
		return false;
	} else if(lowpan->start >= lowpan->end) {
		return false;
	// } else if(lowpan->end > lowpan->frame->len) {
	// 	return false;
	// } else if(lowpan->start >= lowpan->frame->len) {
	// 	return false;
	} else {
		return true;
	}
}


/* lowpan_length ********************************************************************************//**
 * @brief		Returns the number of bytes in the header including type. */
static unsigned lowpan_length(const Lowpan* lowpan)
{
	if(lowpan_is_iphc(lowpan)) {
		return lowpan_iphc_length(lowpan);
	} else {
		return 0;
	}

	// if(lowpan_is_iphc(lowpan)) {
	// 	return lowpan_iphc_length(lowpan);
	// } else if(lowpan_is_frag(lowpan)) {
	// 	return lowpan_frag_length(lowpan);
	// } else if(lowpan_is_frak(lowpan)) {
	// 	return lowpan_frak_length(lowpan);
	// } else {
	// 	return 0;
	// }
}


/* lowpan_type **********************************************************************************//**
 * @brief		Returns the header's type. */
static uint8_t lowpan_type(const Lowpan* lowpan)
{
	return be_get_u8(buffer_peek_at(&lowpan->frame->buffer, lowpan->start, 0));

	// return lowpan->frame->__buf[lowpan->start];
	// return lowpan->frame->data[lowpan->start];

	// if(lowpan->end > lowpan->frame->len) {
	// 	return LOWPAN_NALP;
	// } else if(lowpan->end >= lowpan->frame->len) {
	// 	return LOWPAN_NALP;
	// } else {
	// 	return lowpan->frame->data[lowpan->start];
	// }

	// uint8_t type = LOWPAN_NALP;

	// /* Do not use lowpan_get_many so that we can get the type of a currently non-valid lowpan */
	// list_get_many(&lowpan->frame->list, &type, lowpan->start, 1);
	// return type;
}


/* lowpan_prepend_header ************************************************************************//**
 * @brief		Prepends a 6LOWPAN header before the current header. Moves the 6LOWPAN header to the
 * 				newly prepended header if successful. Reserves len bytes in addition to the header
 * 				type and if the 'in' parameter is not null, copies len bytes after the header type.
 * 				The final length of the 6LOWPAN header after the call to lowpan_prepend_header is
 * 				1 + len bytes.
 * @param[in]	lowpan: the header to prepend to.
 * @param[in]	type: the prepended header's type.
 * @param[in]	in: the content of the header.
 * @param[in]	len: the number of bytes for the content of the header.
 * @retval		true if the new header was appended successfully.
 * @retval		false if the frame is full. */
static bool lowpan_prepend_header(Lowpan* lowpan, uint8_t type, const void* in, unsigned len)
{
	/* Check if lowpan is attached to a frame */
	if(!lowpan->frame)
	{
		return false;
	}
	/* Check if invalid start and end indices */
	else if(lowpan->start > lowpan->end)
	{
		return false;
	}
	/* Check if end is out of bounds. Note: range is [start, end) so end can == end of the frame. */
	else if(lowpan->end > buffer_write(&lowpan->frame->buffer))
	{
		return false;
	}
	else if(ieee154_free(lowpan->frame) < 1 + len)
	{
		return false;
	}

	uint8_t* ptr = buffer_reserve_at(&lowpan->frame->buffer, lowpan->start, 1 + len);

	lowpan->end = lowpan->start + 1 + len;

	/* ptr[0] = lowpan->start + 0 */
	memmove(&ptr[0], &type, 1);

	/* ptr[1] = lowpan->start + 1 */
	if(in)
	{
		memmove(&ptr[1], in, len);
	}
	else
	{
		memset(&ptr[1], 0, len);
	}

	return true;
}


/* lowpan_append_header *************************************************************************//**
 * @brief		Appends a 6LOWPAN header after the current header. Moves the lowpan header to the
 * 				newly appended header if successful. Reserves len bytes in addition to the header
 * 				type and if the 'in' parameter is not null, copies len bytes after the header type.
 * 				The final length of the 6LOWPAN header after the call to lowpan_append_header is
 * 				1 + len bytes.
 * @param[in]	lowpan: the header to append to.
 * @param[in]	type: the appended header's type.
 * @param[in]	in: the content of the header.
 * @param[in]	len: the number of bytes for the content of the header.
 * @retval		true if the new header was appended successfully.
 * @retval		false if the frame is full. */
static bool lowpan_append_header(Lowpan* lowpan, uint8_t type, const void* in, unsigned len)
{
	/* Check that lowpan is attached to a frame */
	if(!lowpan->frame)
	{
		return false;
	}
	/* Check if invalid start and end indices */
	else if(lowpan->start > lowpan->end)
	{
		return false;
	}
	/* Check if end is out of bounds */
	else if(lowpan->end > buffer_write(&lowpan->frame->buffer))
	{
		return false;
	}
	/* Check if there is enough space for the type and the content of the header */
	else if(ieee154_free(lowpan->frame) < 1 + len)
	{
		return false;
	}

	uint8_t* ptr = buffer_reserve_at(&lowpan->frame->buffer, lowpan->end, 1 + len);

	/* Move lowpan to the new position */
	lowpan->start = lowpan->end;
	lowpan->end  += 1 + len;

	/* ptr[0] = lowpan->start + 0 */
	memmove(&ptr[0], &type, 1);

	/* ptr[1] = lowpan->start + 1 */
	if(in)
	{
		memmove(&ptr[1], in, len);
	}
	else
	{
		memset(&ptr[1], 0, len);
	}

	return true;
}


/* lowpan_push **********************************************************************************//**
 * @brief		Appends content to the current header.
 * @param[in]	lowpan: the header onto which new content is appended.
 * @param[in]	in: the new content to append.
 * @param[in]	len: the number of bytes to append.
 * @retval		true if new content was appended successfully.
 * @retval		false if the frame is full. */
static bool lowpan_push(Lowpan* lowpan, const void* in, unsigned len)
{
	if(!lowpan_is_valid(lowpan))
	{
		return false;
	}
	else if(ieee154_free(lowpan->frame) < len)
	{
		return false;
	}

	buffer_write_at(&lowpan->frame->buffer, in, lowpan->end, len);

	lowpan->end += len;
	return true;
}


/* lowpan_get_many ******************************************************************************//**
 * @brief		Copies len elements out of the lowpan header starting at the specified index.
 * @param[in]	lowpan: the lowpan header from which to retrieve bytes.
 * @param[out]	out: buffer to hold a copy of the requested elements.
 * @param[in]	start: pointer to first byte to copy out of the header.
 * @param[in]	len: the number of bytes to retrieve.
 * @retval		true if the bytes were retrieved successfully.
 * @retval		false if the bytes were not retrieved successfully. */
static bool lowpan_get_many(const Lowpan* lowpan, void* out, uint8_t* start, unsigned len)
{
	return lowpan_is_valid(lowpan) && buffer_read_at(&lowpan->frame->buffer, out, start, len);
}


/* lowpan_replace_many **************************************************************************//**
 * @brief		Replaces the specified number of bytes at the specified index of the current header.
 * @param[in]	lowpan: the header to replace bytes.
 * @param[in]	in: the new content.
 * @param[in]	start: pointer to start replacing bytes.
 * @param[in]	len: the number of bytes to replace.
 * @retval		true if the bytes were replaced successfully.
 * @retval		false if the bytes were not replaced successfully. */
static bool lowpan_replace_many(Lowpan* lowpan, const void* in, uint8_t* start, unsigned len)
{
	return lowpan_is_valid(lowpan) && buffer_replace_at(&lowpan->frame->buffer, in, start, len);
}





// // =============================================================================================== //
// // Fragment                                                                                        //
// // =============================================================================================== //
// /* Fragment
//  *  0                   1                   2                   3
//  *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  * |1 1 0 1 0 0 0 0| reserved      | tag                           |
//  * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  * | datagram size                 | datagram offset               |
//  * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
// /* lowpan_is_frag *******************************************************************************//**
//  * @brief		Returns true if the lowpan header is a fragmentation header. */
// static bool lowpan_is_frag(const Lowpan* lowpan)
// {
// 	return (lowpan_type(lowpan) & LOWPAN_FRAG_MASK) == LOWPAN_FRAG;
// }


// /* lowpan_prepend_frag_header *******************************************************************//**
//  * @brief		Prepends a fragmentation header. */
// static bool lowpan_prepend_frag_header(Lowpan* lowpan, uint16_t tag, uint16_t size, uint16_t offset)
// {
// 	/* Total size of the frag header is 7 bytes. Remove one byte for the header type. */
// 	if(!lowpan_prepend_header(lowpan, LOWPAN_FRAG, 0, 8-1))
// 	{
// 		return false;
// 	}
// 	else
// 	{
// 		lowpan_frag_header_init(lowpan, tag, size, offset);
// 		return true;
// 	}
// }


// /* lowpan_append_frag_header ********************************************************************//**
//  * @brief		Appends a fragmentation header. */
// static bool lowpan_append_frag_header(Lowpan* lowpan, uint16_t tag, uint16_t size, uint16_t offset)
// {
// 	/* Total size of the frag header is 8 bytes. Remove one byte for the header type. */
// 	if(!lowpan_append_header(lowpan, LOWPAN_FRAG, 0, 8-1))
// 	{
// 		return false;
// 	}
// 	else
// 	{
// 		lowpan_frag_header_init(lowpan, tag, size, offset);
// 		return true;
// 	}
// }


// /* lowpan_frag_header_init **********************************************************************//**
//  * @brief		Initializes a lowpan fragmentation header. */
// static void lowpan_frag_header_init(Lowpan* lowpan, uint16_t tag, uint16_t size, uint16_t offset)
// {
// 	uint8_t zero = 0;
// 	lowpan_replace_many(lowpan, &zero, lowpan->start + 1, 1);
// 	lowpan_frag_set_tag(lowpan, tag);
// 	lowpan_frag_set_size(lowpan, size);
// 	lowpan_frag_set_offset(lowpan, offset);
// }


// /* lowpan_append_length *************************************************************************//**
//  * @brief		Returns the length of the fragmentation header. */
// static uint16_t lowpan_frag_length(const Lowpan* lowpan)
// {
// 	if(!lowpan_is_frag(lowpan))
// 	{
// 		return 0;
// 	}
// 	else
// 	{
// 		return 8;
// 	}
// }


// /* lowpan_frag_tag ******************************************************************************//**
//  * @brief		Returns the fragmentation header's datagram tag. */
// static uint16_t lowpan_frag_tag(const Lowpan* lowpan)
// {
// 	if(!lowpan_is_frag(lowpan))
// 	{
// 		return 0;
// 	}
// 	else
// 	{
// 		uint8_t buf[2];
// 		lowpan_get_many(lowpan, buf, lowpan->start + 2, sizeof(buf));
// 		return (buf[0] << 8) | (buf[1] << 0);
// 	}
// }


// /* lowpan_frag_size *****************************************************************************//**
//  * @brief		Returns the fragmentation header's datagram size. */
// static uint16_t lowpan_frag_size(const Lowpan* lowpan)
// {
// 	if(!lowpan_is_frag(lowpan))
// 	{
// 		return 0;
// 	}
// 	else
// 	{
// 		uint8_t buf[2];
// 		lowpan_get_many(lowpan, buf, lowpan->start + 4, sizeof(buf));
// 		return (buf[0] << 8) | (buf[1] << 0);
// 	}
// }


// /* lowpan_frag_offset ***************************************************************************//**
//  * @brief		Returns the fragmentation header's datagram offset. */
// static uint16_t lowpan_frag_offset(const Lowpan* lowpan)
// {
// 	if(!lowpan_is_frag(lowpan))
// 	{
// 		return 0;
// 	}
// 	else
// 	{
// 		uint8_t buf[2];
// 		lowpan_get_many(lowpan, buf, lowpan->start + 6, sizeof(buf));
// 		return (buf[0] << 8) | (buf[1] << 0);
// 	}
// }


// /* lowpan_frag_set_tag **************************************************************************//**
//  * @brief		Sets the fragmentation header's datagram tag. */
// static bool lowpan_frag_set_tag(Lowpan* lowpan, uint16_t tag)
// {
// 	if(!lowpan_is_frag(lowpan))
// 	{
// 		return false;
// 	}
// 	else
// 	{
// 		uint8_t buf[2] = { (tag >> 8) & 0xFF, (tag >> 0) & 0xFF };
// 		lowpan_replace_many(lowpan, buf, lowpan->start + 2, sizeof(buf));
// 		return true;
// 	}
// }


// /* lowpan_frag_set_size *************************************************************************//**
//  * @brief		Sets the fragmentation header's datagram size. */
// static bool lowpan_frag_set_size(Lowpan* lowpan, uint16_t size)
// {
// 	if(!lowpan_is_frag(lowpan))
// 	{
// 		return false;
// 	}
// 	else
// 	{
// 		uint8_t buf[2] = { (size >> 8) & 0xFF, (size >> 0) & 0xFF };
// 		lowpan_replace_many(lowpan, buf, lowpan->start + 4, sizeof(buf));
// 		return true;
// 	}
// }


// /* lowpan_frag_set_offset ***********************************************************************//**
//  * @brief		Sets the fragmentation header's datagram offset. */
// static bool lowpan_frag_set_offset(Lowpan* lowpan, uint16_t offset)
// {
// 	if(!lowpan_is_frag(lowpan))
// 	{
// 		return 0;
// 	}
// 	else
// 	{
// 		uint8_t buf[2] = { (offset >> 8) & 0xFF, (offset >> 0) & 0xFF };
// 		lowpan_replace_many(lowpan, buf, lowpan->start + 6, sizeof(buf));
// 		return true;
// 	}
// }





// =============================================================================================== //
// IPHC                                                                                            //
// =============================================================================================== //
/* IPv6
 *
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |Version| Traffic Class |           Flow Label                  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |         Payload Length        |  Next Header  |   Hop Limit   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +                                                               +
 * |                                                               |
 * +                         Source Address                        +
 * |                                                               |
 * +                                                               +
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +                                                               +
 * |                                                               |
 * +                      Destination Address                      +
 * |                                                               |
 * +                                                               +
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * LOWPAN_IPHC
 *
 * 	  0                                       1
 * 	  0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5
 * 	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 	| 0 | 1 | 1 |  TF   |NH | HLIM  |CID|SAC|  SAM  | M |DAC|  DAM  |
 * 	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 	|      SCI      |      DCI      |
 * 	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 	|ECN|   DSCP    |  rsv  |             Flow Label                |
 * 	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 	|          Next Header          |           Hop Limit           |
 *  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 	|                        Source Address                         |
 * 	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 	|                      Destination Address                      |
 * 	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+ */
/* lowpan_is_iphc *******************************************************************************//**
 * @brief		Returns true if the lowpan header is an IPHC header. */
static bool lowpan_is_iphc(const Lowpan* lowpan)
{
	return (lowpan_type(lowpan) & LOWPAN_IPHC_MASK) == LOWPAN_IPHC;
}


// /* lowpan_prepend_iphc_header *******************************************************************//**
//  * @brief		Prepends a IPHC header. */
// static bool lowpan_prepend_iphc_header(Lowpan* lowpan, struct in6_addr* src, struct in6_addr* dest)
// {
// 	/* Total size of the default iphc header is 2 bytes. Remove one byte for the header type. */
// 	if(!lowpan_prepend_header(lowpan, LOWPAN_IPHC, 0, 2))
// 	{
// 		return false;
// 	}
// 	else
// 	{
// 		lowpan_iphc_header_init(lowpan, src, dest);
// 		return true;
// 	}
// }


/* lowpan_append_iphc_header ********************************************************************//**
 * @brief		Appends a IPHC header. */
static bool lowpan_append_iphc_header(Lowpan* lowpan, struct in6_addr* src, struct in6_addr* dest)
{
	/* Total size of the default iphc header is 2 bytes. Remove one byte for the header type. */
	if(!lowpan_append_header(lowpan, LOWPAN_IPHC, 0, 1))
	{
		return false;
	}
	else
	{
		lowpan_iphc_header_init(lowpan, src, dest);
		return true;
	}
}


/* lowpan_iphc_header_init **********************************************************************//**
 * @brief		Initializes an IPHC header. */
static void lowpan_iphc_header_init(Lowpan* lowpan, struct in6_addr* src, struct in6_addr* dest)
{
	/*   0                                       1
	 *   0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5
	 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
	 * | 0 | 1 | 1 |  TF   |NH | HLIM  |CID|SAC|  SAM  | M |DAC|  DAM  |
	 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+ */
	// lowpan_iphc_set_type(lowpan, LOWPAN_IPHC | LOWPAN_IPHC_NH_INLINE | LOWPAN_IPHC_HLIM_INLINE);

	lowpan_iphc_set_type(lowpan, (LOWPAN_IPHC << 8) |
		(LOWPAN_IPHC_TF_NONE | LOWPAN_IPHC_NH_INLINE | LOWPAN_IPHC_HLIM_INLINE));

	lowpan_iphc_set_addr_mode(lowpan, src, dest);
}


/* lowpan_iphc_type *****************************************************************************//**
 * @brief		Returns the first 16 bits of the IPHC header. */
static uint16_t lowpan_iphc_type(const Lowpan* lowpan)
{
	if(!lowpan_is_iphc(lowpan))
	{
		return 0;
	}
	else
	{
		uint8_t buf[2];
		lowpan_get_many(lowpan, buf, lowpan->start, 2);
		return (buf[0] << 8) | (buf[1] << 0);

		// uint16_t type;
		// lowpan_get_many(lowpan, &type, lowpan->start, 2);
		// return ntohs(type);
	}
}


/* lowpan_iphc_length ***************************************************************************//**
 * @brief		Returns the length of the IPHC header. */
static uint16_t lowpan_iphc_length(const Lowpan* lowpan)
{
	/* IPHC is an upper layer header. It spans the rest of the frame. */
	if(!lowpan_is_iphc(lowpan)) {
		return 0;
	} else {
		return buffer_write(&lowpan->frame->buffer) - lowpan->start;
	}
}



// ----------------------------------------------------------------------------------------------- //
// LOWPAN IPHC Get                                                                                 //
// ----------------------------------------------------------------------------------------------- //
static uint16_t lowpan_iphc_read_type(Lowpan* lowpan)
{
	return be_get_u16(buffer_pop_u16(&lowpan->frame->buffer));
}



/* lowpan_iphc_cid ******************************************************************************//**
 * @brief		Returns the context identifier if set. */
static uint8_t lowpan_iphc_read_cid(Lowpan* lowpan, uint16_t iphc)
{
	/* TODO: need to do further bitmasks to get SCI and DCI */
	if((iphc & LOWPAN_IPHC_CID_MASK) == LOWPAN_IPHC_CID_NONE) {
		return 0;
	} else {
		return be_get_u8(buffer_pop_u8(&lowpan->frame->buffer));
	}
}


/* lowpan_iphc_read_tcfl ************************************************************************//**
 * @brief		Reads the explicit congestion notification, differentiated services code point, and
 * 				flow label from the lowpan header and writes them into the ipv6 header. */
static void lowpan_iphc_read_tcfl(struct net_ipv6_hdr* ipv6, Lowpan* lowpan, uint16_t iphc)
{
	/* 6LOWPAN Traffic Class/Flow Label fields:
	 *
	 * 		 0                   1                   2                   3
	 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * 		|ECN|   DSCP    |  rsv  |             Flow Label                |
	 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * 		 Figure 4: TF = 00: Traffic Class and Flow Label carried in-line
	 *
	 * 		 0                   1                   2
	 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
	 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * 		|ECN|rsv|             Flow Label                |
	 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * 		Figure 5: TF = 01: Flow Label carried in-line
	 *
	 * 		 0 1 2 3 4 5 6 7
	 * 		+-+-+-+-+-+-+-+-+
	 * 		|ECN|   DSCP    |
	 * 		+-+-+-+-+-+-+-+-+
	 * 		Figure 6: TF = 10: Traffic Class carried in-line
	 *
	 * IPv6 Traffic Class:
	 *
	 * 		 0                   1                   2                   3
	 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * 		|Version| Traffic Class |           Flow Label                  |
	 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  	        | DSCP      |ECN|
	 *  	        +---------------+
	 */
	uint8_t buf[4] = { 0 };
	uint8_t temp = 0;

	ipv6->vtc = 0x60;

	if((iphc & LOWPAN_IPHC_TF_MASK) == LOWPAN_IPHC_TF_TC_FL)
	{
		buffer_pop_mem(&lowpan->frame->buffer, buf, 4);
		temp |= (buf[0] >> 6);	/* Get ECN */
		temp |= (buf[0] << 2);	/* Get DSCP */
		ipv6->vtc    |= (temp & 0xF0) >> 4;
		ipv6->tcflow |= (temp & 0x0F) << 4;
		ipv6->tcflow |= (buf[1] & 0x0F);	/* Set first 4 bits of flow label */
		memmove(&ipv6->flow, &buf[2], 2);
	}
	else if((iphc & LOWPAN_IPHC_TF_MASK) == LOWPAN_IPHC_TF_FL)
	{
		buffer_pop_mem(&lowpan->frame->buffer, buf, 3);
		temp |= (buf[0] >> 6);	/* Get ECN */
		ipv6->tcflow |= (temp & 0x0F) << 4;
		ipv6->tcflow |= (buf[1] & 0x0F);	/* Set first 4 bits of flow label */
		memmove(&ipv6->flow, &buf[1], 2);
	}
	else if((iphc & LOWPAN_IPHC_TF_MASK) == LOWPAN_IPHC_TF_TC)
	{
		buffer_pop_mem(&lowpan->frame->buffer, buf, 1);
		temp |= (buf[0] >> 6);	/* Get ECN */
		temp |= (buf[0] << 2);	/* Get DSCP */
		ipv6->vtc    |= (temp & 0xF0) >> 4;
		ipv6->tcflow |= (temp & 0x0F) << 4;
		ipv6->flow = 0;
	}
	else
	{
		ipv6->tcflow = 0;
		ipv6->flow   = 0;
	}
}


/* lowpan_iphc_read_nh **************************************************************************//**
 * @brief		Reads the next header field from the lowpan header and writes it into the ipv6
 * 				header. */
static void lowpan_iphc_read_nh(struct net_ipv6_hdr* ipv6, Lowpan* lowpan, uint16_t iphc)
{
	if((iphc & LOWPAN_IPHC_NH_MASK) == LOWPAN_IPHC_NH_NHC)
	{
		LOG_ERR("compressed next header field unsupported");
	}
	else
	{
		ipv6->nexthdr = be_get_u8(buffer_pop_u8(&lowpan->frame->buffer));
	}
}


/* lowpan_iphc_read_hoplimit ********************************************************************//**
 * @brief		Reads the hop limit field from the lowpan header and writes it into the ipv6
 * 				header. */
static void lowpan_iphc_read_hoplimit(struct net_ipv6_hdr* ipv6, Lowpan* lowpan, uint16_t iphc)
{
	if((iphc & LOWPAN_IPHC_HLIM_MASK) == LOWPAN_IPHC_HLIM_1) {
		ipv6->hop_limit = 1;
	} else if((iphc & LOWPAN_IPHC_HLIM_MASK) == LOWPAN_IPHC_HLIM_64) {
		ipv6->hop_limit = 64;
	} else if((iphc & LOWPAN_IPHC_HLIM_MASK) == LOWPAN_IPHC_HLIM_255) {
		ipv6->hop_limit = 255;
	} else {
		ipv6->hop_limit = be_get_u8(buffer_pop_u8(&lowpan->frame->buffer));
	}
}


/* lowpan_iphc_read_src *************************************************************************//**
 * @brief		Decompresses the source address from the lowpan header. */
static void lowpan_iphc_read_src(struct in6_addr* src, Lowpan* lowpan, uint16_t iphc, uint8_t cid)
{
	/* Get the context. Defaults to link local context (contex id == 0). */
	uint16_t frame_src_len;
	uint8_t sci = (cid & LOWPAN_IPHC_CID_SCI_MASK) >> LOWPAN_IPHC_CID_SCI_SHIFT;
	struct in6_addr* ctx_addr = lowpan_ctx_search_id(sci);

	memmove(src, ctx_addr, sizeof(struct in6_addr));

	switch(iphc & (LOWPAN_IPHC_SAC_MASK | LOWPAN_IPHC_SAM_MASK)) {
	/* Full 128-bit address inline */
	case LOWPAN_IPHC_SAC_STATELESS | LOWPAN_IPHC_SAM_SL_128:
		buffer_pop_mem(&lowpan->frame->buffer, &src->s6_addr[0], 16);
		break;

	/* The unspecified address (::) */
	case LOWPAN_IPHC_SAC_STATEFUL | LOWPAN_IPHC_SAM_SF_UNSPEC:
		memset(&src->s6_addr[0], 0, 16);
		break;

	/* 64 bits link local or from context, 64-bit inline */
	case LOWPAN_IPHC_SAC_STATELESS | LOWPAN_IPHC_SAM_SL_64:
	case LOWPAN_IPHC_SAC_STATEFUL  | LOWPAN_IPHC_SAM_SF_64:
		buffer_pop_mem(&lowpan->frame->buffer, &src->s6_addr[8], 8);
		break;

	/* 64 bits link local or from context, 0000:00ff:fe00:XXXX */
	case LOWPAN_IPHC_SAC_STATELESS | LOWPAN_IPHC_SAM_SL_16:
	case LOWPAN_IPHC_SAC_STATEFUL  | LOWPAN_IPHC_SAM_SF_16:
		src->s6_addr[11] = 0xFF;
		src->s6_addr[12] = 0xFE;
		buffer_pop_mem(&lowpan->frame->buffer, &src->s6_addr[14], 2);
		break;

	/* 64 bits link local or from context, 64-bit from encapsulating frame */
	case LOWPAN_IPHC_SAC_STATELESS | LOWPAN_IPHC_SAM_SL_0:
	case LOWPAN_IPHC_SAC_STATEFUL  | LOWPAN_IPHC_SAM_SF_0:
		frame_src_len = ieee154_length_src_addr(lowpan->frame);
		memmove(&src->s6_addr[16-frame_src_len], ieee154_src_addr(lowpan->frame), frame_src_len);
		if(frame_src_len == 2)
		{
			/* IID in the form of: 0000:00ff:fe00:XXXX */
			src->s6_addr[11] = 0xFF;
			src->s6_addr[12] = 0xFE;
		}
		else if(frame_src_len != 8)
		{
			return; // false;
		}
		break;

	default: return; // false;
	}

	return; // true;
}


/* lowpan_iphc_read_dest ************************************************************************//**
 * @brief		Decompresses the destination address from the lowpan header. */
static bool lowpan_iphc_read_dest(struct in6_addr* dest, Lowpan* lowpan, uint16_t iphc, uint8_t cid)
{
	/* Get the context. Defaults to link local context (contex id == 0). */
	uint16_t frame_dest_len;
	uint8_t dci = (cid & LOWPAN_IPHC_CID_DCI_MASK) >> LOWPAN_IPHC_CID_DCI_SHIFT;
	struct in6_addr* ctx_addr = lowpan_ctx_search_id(dci);

	memmove(&dest->s6_addr[0], ctx_addr, sizeof(struct in6_addr));

	switch(iphc & (LOWPAN_IPHC_DAC_MASK | LOWPAN_IPHC_M_MASK | LOWPAN_IPHC_DAM_MASK)) {
	/* Full 128-bit address inline */
	case LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_M_NOT_MULTICAST | LOWPAN_IPHC_DAM_SL_128:
	case LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_M_MULTICAST     | LOWPAN_IPHC_DAM_MSL_128:
		buffer_pop_mem(&lowpan->frame->buffer, &dest->s6_addr[0], 16);
		break;

	/* 64-bit link local or from context, 64-bit inline */
	case LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_M_NOT_MULTICAST | LOWPAN_IPHC_DAM_SL_64:
	case LOWPAN_IPHC_DAC_STATEFUL  | LOWPAN_IPHC_M_NOT_MULTICAST | LOWPAN_IPHC_DAM_SF_64:
		buffer_pop_mem(&lowpan->frame->buffer, &dest->s6_addr[8], 8);
		break;

	/* 64-bit link local or from context, 0000:00ff:fe00:XXXX */
	case LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_M_NOT_MULTICAST | LOWPAN_IPHC_DAM_SL_16:
	case LOWPAN_IPHC_DAC_STATEFUL  | LOWPAN_IPHC_M_NOT_MULTICAST | LOWPAN_IPHC_DAM_SF_16:
		dest->s6_addr[11] = 0xFF;
		dest->s6_addr[12] = 0xFE;
		buffer_pop_mem(&lowpan->frame->buffer, &dest->s6_addr[14], 2);
		break;

	/* 64-bit link local or from context, 64-bit encapsulating */
	case LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_M_NOT_MULTICAST | LOWPAN_IPHC_DAM_SL_0:
	case LOWPAN_IPHC_DAC_STATEFUL  | LOWPAN_IPHC_M_NOT_MULTICAST | LOWPAN_IPHC_DAM_SF_0:
		frame_dest_len = ieee154_length_dest_addr(lowpan->frame);
		memmove(&dest->s6_addr[16-frame_dest_len], ieee154_dest_addr(lowpan->frame), frame_dest_len);
		if(frame_dest_len == 2)
		{
			/* IID in the form of: 0000:00ff:fe00:XXXX */
			dest->s6_addr[11] = 0xFF;
			dest->s6_addr[12] = 0xFE;
		}
		else if(frame_dest_len != 8)
		{
			return false;
		}
		break;

	/* ffXX::00XX:XXXX:XXXX */
	case LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_M_MULTICAST | LOWPAN_IPHC_DAM_MSL_48:
		dest->s6_addr[0] = 0xFF;
		dest->s6_addr[1] = be_get_u8(buffer_pop_u8(&lowpan->frame->buffer));
		buffer_pop_mem(&lowpan->frame->buffer, &dest->s6_addr[11], 5);
		break;

	/* ffXX::00XX:XXXX */
	case LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_M_MULTICAST | LOWPAN_IPHC_DAM_MSL_32:
		dest->s6_addr[0] = 0xFF;
		dest->s6_addr[1] = be_get_u8(buffer_pop_u8(&lowpan->frame->buffer));
		buffer_pop_mem(&lowpan->frame->buffer, &dest->s6_addr[13], 3);
		break;

	/* ff02::00XX */
	case LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_M_MULTICAST | LOWPAN_IPHC_DAM_MSL_8:
		dest->s6_addr[0] = 0xFF;
		dest->s6_addr[1] = 0x02;
		dest->s6_addr[15] = be_get_u8(buffer_pop_u8(&lowpan->frame->buffer));
		break;

	/* ffXX:XXLL:PPPP:PPPP:PPPP:PPPP:XXXX:XXXX. L and P are taken from the specified context. Context
	 * is already copied to the output parameter. @note: context is a full 128-bit IPv6 address. It
	 * is intended that it overlays exactly with the above multicast address. That is, the context is
	 * not shifted so that the prefix is located at index 0. */
	case LOWPAN_IPHC_DAC_STATEFUL | LOWPAN_IPHC_M_MULTICAST | LOWPAN_IPHC_DAM_MSF_48:
		dest->s6_addr[0] = 0xFF;
		dest->s6_addr[1] = be_get_u8(buffer_pop_u8(&lowpan->frame->buffer));
		dest->s6_addr[2] = be_get_u8(buffer_pop_u8(&lowpan->frame->buffer));
		dest->s6_addr[3] = ctx_addr->s6_addr[3] > 64 ? 64 : ctx_addr->s6_addr[3];
		buffer_pop_mem(&lowpan->frame->buffer, &dest->s6_addr[12], 4);
		break;

	default: return false;
	}

	return true;
}


/* lowpan_iphc_set_type *************************************************************************//**
 * @brief		Sets the IPHC header's type.
 * @warning		Assumes that lowpan is a valid IPHC header. */
static void lowpan_iphc_set_type(Lowpan* lowpan, uint16_t iphc)
{
	uint8_t buf[2];
	buf[0] = (iphc >> 8) & 0xFF;
	buf[1] = (iphc >> 0) & 0xFF;
	lowpan_replace_many(lowpan, buf, lowpan->start, sizeof(buf));

	// iphc = htons(iphc);
	// lowpan_replace_many(lowpan, &iphc, lowpan->start, 2);
}


/* lowpan_iphc_write_cid ************************************************************************//**
 * @brief		Writes source and destination context identifier into the lowpan header. */
static void lowpan_iphc_write_cid(Lowpan* lowpan, uint8_t sci, uint8_t dci)
{
	uint8_t cid = (sci << LOWPAN_IPHC_CID_SCI_SHIFT) | (dci << LOWPAN_IPHC_CID_DCI_SHIFT);

	if(cid != 0)
	{
		uint16_t iphc = lowpan_iphc_type(lowpan);
		lowpan_iphc_set_type(lowpan, iphc | LOWPAN_IPHC_CID_EXT);
		lowpan_push(lowpan, &cid, 1);
	}
}


/* lowpan_iphc_write_tcfl ***********************************************************************//**
 * @brief		Writes traffic class / flow label into the lowpan header. */
static void lowpan_iphc_write_tcfl(Lowpan* lowpan, const struct net_ipv6_hdr* ipv6)
{
	/* 6LOWPAN Traffic Class/Flow Label fields:
	 *
	 * 		 0                   1                   2                   3
	 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * 		|ECN|   DSCP    |  rsv  |             Flow Label                |
	 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * 		 Figure 4: TF = 00: Traffic Class and Flow Label carried in-line
	 *
	 * 		 0                   1                   2
	 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
	 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * 		|ECN|rsv|             Flow Label                |
	 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * 		Figure 5: TF = 01: Flow Label carried in-line
	 *
	 * 		 0 1 2 3 4 5 6 7
	 * 		+-+-+-+-+-+-+-+-+
	 * 		|ECN|   DSCP    |
	 * 		+-+-+-+-+-+-+-+-+
	 * 		Figure 6: TF = 10: Traffic Class carried in-line
	 *
	 * IPv6 Traffic Class:
	 *
	 * 		 0              |    1          |        2      |            3
	 * 		 0 1 2 3 4 5 6 7|8 9 0 1 2 3 4 5|6 7 8 9 0 1 2 3|4 5 6 7 8 9 0 1
	 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * 		|Version| Traffic Class |       |   Flow Label  |               |
	 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  	        | DSCP  |   |ECN|       |               |
	 *  	        +-------|-------+       |               |
	 */
	uint8_t  buf[4];
	uint16_t iphc = lowpan_iphc_type(lowpan) & ~(LOWPAN_IPHC_TF_MASK);
	uint8_t  ecn  = (ipv6->tcflow >> 4) & 0x03;
	uint8_t  dscp = ((ipv6->vtc & 0x0F) << 2) | (ipv6->tcflow >> 6);
	uint8_t  flowlabel[3];
	flowlabel[0] = (ipv6->tcflow & 0x0F);
	memmove(&flowlabel[1], &ipv6->flow, 2);

	/* TF = 00 if flowlabel & dscp (ecn set regardless)
	 * TF = 01 if flowlabel        (ecn set regardless)
	 * TF = 10 if ecn | dscp       (flowlabel not set)
	 * TF = 11 else                (nothing needs to be set) */
	if(flowlabel[0] != 0 && flowlabel[1] != 0 && flowlabel[2] != 0) {
		if(dscp != 0) {
			iphc |= LOWPAN_IPHC_TF_TC_FL;
			buf[0] = (ecn << 6) | dscp;
			buf[1] = (flowlabel[0] & 0x0F);
			buf[2] = (flowlabel[1]);
			buf[3] = (flowlabel[2]);
			lowpan_push(lowpan, buf, 4);
		} else {
			iphc |= LOWPAN_IPHC_TF_FL;
			buf[0] = (ecn << 6) | (flowlabel[0] & 0x0F);
			buf[1] = (flowlabel[1]);
			buf[2] = (flowlabel[2]);
			lowpan_push(lowpan, buf, 3);
		}
	} else if(ecn != 0 || dscp != 0) {
		iphc |= LOWPAN_IPHC_TF_TC;
		buf[0] = (ecn << 6) | dscp;
		lowpan_push(lowpan, buf, 1);
	} else {
		iphc |= LOWPAN_IPHC_TF_NONE;
	}

	lowpan_iphc_set_type(lowpan, iphc);
}


/* lowpan_iphc_set_next_header ******************************************************************//**
 * @brief		Writes the next header field. Next header will always be set as
 * 				LOWPAN_IPHC_NH_INLINE. */
static void lowpan_iphc_write_next_header(Lowpan* lowpan, const struct net_ipv6_hdr* ipv6)
{
	/* TODO: verify that LOWPAN_IPHC_NH_INLINE is set when compressing */
	lowpan_push(lowpan, &ipv6->nexthdr, 1);
}


/* lowpan_iphc_write_hop_limit ******************************************************************//**
 * @brief		Writes the hop limit field. Hop limit will always be set as
 * 				LOWPAN_IPHC_HLIM_INLINE. */
static void lowpan_iphc_write_hop_limit(Lowpan* lowpan, const struct net_ipv6_hdr* ipv6)
{
	/* TODO: verify that LOWPAN_IPHC_HLIM_INLINE is set when compressing */
	lowpan_push(lowpan, &ipv6->hop_limit, 1);
}


/* lowpan_iphc_set_addr_mode ********************************************************************//**
 * @brief		Sets the IPHC lowpan header's addressing mode.
 * @desc		Potentially sets SAC, SAM, DAC, DAM. */
static void lowpan_iphc_set_addr_mode(
	Lowpan* lowpan,
	const struct in6_addr* src,
	const struct in6_addr* dest)
{
	uint8_t sci = lowpan_iphc_set_src_addr_mode(lowpan, src);
	uint8_t dci = lowpan_iphc_set_dest_addr_mode(lowpan, dest);
	lowpan_iphc_write_cid(lowpan, sci, dci);

	/* TODO: need to get src and dest context identifier */
	// iphc |= lowpan_iphc_set_src_addr_mode(lowpan, src);
	// iphc |= lowpan_iphc_set_dest_addr_mode(lowpan, dest);

	// lowpan_iphc_set_type(lowpan, mode);
	// return mode;
}


/* lowpan_iphc_set_set_src_addr_mode ************************************************************//**
 * @brief		Sets the source addressing mode bits in the iphc header. Returns the SCI
 * 				corresponding to addressing mode. */
static uint8_t lowpan_iphc_set_src_addr_mode(Lowpan* lowpan, const struct in6_addr* src)
{
	uint8_t  ctx_id = -1;
	uint16_t iphc   = lowpan_iphc_type(lowpan) & ~(LOWPAN_IPHC_SAC_MASK | LOWPAN_IPHC_SAM_MASK);

	/* Is address unspecified ::0? */
	if(memcmp(src, &in6addr_any, sizeof(src)) == 0)
	{
		iphc |= LOWPAN_IPHC_SAC_STATEFUL | LOWPAN_IPHC_SAM_SF_UNSPEC;
		goto done;
	}

	/* Search context prefix, which is /64. I.E. the first 8 bytes. */
	ctx_id = lowpan_ctx_search_addr(src, 0, 8);

	if(ctx_id > 16)
	{
		iphc |= LOWPAN_IPHC_SAC_STATELESS | LOWPAN_IPHC_SAM_SL_128;
		goto done;
	}

	/* Is the address link local? */
	if(ctx_id == 0)
	{
		iphc |= LOWPAN_IPHC_SAC_STATELESS;
	}
	else
	{
		iphc |= LOWPAN_IPHC_SAC_STATEFUL;
	}

	/* Is the address in the form 0000:00ff:fe00:XXXX? */
	if(src->s6_addr[8]  == 0x00 && src->s6_addr[9]  == 0x00 && src->s6_addr[10] == 0x00 &&
	   src->s6_addr[11] == 0xFF && src->s6_addr[12] == 0xFE && src->s6_addr[13] == 0x00)
	{
		iphc |= LOWPAN_IPHC_SAM_16;
	}
	else
	{
		iphc |= LOWPAN_IPHC_SAM_64;
	}

	/* Can the source address be elided? Does the encapsulating frame contain the source address? */
	uint16_t frame_src_len = ieee154_length_src_addr(lowpan->frame);

	if(frame_src_len && memcmp(
		&src->s6_addr[16-frame_src_len],
		ieee154_src_addr(lowpan->frame),
		frame_src_len) == 0)
	{
		/* The source address can be elided */
		iphc = (iphc & ~LOWPAN_IPHC_SAM_MASK) | LOWPAN_IPHC_SAM_0;
	}

	done:
		lowpan_iphc_set_type(lowpan, iphc);
		return ctx_id;

		if(ctx_id > 16)
		{
			return 0;
		}
		else
		{
			return ctx_id;
		}
}


/* lowpan_iphc_write_src ************************************************************************//**
 * @brief		Writes the compressed source address to the frame. */
static void lowpan_iphc_write_src(Lowpan* lowpan, const struct in6_addr* src)
{
	uint16_t iphc = lowpan_iphc_type(lowpan);
	unsigned flen = lowpan_iphc_flen_src(iphc);

	lowpan_push(lowpan, &src->s6_addr[16 - flen], flen);
}


/* lowpan_iphc_flen_src *************************************************************************//**
 * @brief		Returns the expected length of the source address field from the IPHC type. */
static unsigned lowpan_iphc_flen_src(uint16_t iphc)
{
	switch(iphc & (LOWPAN_IPHC_SAC_MASK | LOWPAN_IPHC_SAM_MASK)) {
	/* SAC=0: Stateless (SL) Source Addressing Mode */
	case LOWPAN_IPHC_SAC_STATELESS | LOWPAN_IPHC_SAM_SL_128:    return 16;
	case LOWPAN_IPHC_SAC_STATELESS | LOWPAN_IPHC_SAM_SL_64:     return 8;
	case LOWPAN_IPHC_SAC_STATELESS | LOWPAN_IPHC_SAM_SL_16:     return 2;
	case LOWPAN_IPHC_SAC_STATELESS | LOWPAN_IPHC_SAM_SL_0:      return 0;

	/* SAC=1: Stateful (SF) Source Addressing Mode */
	case LOWPAN_IPHC_SAC_STATEFUL | LOWPAN_IPHC_SAM_SF_UNSPEC:  return 0;
	case LOWPAN_IPHC_SAC_STATEFUL | LOWPAN_IPHC_SAM_SF_64:      return 8;
	case LOWPAN_IPHC_SAC_STATEFUL | LOWPAN_IPHC_SAM_SF_16:      return 2;
	case LOWPAN_IPHC_SAC_STATEFUL | LOWPAN_IPHC_SAM_SF_0:       return 0;
	default : return 0;
	}
}


/* lowpan_iphc_set_dest_addr_mode ***************************************************************//**
 * @brief		Returns the addressing bits for the source address. */
static uint8_t lowpan_iphc_set_dest_addr_mode(Lowpan* lowpan, const struct in6_addr* dest)
{
	uint8_t ctx_id = -1;
	uint16_t iphc = lowpan_iphc_type(lowpan) &
		~(LOWPAN_IPHC_M_MASK | LOWPAN_IPHC_DAC_MASK | LOWPAN_IPHC_DAM_MASK);

	/* Is the destination a multicast address? */
	if(net_ipv6_is_addr_mcast(dest))
	{
		iphc |= LOWPAN_IPHC_M_MULTICAST;

		/* Find context in the form ffXX:XXLL:PPPP:PPPP:PPPP:PPPP:XXXX:XXXX */
		ctx_id = lowpan_ctx_search_addr(dest, 3, 9);

		if(0 < ctx_id && ctx_id < 16)
		{
			iphc |= LOWPAN_IPHC_DAC_STATEFUL | LOWPAN_IPHC_DAM_MSF_48;
		}
		/* Does the address take the form ff02::00XX? */
		else if(dest->s6_addr[1]  == 0x02 && dest->s6_addr[2]  == 0x00 &&
		        dest->s6_addr[3]  == 0x00 && dest->s6_addr[4]  == 0x00 &&
		        dest->s6_addr[5]  == 0x00 && dest->s6_addr[6]  == 0x00 &&
		        dest->s6_addr[7]  == 0x00 && dest->s6_addr[8]  == 0x00 &&
		        dest->s6_addr[9]  == 0x00 && dest->s6_addr[10] == 0x00 &&
		        dest->s6_addr[11] == 0x00 && dest->s6_addr[12] == 0x00 &&
		        dest->s6_addr[13] == 0x00 && dest->s6_addr[14] == 0x00)
		{
			iphc |= LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_DAM_MSL_8;
		}
		/* Does the address take the form ffXX::00XX:XXXX? */
		else if(dest->s6_addr[2]  == 0x00 && dest->s6_addr[3]  == 0x00 &&
		        dest->s6_addr[4]  == 0x00 && dest->s6_addr[5]  == 0x00 &&
		        dest->s6_addr[6]  == 0x00 && dest->s6_addr[7]  == 0x00 &&
		        dest->s6_addr[8]  == 0x00 && dest->s6_addr[9]  == 0x00 &&
		        dest->s6_addr[10] == 0x00 && dest->s6_addr[11] == 0x00 &&
		        dest->s6_addr[12] == 0x00)
		{
			iphc |= LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_DAM_MSL_32;
		}
		/* Does the address take the form ffXX::00XX:XXXX:XXXX? */
		else if(dest->s6_addr[2]  == 0x00 && dest->s6_addr[3]  == 0x00 &&
		        dest->s6_addr[4]  == 0x00 && dest->s6_addr[5]  == 0x00 &&
		        dest->s6_addr[6]  == 0x00 && dest->s6_addr[7]  == 0x00 &&
		        dest->s6_addr[8]  == 0x00 && dest->s6_addr[9]  == 0x00 &&
		        dest->s6_addr[10] == 0x00)
		{
			iphc |= LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_DAM_MSL_48;
		}
		/* Must carry full 128-bit address inline */
		else
		{
			iphc |= LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_DAM_MSL_128;
		}
	}
	/* Address is not a multicast address */
	else
	{
		iphc |= LOWPAN_IPHC_M_NOT_MULTICAST;

		ctx_id = lowpan_ctx_search_addr(dest, 0, 8);

		if(ctx_id > 16)
		{
			iphc |= LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_DAM_SL_128;
		}
		else
		{
			/* Is the address link local? */
			if(ctx_id == 0)
			{
				iphc |= LOWPAN_IPHC_DAC_STATELESS;
			}
			else
			{
				iphc |= LOWPAN_IPHC_DAC_STATEFUL;
			}

			/* Is the IID in the form 0000:00ff:fe00:XXXX? */
			if(dest->s6_addr[8]  == 0x00 && dest->s6_addr[9]  == 0x00 &&
			   dest->s6_addr[10] == 0x00 && dest->s6_addr[11] == 0xFF &&
			   dest->s6_addr[12] == 0xFE && dest->s6_addr[13] == 0x00)
			{
				iphc |= LOWPAN_IPHC_DAM_16;
			}
			else
			{
				iphc |= LOWPAN_IPHC_DAM_64;
			}

			/* Can the destination address be elided? Does the encapsulating frame contain the
			 * destination address? */
			uint16_t frame_dest_len = ieee154_length_dest_addr(lowpan->frame);

			if(frame_dest_len && memcmp(
				&dest->s6_addr[16-frame_dest_len],
				ieee154_dest_addr(lowpan->frame),
				frame_dest_len) == 0)
			{
				/* The destination address can be elided */
				iphc = (iphc & ~(LOWPAN_IPHC_DAM_MASK)) | LOWPAN_IPHC_DAM_0;
			}
		}
	}

	lowpan_iphc_set_type(lowpan, iphc);

	if(ctx_id > 16)
	{
		return 0;
	}
	else
	{
		return ctx_id;
	}
}


/* lowpan_iphc_write_dest ***********************************************************************//**
 * @brief		Writes the compressed destination address to the frame. */
static void lowpan_iphc_write_dest(Lowpan* lowpan, const struct in6_addr* dest)
{
	uint16_t iphc = lowpan_iphc_type(lowpan);
	unsigned flen = lowpan_iphc_flen_dest(iphc);

	/* Copy the first few bytes if multicast address. However, no need to handle ff02::00XX the
	 * address bytes are contiguous (this case is handled like non-multicast addresses). */
	if((iphc & LOWPAN_IPHC_M_MASK) == LOWPAN_IPHC_M_MULTICAST)
	{
		/* Is address in the form ffXX:XXLL:PPPP:PPPP:PPPP:PPPP:XXXX:XXXX? */
		if((iphc & LOWPAN_IPHC_DAC_MASK) == LOWPAN_IPHC_DAC_STATEFUL &&
		  ((iphc & LOWPAN_IPHC_DAM_MASK) == LOWPAN_IPHC_DAM_MSF_48))
		{
			lowpan_push(lowpan, &dest->s6_addr[1], 2);
			flen -= 2;
		}
		/* Is address in the form ffXX::00XX:XXXX or ffXX::00XX:XXXX:XXXX? */
		else if((iphc & LOWPAN_IPHC_DAC_MASK) == LOWPAN_IPHC_DAC_STATELESS &&
		       ((iphc & LOWPAN_IPHC_DAM_MASK) == LOWPAN_IPHC_DAM_MSL_32 ||
		        (iphc & LOWPAN_IPHC_DAM_MASK) == LOWPAN_IPHC_DAM_MSL_48))
		{
			lowpan_push(lowpan, &dest->s6_addr[1], 1);
			flen -= 1;
		}
	}

	lowpan_push(lowpan, &dest->s6_addr[16-flen], flen);
}


/* lowpan_iphc_flen_dest ************************************************************************//**
 * @brief		Returns the expected length of the destination address field from the IPHC type. */
static unsigned lowpan_iphc_flen_dest(uint16_t iphc)
{
	switch(iphc & (LOWPAN_IPHC_M_MASK | LOWPAN_IPHC_DAC_MASK | LOWPAN_IPHC_DAM_MASK)) {
	/* M=0, DAC=0: Not multicast, stateless (SL) destination addressing */
	case LOWPAN_IPHC_M_NOT_MULTICAST | LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_DAM_SL_128:
		return 16;
	case LOWPAN_IPHC_M_NOT_MULTICAST | LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_DAM_SL_64:
		return 8;
	case LOWPAN_IPHC_M_NOT_MULTICAST | LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_DAM_SL_16:
		return 2;
	case LOWPAN_IPHC_M_NOT_MULTICAST | LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_DAM_SL_0:
		return 0;

	/* M=0, DAC=1: Not multicast, stateful (SF) destination addressing */
	case LOWPAN_IPHC_M_NOT_MULTICAST | LOWPAN_IPHC_DAC_STATEFUL | LOWPAN_IPHC_DAM_SF_64:
		return 8;
	case LOWPAN_IPHC_M_NOT_MULTICAST | LOWPAN_IPHC_DAC_STATEFUL | LOWPAN_IPHC_DAM_SF_16:
		return 2;
	case LOWPAN_IPHC_M_NOT_MULTICAST | LOWPAN_IPHC_DAC_STATEFUL | LOWPAN_IPHC_DAM_SF_0:
		return 0;

	/* M=1, DAC=0: Multicast, stateless (SL) destination addressing */
	case LOWPAN_IPHC_M_MULTICAST | LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_DAM_MSL_128:
		return 16;
	case LOWPAN_IPHC_M_MULTICAST | LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_DAM_MSL_48:
		return 6;
	case LOWPAN_IPHC_M_MULTICAST | LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_DAM_MSL_32:
		return 4;
	case LOWPAN_IPHC_M_MULTICAST | LOWPAN_IPHC_DAC_STATELESS | LOWPAN_IPHC_DAM_MSL_8:
		return 1;

	/* M=1, DAC=1: Multicast (M), stateful (SF) destination addressing */
	case LOWPAN_IPHC_M_MULTICAST | LOWPAN_IPHC_DAC_STATEFUL | LOWPAN_IPHC_DAM_MSF_48:
		return 6;

	default : return 0;
	}
}


/******************************************* END OF FILE *******************************************/
