/************************************************************************************************//**
 * @file		hyperopt.c
 * @brief
 * @desc
 *
 * @copyright	Copyright Kurt Hildebrand.
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
#include <math.h>
#include <stdlib.h>

#include <stdio.h>

#include "buffer.h"
#include "ipv6.h"


/* Private Macros -------------------------------------------------------------------------------- */
#define HYPERSPACE_COORD_OPT_TYPE	(0x22)


/* Private Types --------------------------------------------------------------------------------- */
typedef struct __attribute__((packed)) {
	float r;
	float t;
} Hypercoord;

typedef struct __attribute__((packed)) {
	uint8_t  src_seq;
	uint8_t  dest_seq;
	uint16_t packet_id;
	Hypercoord src;
	Hypercoord dest;
} HyperOpt;


/* Private Variables ----------------------------------------------------------------------------- */
uint16_t pkt_id = 0;


/* hyperopt_alloc_pkt ***************************************************************************//**
 * @brief		Allocates an IPv6 Packet for use through PInvoke. */
IPPacket* hyperopt_alloc_pkt(void)
{
	return malloc(sizeof(IPPacket));
}


/* hyperopt_free_pkt ****************************************************************************//**
 * @brief		Deallocates the IPv6 Packet. */
void hyperopt_free_pkt(IPPacket* pkt)
{
	if(pkt)
	{
		free(pkt);
	}
}


/* hyperopt_insert ******************************************************************************//**
 * @brief		Searches for an existing HyperOpt extension header and inserts one if one doesn't
 * 				exist. The opt output parameter is set to point to the HyperOpt in the packet.
 * @param[in]	pkt: the packet to insert a HyperOpt into.
 * @param[in]	buffer: the packet's data buffer.
 * @param[in]	count: the current number of bytes in the packet.
 * @param[in]	size: the buffer's total size in bytes.
 * @param[out]	out: address of a pointer, which gets set to point to the existing or inserted
 * 				HyperOpt.
 * @return		New length of the packet in bytes. */
int hyperopt_insert(IPPacket* pkt, uint8_t* buffer, int count, int size, HyperOpt** out)
{
	ipv6_init(pkt, buffer, count, size);

	IPExthdr eh = ipv6_eh_first(pkt);
	if(ipv6_eh_type(&eh) != IPV6_HBH)
	{
		ipv6_eh_prepend(&eh, IPV6_HBH, 0, 0);
	}

	/* Search for existing hyperspace option */
	IPOption opt = ipv6_opt_first(&eh);

	while(ipv6_opt_is_valid(&opt))
	{
		if(ipv6_opt_type(&opt) == HYPERSPACE_COORD_OPT_TYPE)
		{
			break;
		}

		ipv6_opt_next(&opt);
	}

	if(!ipv6_opt_is_valid(&opt))
	{
		HyperOpt temp;
		temp.src_seq   = 0;
		temp.dest_seq  = 0;
		temp.packet_id = pkt_id++;
		temp.src.r     = 0.0f;
		temp.src.t     = 0.0f;
		temp.dest.r    = NAN;
		temp.dest.t    = NAN;
		ipv6_opt_append(&opt, HYPERSPACE_COORD_OPT_TYPE, &temp, sizeof(temp), 4, 2);
		ipv6_opt_finalize(&opt);
	}

	ipv6_eh_finalize(&eh);

	/* Pointer to the hyperopt in the packet */
	*out = buffer_peek(ipv6_opt_reset_buffer(&opt), 0);	/* ipv6_opt_content */

	return buffer_length(&pkt->buffer);
}


/* hyperopt_get *********************************************************************************//**
 * @brief		Searches for an existing HyperOpt extension. The opt output parameter is set to
 * 				point to the HyperOpt in the packet.
 * @param[in]	pkt: the packet to search.
 * @param[in]	buffer: the packet's data buffer.
 * @param[in]	count: the current number of bytes in the packet.
 * @param[in]	size: the buffer's total size in bytes.
 * @param[out]	out: address of a pointer, which gets set to point to the existing HyperOpt.
 * @return		The length of the packet. */
int hyperopt_get(IPPacket* pkt, uint8_t* buffer, int count, int size, HyperOpt** out)
{
	ipv6_init(pkt, buffer, count, size);

	IPExthdr eh = ipv6_eh_first(pkt);
	if(ipv6_eh_type(&eh) != IPV6_HBH)
	{
		ipv6_eh_prepend(&eh, IPV6_HBH, 0, 0);
	}

	ipv6_eh_finalize(&eh);

	/* Search for existing hyperspace option */
	IPOption opt = ipv6_opt_first(&eh);
	while(ipv6_opt_is_valid(&opt))
	{
		if(ipv6_opt_type(&opt) == HYPERSPACE_COORD_OPT_TYPE)
		{
			break;
		}

		ipv6_opt_next(&opt);
	}

	/* Pointer to the hyperopt in the packet */
	*out = buffer_peek(ipv6_opt_reset_buffer(&opt), 0);	/* ipv6_opt_content */

	return buffer_length(&pkt->buffer);
}


/* hyperopt_r ***********************************************************************************//**
 * @brief		Returns the source r coordinate from the HyperOpt. This function exists to make
 * 				the PInvoke implementation opaque. That is, PInvoke doesn't need to duplicate the
 * 				HyperOpt structure. */
float hyperopt_src_r(const HyperOpt* opt)
{
	return opt->src.r;
}


/* hyperopt_t ***********************************************************************************//**
 * @brief		Returns the source t coordinate from the HyperOpt. This function exists to make
 * 				the PInvoke implementation opaque. That is, PInvoke doesn't need to duplicate the
 * 				HyperOpt structure. */
float hyperopt_src_t(const HyperOpt* opt)
{
	return opt->src.t;
}


/* hyperopt_seq *********************************************************************************//**
 * @brief		Returns the source seq number from the HyperOpt. This function exists to make the
 * 				PInvoke implementation opaque. That is, PInvoke doesn't need to duplicate the
 * 				HyperOpt structure. */
uint8_t hyperopt_src_seq(const HyperOpt* opt)
{
	return opt->src_seq;
}


/* hyperopt_set *********************************************************************************//**
 * @brief		Sets the HyperOpt extension header's destination r, t, and sequence numbers. This
 * 				function exists to make the PInvoke implementation opaque. That is, PInvoke doesn't
 * 				need to duplicate the HyperOpt structure. */
void hyperopt_set(HyperOpt* opt, float r, float t, uint8_t dest_seq)
{
	opt->dest.r = r;
	opt->dest.t = t;
	opt->dest_seq = dest_seq;
}


/* hyperopt_finalize ****************************************************************************//**
 * @brief		Updates the IPv6 packet's length as a final step before sending the packet. This is
 * 				required after hyperopt_insert which may have added a HyperOpt extension header. */
void hyperopt_finalize(IPPacket* pkt)
{
	ipv6_finalize(pkt);
}


/******************************************* END OF FILE *******************************************/
