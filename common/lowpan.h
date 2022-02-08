/************************************************************************************************//**
 * @file		lowpan.h
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
 * @brief		This file implements IPv6 over IEEE 802.15.4. Specifically, RFC 4944, RFC 6282,
 * @TODO		Rewrite lowpan documentation in this header.
 * @TODO		RFC 8205
 * @ref			https://tools.ietf.org/html/rfc1700
 * @ref			https://tools.ietf.org/html/rfc4944
 * @ref			https://tools.ietf.org/html/rfc6282
 * @ref			https://tools.ietf.org/html/rfc8025
 * @ref			https://www.iana.org/assignments/_6lowpan-parameters/_6lowpan-parameters.xhtml
 * @ref			https://www.ietf.org/proceedings/74/slides/6lowpan-2.pdf
 * @ref			http://wireless.ictp.it/school_2016/Slides/ICTP-Alvaro-DAY2-1-6Lowpan.pdf
 * @desc		[RFC 4944]
 * 				All LoWPAN encapsulated datagrams transported over IEEE 802.15.4 are prefixed by an
 * 				encapsulation header stack.  Each header in the header stack contains a header type
 * 				followed by zero or more header fields. Whereas in an IPv6 header the stack would
 * 				contain, in the following order, addressing, hop-by-hop options, routing,
 * 				fragmentation, destination options, and finally payload [RFC2460]; in a LoWPAN
 * 				header, the analogous header sequence is mesh (L2) addressing, hop- by-hop options
 * 				(including L2 broadcast/multicast), fragmentation, and finally payload.  These
 * 				examples show typical header stacks that may be used in a LoWPAN network.
 *
 * 				A LoWPAN encapsulated IPv6 datagram:
 *
 * 					+---------------+-------------+---------+
 * 					| IPv6 Dispatch | IPv6 Header | Payload |
 * 					+---------------+-------------+---------+
 *
 * 				A LoWPAN encapsulated LOWPAN_HC1 compressed IPv6 datagram:
 *
 * 					+--------------+------------+---------+
 * 					| HC1 Dispatch | HC1 Header | Payload |
 * 					+--------------+------------+---------+
 *
 * 				A LoWPAN encapsulated LOWPAN_HC1 compressed IPv6 datagram that requires mesh
 * 				addressing:
 *
 * 					+-----------+-------------+--------------+------------+---------+
 * 					| Mesh Type | Mesh Header | HC1 Dispatch | HC1 Header | Payload |
 * 					+-----------+-------------+--------------+------------+---------+
 *
 * 				A LoWPAN encapsulated LOWPAN_HC1 compressed IPv6 datagram that requires
 * 				fragmentation:
 *
 * 					+-----------+-------------+--------------+------------+---------+
 * 					| Frag Type | Frag Header | HC1 Dispatch | HC1 Header | Payload |
 * 					+-----------+-------------+--------------+------------+---------+
 *
 * 				A LoWPAN encapsulated LOWPAN_HC1 compressed IPv6 datagram that requires both mesh
 * 				addressing and fragmentation:
 *
 * 					+-------+-------+-------+-------+---------+---------+---------+
 * 					| M Typ | M Hdr | F Typ | F Hdr | HC1 Dsp | HC1 Hdr | Payload |
 * 					+-------+-------+-------+-------+---------+---------+---------+
 *
 * 				A LoWPAN encapsulated LOWPAN_HC1 compressed IPv6 datagram that requires both mesh
 * 				addressing and a broadcast header to support mesh broadcast/multicast:
 *
 * 					+-------+-------+-------+-------+---------+---------+---------+
 * 					| M Typ | M Hdr | B Dsp | B Hdr | HC1 Dsp | HC1 Hdr | Payload |
 * 					+-------+-------+-------+-------+---------+---------+---------+
 *
 * 				When more than one LoWPAN header is used in the same packet, they MUST appear in the
 * 				following order:
 *
 * 					Mesh Addressing Header
 * 					Broadcast Header
 * 					Fragmentation Header
 *
 * 				All protocol datagrams (e.g., IPv6, compressed IPv6 headers, etc.) SHALL be preceded
 * 				by one of the valid LoWPAN encapsulation headers, examples of which are given above.
 * 				This permits uniform software treatment of datagrams without regard to the mode of
 * 				their transmission.
 *
 * 				Pattern     Header Type
 * 				00  xxxxxx  NALP       - Not a LoWPAN frame
 * 				01  000001  IPv6       - Uncompressed IPv6 Addresses
 * 				01  000010  LOWPAN_HC1 - LOWPAN_HC1 compressed IPv6
 * 				01  000011  reserved   - Reserved for future use
 * 				  ...       reserved   - Reserved for future use
 * 				01  001111  reserved   - Reserved for future use
 * 				01  010000  LOWPAN_BC0 - LOWPAN_BC0 broadcast
 * 				01  010001  reserved   - Reserved for future use
 * 				  ...       reserved   - Reserved for future use
 * 				01  111110  reserved   - Reserved for future use
 * 				01  111111  ESC        - Additional header bytes follow
 * 				10  xxxxxx  MESH       - Mesh Header
 * 				11  000xxx  FRAG1      - Fragmentation Header (first)
 * 				11  001000  reserved   - Reserved for future use
 * 				  ...       reserved   - Reserved for future use
 * 				11  011111  reserved   - Reserved for future use
 * 				11  100xxx  FRAGN      - Fragmentation Header (subsequent)
 * 				11  101000  reserved   - Reserved for future use
 * 				  ...       reserved   - Reserved for future use
 * 				11  111111  reserved   - Reserved for future use
 *
 * 				Dispatch Type and Header
 *
 * 					 0                   1                   2                   3
 * 					 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					|0 1| Dispatch  |  type-specific header
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * 					Dispatch: 6-bit selector. Identifies the type of header immediately following the
 * 						Dispatch Header.
 *
 * 					type-specific header: A header determined by the Dispatch Header.
 *
 * 				Mesh Addressing Type and Header
 *
 * 					 0                   1                   2                   3
 * 					 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					|1 0|V|F|HopsLft| originator address, final address
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * 					V: This 1-bit field SHALL be zero if the Originator (or "Very first") Address is
 * 						an IEEE extended 64-bit address (EUI-64), or 1 if it is a short 16-bit
 * 						addresses.
 *
 * 					F: This 1-bit field SHALL be zero if the Final Destination Address is an IEEE
 * 						extended 64-bit address (EUI-64), or 1 if it is a short 16-bit addresses.
 *
 * 					Hops Left:  This 4-bit field SHALL be decremented by each forwarding node before
 * 						sending this packet towards its next hop. The packet is not forwarded any
 * 						further if Hops Left is decremented to zero. The value 0xF is reserved and
 * 						signifies an 8-bit Deep Hops Left field immediately following, and allows a
 * 						source node to specify a hop limit greater than 14 hops.
 *
 * 					Originator Address: This is the link-layer address of the Originator.
 *
 * 					Final Destination Address: This is the link-layer address of the Final
 * 						Destination.
 *
 * 				Fragmentation Type and Header
 * 				If an entire payload (e.g., IPv6) datagram fits within a single 802.15.4 frame, it is
 * 				unfragmented and the LoWPAN encapsulation should not contain a fragmentation header.
 * 				If the datagram does not fit within a single IEEE 802.15.4 frame, it SHALL be broken
 * 				into link fragments. As the fragment offset can only express multiples of eight
 * 				bytes, all link fragments for a datagram except the last one MUST be multiples of
 * 				eight bytes in length. The first link fragment SHALL contain the first fragment
 * 				header as defined below.
 *
 * 				First Fragment
 *
 * 					 0                   1                   2                   3
 * 					 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					|1 1 0 0 0|    datagram_size    |         datagram_tag          |
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * 				Subsequent Fragments
 *
 * 					 0                   1                   2                   3
 * 					 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					|1 1 1 0 0|    datagram_size    |         datagram_tag          |
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					|datagram_offset|
 * 					+-+-+-+-+-+-+-+-+
 *
 *
 *
 * 				[RFC 6282]
 * 				LOWPAN_IPHC
 *
 * 					  0                                       1
 * 					  0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5
 * 					+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 					| 0 | 1 | 1 |  TF   |NH | HLIM  |CID|SAC|  SAM  | M |DAC|  DAM  |
 * 					+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *
 * 				bits 0-2	011b
 * 				bits 3-4 	TF: 	Traffic Class, Flow Label
 * 				bit  5		NH: 	Next Header
 * 				bits 6-7	HLIM: 	Hop Limit
 * 				bit  8		CID: 	Context Identifier Extension
 * 				bit  9		SAC: 	Source Address Compression
 * 				bits 10-11	SAM: 	Source Address Mode
 * 				bit  12		M: 		Multicast Compression
 * 				bit  13		DAC:	Destination Address Compression
 * 				bits 14-15	DAM:	Destination Address Mode
 *
 * 				TF: Traffic Class, Flow Label
 * 				As specified in [RFC3168], the 8-bit IPv6 Traffic Class field is split into two
 * 				fields: 2-bit Explicit Congestion Notification (ECN) and 6-bit Differentiated
 * 				Services Code Point (DSCP).
 *
 * 					00:	ECN + DSCP + 4-bit Pad + Flow Label (4 bytes)
 * 					01:	ECN + 2-bit Pad + Flow Label (3 bytes), DSCP is elided.
 * 					10:	ECN + DSCP (1 byte), Flow Label is elided.
 * 					11:	Traffic Class and Flow Label are elided.
 *
 * 				NH: Next Header
 *
 * 					0: 	Full 8 bits for Next Header are carried in-line.
 * 					1: 	The Next Header field is compressed and the next header is encoded using
 * 						LOWPAN_NHC, which is discussed in Section 4.1.
 *
 * 				HLIM: Hop Limit
 *
 * 					00:	The Hop Limit field is carried in-line.
 * 					01:	The Hop Limit field is compressed and the hop limit is 1.
 * 					10:	The Hop Limit field is compressed and the hop limit is 64.
 * 					11:	The Hop Limit field is compressed and the hop limit is 255.
 *
 * 				CID: Context Identifier Extension
 *
 * 					0:	No additional 8-bit Context Identifier Extension is used. If context-based
 * 						compression is specified in either Source Address Compression (SAC) or
 * 						Destination Address Compression (DAC), context 0 is used.
 * 					1:	An additional 8-bit Context Identifier Extension field immediately follows
 * 						the Destination Address Mode (DAM) field.
 *
 * 				SAC: Source Address Compression
 *
 * 					0: Source address compression uses stateless compression.
 * 					1: Source address compression uses stateful, context-based compression.
 *
 * 				SAM: Source Address Mode
 * 				If SAC=0:
 *
 * 					00:	128 bits. The full address is carried in-line.
 * 					01:	64 bits. The first 64-bits of the address are elided. The value of those bits
 * 						is the link-local prefix padded with zeros. The remaining 64 bits are carried
 * 						in-line.
 * 					10:	16 bits. The first 112 bits of the address are elided. The value of the first
 * 						64 bits is the link-local prefix padded with zeros. The following 64 bits are
 * 						0000:00ff:fe00:XXXX, where XXXX are the 16 bits carried in-line.
 * 					11:	0 bits. The address is fully elided. The first 64 bits of the address are the
 * 						link-local prefix padded with zeros. The remaining 64 bits are computed from
 * 						the encapsulating header (e.g., 802.15.4 or IPv6 source address) as specified
 * 						in Section 3.2.2.
 *
 * 						+--------------------------------------------------------------+
 * 					00:	|                                                              |
 * 						+------------------------------+-------------------------------+
 * 					01:	|                              |
 * 						+------+-----------------------+
 * 					02:	|      |
 * 						+------+
 * 					03:
 *
 * 				If SAC=1:
 *
 * 					00:	The UNSPECIFIED address, ::
 * 					01:	64 bits. The address is derived using context information and the 64 bits
 * 						carried in-line. Bits covered by context information are always used. Any IID
 * 						bits not covered by context information are taken directly from the
 * 						corresponding bits carried in-line. Any remaining bits are zero.
 * 					10:	16 bits. The address is derived using context information and the 16 bits
 * 						carried in-line. Bits covered by context information are always used. Any IID
 * 						bits not covered by context information are taken directly from their
 * 						corresponding bits in the 16-bit to IID mapping given by 0000:00ff:fe00:XXXX,
 * 						where XXXX are the 16 bits carried inline. Any remaining bits are zero.
 * 					11:	0 bits. The address is fully elided and is derived using context information
 * 						and the encapsulating header (e.g., 802.15.4 or IPv6 source address). Bits
 * 						covered by context information are always used. Any IID bits not covered by
 * 						context information are computed from the encapsulating header as specified
 * 						in Section 3.2.2. Any remaining bits are zero.
 *
 * 					00:
 * 						+------------------------------+
 * 					01:	|                              |
 * 						+------+-----------------------+
 * 					02:	|      |
 * 						+------+
 * 					03:
 *
 * 				M: Multicast Compression
 *
 * 					0: Destination address is not a multicast address.
 * 					1: Destination address is a multicast address.
 *
 * 				DAC: Destination Address Compression
 *
 * 					0: Destination address compression uses stateless compression.
 * 					1: Destination address compression uses stateful, context-based compression.
 *
 * 				DAM: Destination Address Mode
 * 				If M=0 and DAC=0 This case matches SAC=0 but for the destination address:
 *
 * 					00:	128 bits. The full address is carried in-line.
 * 					01:	64 bits. The first 64-bits of the address are elided. The value of those bits
 * 						is the link-local prefix padded with zeros. The remaining 64 bits are carried
 * 						in-line.
 * 					10:	16 bits. The first 112 bits of the address are elided. The value of the first
 * 						64 bits is the link-local prefix padded with zeros. The following 64 bits are
 * 						0000:00ff:fe00:XXXX, where XXXX are the 16 bits carried in-line.
 * 					11:	0 bits. The address is fully elided. The first 64 bits of the address are the
 * 						link-local prefix padded with zeros. The remaining 64 bits are computed from
 * 						the encapsulating header (e.g., 802.15.4 or IPv6 destination address) as
 * 						specified in Section 3.2.2.
 *
 * 				If M=0 and DAC=1:
 *
 * 					00:	Reserved.
 * 					01:	64 bits. The address is derived using context information and the 64 bits
 * 						carried in-line. Bits covered by context information are always used. Any IID
 * 						bits not covered by context information are taken directly from the
 * 						corresponding bits carried in-line. Any remaining bits are zero.
 * 					10: 16 bits. The address is derived using context information and the 16 bits
 * 						carried in-line. Bits covered by context information are always used. Any IID
 * 						bits not covered by context information are taken directly from their
 * 						corresponding bits in the 16-bit to IID mapping given by 0000:00ff:fe00:XXXX,
 * 						where XXXX are the 16 bits carried inline. Any remaining bits are zero.
 * 					11:	0 bits. The address is fully elided and is derived using context information
 * 						and the encapsulating header (e.g. 802.15.4 or IPv6 destination address).
 * 						Bits covered by context information are always used. Any IID bits not covered
 * 						by context information are computed from the encapsulating header as
 * 						specified in Section 3.2.2. Any remaining bits are zero.
 *
 * 				If M=1 and DAC=0:
 *
 * 					00:	128 bits. The full address is carried in-line.
 * 					01: 48 bits. The address takes the form ffXX::00XX:XXXX:XXXX.
 * 					10: 32 bits. The address takes the form ffXX::00XX:XXXX.
 * 					11: 8 bits. The address takes the form ff02::00XX.
 *
 * 				If M=1 and DAC=1:
 *
 * 					00:	48 bits. This format is designed to match Unicast-Prefixbased IPv6 Multicast
 * 						Addresses as defined in [RFC3306] and [RFC3956]. The multicast address takes
 * 						the form ffXX:XXLL:PPPP:PPPP:PPPP:PPPP:XXXX:XXXX. where the X are the nibbles
 * 						that are carried in-line, in the order in which they appear in this format.
 * 						P denotes nibbles used to encode the prefix itself. L denotes nibbles used to
 * 						encode the prefix length. The prefix information P and L is taken from the
 * 						specified context.
 * 					01:	Reserved
 * 					10: Reserved
 * 					11: Reserved
 *
 * 				LOWPAN IPHC CID (Context Identifier)
 * 				If the CID field is set to ’1’ in the LOWPAN_IPHC encoding, then an additional octet
 * 				extends the LOWPAN_IPHC encoding following the DAM bits but before the IPv6 header
 * 				fields that are carried in-line. The additional octet identifies the pair of contexts
 * 				to be used when the IPv6 source and/or destination address is compressed. The context
 * 				identifier is 4 bits for each address, supporting up to 16 contexts. Context 0 is the
 * 				default context.
 *
 * 					  0   1   2   3   4   5   6   7
 * 					+---+---+---+---+---+---+---+---+
 * 					|      SCI      |      DCI      |
 * 					+---+---+---+---+---+---+---+---+
 *
 * 				bits 0-3	SCI Source Context Identifier. Identifies the prefix that is used when
 * 							the IPv6 source address is statefully compressed.
 * 				bits 4-7	DCI: Destination Context Identifier. Identifies the prefix that is used
 * 							when the IPv6 destination address is statefully compressed.
 *
 * 				3.2. IPv6 Header Encoding
 *   			Fields carried in-line (in part or in whole) appear in the same order as they do in
 * 				the IPv6 header format [RFC2460]. The Version field is always elided. Unicast IPv6
 * 				addresses may be compressed to 64 or 16 bits or completely elided. Multicast IPv6
 * 				addresses may be compressed to 8, 32, or 48 bits. The IPv6 Payload Length field MUST
 * 				always be elided and inferred from lower layers using the 6LoWPAN Fragmentation
 * 				header or the IEEE 802.15.4 header.
 *
 * 				3.2.1. Traffic Class and Flow Label Compression
 * 				The Traffic Class field in the IPv6 header comprises 6 bits of Diffserv extension
 * 				[RFC2474] and 2 bits of Explicit Congestion Notification (ECN) [RFC3168]. The TF
 * 				field in the LOWPAN_IPHC encoding indicates whether the Traffic Class and Flow Label
 * 				are carried in-line in the compressed IPv6 header. When Flow Label is included while
 * 				the Traffic Class is compressed, an additional 4 bits are included to maintain byte
 * 				alignment. Two of the 4 bits contain the ECN bits from the Traffic Class field.
 *
 * 				To ensure that the ECN bits appear in the same location for all encodings that
 * 				include them, the Traffic Class field is rotated right by 2 bits in the compressed
 * 				IPv6 header. The encodings are shown below:
 *
 * 					 0                   1                   2                   3
 * 					 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					|ECN|   DSCP    |  rsv  |             Flow Label                |
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					 Figure 4: TF = 00: Traffic Class and Flow Label carried in-line
 *
 * 					 0                   1                   2
 * 					 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					|ECN|rsv|             Flow Label                |
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					Figure 5: TF = 01: Flow Label carried in-line
 *
 * 					 0 1 2 3 4 5 6 7
 * 					+-+-+-+-+-+-+-+-+
 * 					|ECN|   DSCP    |
 * 					+-+-+-+-+-+-+-+-+
 * 					Figure 6: TF = 10: Traffic Class carried in-line
 *
 * 				3.2.2. Deriving IIDs from the Encapsulating Header
 * 				LOWPAN_IPHC elides the IIDs of source or destination addresses when SAM = 3 or
 * 				DAM = 3, respectively. In this mode, the IID is derived from the encapsulating
 * 				header. When the encapsulating header carries IPv6 addresses, bits for the source and
 * 				destination addresses are copied from the source and destination addresses of the
 * 				encapsulating IPv6 header.
 *
 * 				The remainder of this section defines the mapping from IEEE 802.15.4 [IEEE802.15.4]
 * 				link-layer addresses to IIDs for both short and extended IEEE 802.15.4 addresses. IID
 * 				bits not covered by the context information MAY be elided if they match the
 * 				link-layer address mapping and MUST NOT be elided if they do not. An extended IEEE
 * 				802.15.4 address takes the form of an IEEE EUI-64 address. Generating an IID from an
 * 				extended address is identical to that defined in Appendix A of [RFC4291]. The only
 * 				change needed to transform an IEEE EUI-64 identifier to an interface identifier is to
 * 				invert the universal/local bit.
 *
 * 				A short IEEE 802.15.4 address is 16 bits in length. Short addresses are mapped into
 * 				the restricted space of IEEE EUI-64 addresses by setting the middle 16 bits to
 * 				0xfffe, the bottom 16 bits to the short address, and all other bits to zero. As a
 * 				result, an IID generated from a short address has the form:
 *
 * 					0000:00ff:fe00:XXXX
 *
 * 				where XXXX carries the short address. The universal/local bit is zero to indicate
 * 				local scope. This mapping for non-EUI-64 identifiers differs from that presented in
 * 				Appendix A of [RFC4291]. Using the restricted space ensures no overlap with IIDs
 * 				generated from unrestricted IEEE EUI-64 addresses. Also, including 0xfffe in the
 * 				middle of the IID helps avoid overlap with other locally managed IIDs. This mapping
 * 				from a short IEEE 802.15.4 address to 64-bit IIDs is also used to reconstruct any
 * 				part of an IID not covered by context information.
 *
 * 				3.2.3. Stateless Multicast Address Compression
 * 				LOWPAN_IPHC supports stateless compression of multicast addresses when M = 1 and
 * 				DAC = 0. An IPv6 multicast address may be compressed down to 48, 32, or 8 bits using
 * 				stateless compression. The format supports compression of the Solicited-Node
 * 				Multicast Address (ff02::1:ffXX:XXXX) as well as any IPv6 multicast address where the
 * 				upper bits of the multicast group identifier are zero. The 8-bit compressed form only
 * 				carries the least-significant bits of the multicast group identifier. The 48- and
 * 				32-bit compressed forms carry the multicast scope and flags in-line, in addition to
 * 				the least-significant bits of the multicast group identifier.
 *
 * 					 0                   1                   2                   3
 * 					 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					| Flags | Scope |              Group Identifier                 |
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					|        Group Identifier       |
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					Figure 7: DAM = 01. 48-bit Compressed Multicast Address (ffFS::00GG:GGGG:GGGG)
 *
 * 					 0                   1                   2                   3
 * 					 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					| Flags | Scope |              Group Identifier                 |
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					Figure 8: DAM = 10. 32-bit Compressed Multicast Address (ffFS::00GG:GGGG)
 *
 * 					 0 1 2 3 4 5 6 7
 * 					+-+-+-+-+-+-+-+-+
 * 					|   Group ID    |
 * 					+-+-+-+-+-+-+-+-+
 * 					Figure 9: DAM = 11. 8-bit Compressed Multicast Address (ff02::GG)
 *
 * 				3.2.4. Stateful Multicast Address Compression
 * 				LOWPAN_IPHC supports stateful compression of multicast addresses when M = 1 and
 * 				DAC = 1. This document currently defines DAM = 00: context-based compression of
 * 				Unicast-Prefix-based IPv6 Multicast Addresses [RFC3306][RFC3956]. In particular, the
 * 				Prefix Length and Network Prefix can be taken from a context. As a result,
 * 				LOWPAN_IPHC can compress a Unicast-Prefix-based IPv6 Multicast Address down to 6
 * 				octets by only carrying the 4-bit Flags, 4-bit Scope, 8-bit Rendezvous Point
 * 				Interface ID (RIID), and 32-bit Group Identifier in-line.
 *
 * 					 0                   1                   2                   3
 * 					 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					| Flags | Scope | Rsvd / RIID   |       Group Identifier        |
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					|        Group Identifier       |
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					Figure 10: DAM = 00.  Unicast-Prefix-based IPv6 Multicast Address Compression
 *
 * 				Note that the Reserved field MUST carry the reserved bits from the multicast address
 * 				format as described in [RFC3306]. When a Rendezvous Point is encoded in the multicast
 * 				address as described in [RFC3956], the Reserved field carries the RIID bits in-line.
 *
 * 				4. IPv6 Next Header Compression
 * 				LOWPAN_IPHC elides the IPv6 Next Header field when the NH bit is set to 1. This also
 * 				indicates the use of 6LoWPAN next header compression, LOWPAN_NHC. The value of IPv6
 * 				Next Header is recovered from the first bits in the LOWPAN_NHC encoding. The
 * 				following bits are specific to the IPv6 Next Header value. Figure 11 shows the
 * 				structure of an IPv6 datagram compressed using LOWPAN_IPHC and LOWPAN_NHC.
 *
 * 					+-------------+-------------+-------------+-----------------+--------
 * 					| LOWPAN_IPHC | In-line     | LOWPAN_NHC  | In-line Next    | Payload
 * 					| Encoding    | IP Fields   | Encoding    | Header Fields   |
 * 					+-------------+-------------+-------------+-----------------+--------
 *
 * 				4.1. LOWPAN_NHC Format
 * 				Compression formats for different next headers are identified by a variable-length
 * 				bit-pattern immediately following the LOWPAN_IPHC compressed header. When defining a
 * 				next header compression format, the number of bits used SHOULD be determined by the
 * 				perceived frequency of using that format. However, the number of bits and any
 * 				remaining encoding bits SHOULD respect octet alignment. The following bits are
 * 				specific to the next header compression format. This document defines a compression
 * 				format for IPv6 Extension and UDP headers.
 *
 * 					+----------------+---------------------------
 * 					| var-len NHC ID | compressed next header...
 * 					+----------------+---------------------------
 *
 * 				4.2. IPv6 Extension Header Compression
 * 				A necessary property of encoding headers using LOWPAN_NHC is that the immediately
 * 				preceding header must be encoded using either LOWPAN_IPHC or LOWPAN_NHC. In other
 * 				words, all headers encoded using the 6LoWPAN encoding format defined in this document
 * 				must be contiguous. As a result, this document defines a set of LOWPAN_NHC encodings
 * 				for selected IPv6 Extension Headers such that the UDP Header Compression defined in
 * 				Section 4.3 may be used in the presence of those extension headers.
 *
 * 				The LOWPAN_NHC encodings for IPv6 Extension Headers are composed of a single
 * 				LOWPAN_NHC octet followed by the IPv6 Extension Header. The format of the LOWPAN_NHC
 * 				octet is shown in Figure 13. The first 7 bits serve as an identifier for the IPv6
 * 				Extension Header immediately following the LOWPAN_NHC octet. The remaining bit
 * 				indicates whether or not the following header utilizes LOWPAN_NHC encoding.
 *
 * 					  0   1   2   3   4   5   6   7
 * 					+---+---+---+---+---+---+---+---+
 * 					| 1 | 1 | 1 | 0 |    EID    |NH |
 * 					+---+---+---+---+---+---+---+---+
 *
 * 				EID: IPv6 Extension Header ID:
 *
 * 					0:	IPv6 Hop-by-Hop Options Header [RFC2460]
 * 					1:	IPv6 Routing Header [RFC2460]
 * 					2:	IPv6 Fragment Header [RFC2460]
 * 					3:	IPv6 Destination Options Header [RFC2460]
 * 					4:	IPv6 Mobility Header [RFC6275]
 * 					5:	Reserved
 * 					6:	Reserved
 * 					7:	IPv6 Header
 *
 * 				NH: Next Header:
 *
 * 					0:	Full 8 bits for Next Header are carried in-line.
 * 					1:	The Next Header field is elided and the next header is encoded using
 * 						LOWPAN_NHC, which is discussed in Section 4.1.
 *
 * 				4.3.3.  UDP LOWPAN_NHC Format
 *
 * 					  0   1   2   3   4   5   6   7
 * 					+---+---+---+---+---+---+---+---+
 * 					| 1 | 1 | 1 | 1 | 0 | C |   P   |
 * 					+---+---+---+---+---+---+---+---+
 * 					Figure 14: UDP Header Encoding
 *
 * 				C: Checksum:
 *
 * 					0: 	All 16 bits of Checksum are carried in-line.
 * 					1: 	All 16 bits of Checksum are elided. The Checksum is recovered by recomputing
 * 						it on the 6LoWPAN termination point.
 *
 * 				P: Ports:
 *
 * 					00:	All 16 bits for both Source Port and Destination Port are carried in-line.
 * 					01:	All 16 bits for Source Port are carried in-line. First 8 bits of Destination
 * 						Port is 0xf0 and elided. The remaining 8 bits of Destination Port are carried
 * 						in-line.
 * 					10:	First 8 bits of Source Port are 0xf0 and elided. The remaining 8 bits of
 * 						Source Port are carried in-line. All 16 bits for Destination Port are carried
 * 						in-line.
 * 					11:	First 12 bits of both Source Port and Destination Port are 0xf0b and elided.
 * 						The remaining 4 bits for each are carried in-line.
 *
 * 				P=00
 *
 * 					 0                   1                   2                   3
 * 					 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					|          Source Port          |           Dest Port           |
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * 				P=01
 *
 * 					 0                   1                   2
 * 					 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					|          Source Port          |   Dest Port   |
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * 				P=02
 *
 * 					 0                   1                   2
 * 					 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 					|  Source Port  |           Dest Port           |
 * 					+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * 				P=03
 *
 * 					 0 1 2 3 4 5 6 7
 * 					+-+-+-+-+-+-+-+-+
 * 					| Src   | Dest  |
 * 					+-+-+-+-+-+-+-+-+
 *
 * 				Fields carried in-line (in part or in whole) appear in the same order as they do in
 * 				the UDP header format [RFC0768]. The UDP Length field MUST always be elided and is
 * 				inferred from lower layers using the 6LoWPAN Fragmentation header or the IEEE
 * 				802.15.4 header.
 *
 * 				1110000N: IPv6 Hop-by-Hop Options Header
 * 				1110001N: IPv6 Routing Header
 * 				1110010N: IPv6 Fragment Header
 * 				1110011N: IPv6 Destination Options Header
 * 				1110100N: IPv6 Mobility Header
 * 				1110111N: IPv6 Header
 * 				11110CPP: UDP Header
 *
 ***************************************************************************************************/
#ifndef LOWPAN_H
#define LOWPAN_H

#if __STDC_VERSION__ < 199901L
#error Compile with C99 or higher!
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* Includes -------------------------------------------------------------------------------------- */
#include <stdint.h>

#include "bits.h"
#include "ieee_802_15_4.h"
// #include "ipv6.h"
// #include "range.h"


/* Public Macros --------------------------------------------------------------------------------- */
/* Public Types ---------------------------------------------------------------------------------- */
/* 	Pattern     Header Type
 * 	00  xxxxxx  NALP        - Not a LoWPAN frame
 * 	01  000001  IPv6        - Uncompressed IPv6 Addresses
 * 	01  1xxxxx  LOWPAN_IPHC -
 * 	01  111111  ESC         - Additional header bytes follow
 * 	10  xxxxxx  MESH        - Mesh Header
 * 	11  010000  FRAG        - Mist Fragmentation Header
 * 	11  011000  FRAK        - Mist Fragmentation Acknowledgement Header */
typedef enum {
	/* 00 xxxxxx */
	LOWPAN_NALP_MASK = 0xC0,

	/* 01 xxxxxx */
	LOWPAN_IPV6_MASK = 0xFF,
	LOWPAN_IPHC_MASK = 0xE0,
	LOWPAN_ESC_MASK  = 0xFF,

	/* 10 xxxxxx */
	LOWPAN_MESH_MASK = 0xC0,

	/* 11 xxxxxx */
	LOWPAN_FRAG_MASK = 0xFF,
	LOWPAN_FRAK_MASK = 0xFF,
} Lowpan_Dispatch_Mask;


typedef enum {
	/* 00 xxxxxx */
	LOWPAN_NALP = 0x0,

	/* 01 xxxxxx */
	LOWPAN_IPV6 = 0x41,
	LOWPAN_ESC  = 0x7F,
	LOWPAN_IPHC = 0x60,

	/* 10 xxxxxx */
	LOWPAN_MESH = 0x80,

	/* 11 xxxxxx */
	LOWPAN_FRAG = 0xD0,
	LOWPAN_FRAK = 0xD8,
} Lowpan_Dispatch_Types;


/* LOWPAN_IPHC
 *   0                                       1
 *   0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * | 0 | 1 | 1 |  TF   |NH | HLIM  |CID|SAC|  SAM  | M |DAC|  DAM  |
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *
 * bits 0-2		011b
 * bits 3-4 	TF: 	Traffic Class, Flow Label
 * bit  5		NH: 	Next Header
 * bits 6-7		HLIM: 	Hop Limit
 * bit  8		CID: 	Context Identifier Extension
 * bit  9		SAC: 	Source Address Compression
 * bits 10-11	SAM: 	Source Address Mode
 * bit  12		M: 		Multicast Compression
 * bit  13		DAC:	Destination Address Compression
 * bits 14-15	DAM:	Destination Address Mode */
typedef enum {
	LOWPAN_IPHC_TF_SHIFT   = 11,
	LOWPAN_IPHC_NH_SHIFT   = 10,
	LOWPAN_IPHC_HLIM_SHIFT = 8,
	LOWPAN_IPHC_CID_SHIFT  = 7,
	LOWPAN_IPHC_SAC_SHIFT  = 6,
	LOWPAN_IPHC_SAM_SHIFT  = 4,
	LOWPAN_IPHC_M_SHIFT    = 3,
	LOWPAN_IPHC_DAC_SHIFT  = 2,
	LOWPAN_IPHC_DAM_SHIFT  = 0,
} Lowpan_Iphc_Shift;

typedef enum {
	LOWPAN_IPHC_TF_MASK   = 0x3 << LOWPAN_IPHC_TF_SHIFT,
	LOWPAN_IPHC_NH_MASK   = 0x1 << LOWPAN_IPHC_NH_SHIFT,
	LOWPAN_IPHC_HLIM_MASK = 0x3 << LOWPAN_IPHC_HLIM_SHIFT,
	LOWPAN_IPHC_CID_MASK  = 0x1 << LOWPAN_IPHC_CID_SHIFT,
	LOWPAN_IPHC_SAC_MASK  = 0x1 << LOWPAN_IPHC_SAC_SHIFT,
	LOWPAN_IPHC_SAM_MASK  = 0x3 << LOWPAN_IPHC_SAM_SHIFT,
	LOWPAN_IPHC_M_MASK    = 0x1 << LOWPAN_IPHC_M_SHIFT,
	LOWPAN_IPHC_DAC_MASK  = 0x1 << LOWPAN_IPHC_DAC_SHIFT,
	LOWPAN_IPHC_DAM_MASK  = 0x3 << LOWPAN_IPHC_DAM_SHIFT,
} Lowpan_Iphc_Mask;


typedef enum {
	LOWPAN_IPHC_TF_TC_FL    = 0x0 << LOWPAN_IPHC_TF_SHIFT,	/* Traffic class and flow label inline */
	LOWPAN_IPHC_TF_FL       = 0x1 << LOWPAN_IPHC_TF_SHIFT, 	/* Flow label inline */
	LOWPAN_IPHC_TF_TC       = 0x2 << LOWPAN_IPHC_TF_SHIFT, 	/* Traffic class inline */
	LOWPAN_IPHC_TF_NONE     = 0x3 << LOWPAN_IPHC_TF_SHIFT, 	/* Traffic class and flow label elided */
	LOWPAN_IPHC_NH_INLINE   = 0x0 << LOWPAN_IPHC_NH_SHIFT,	/* Next header encoded using 8 bits */
	LOWPAN_IPHC_NH_NHC      = 0x1 << LOWPAN_IPHC_NH_SHIFT,	/* Next header encoded using NHC */
	LOWPAN_IPHC_HLIM_INLINE = 0x0 << LOWPAN_IPHC_HLIM_SHIFT,
	LOWPAN_IPHC_HLIM_1      = 0x1 << LOWPAN_IPHC_HLIM_SHIFT,
	LOWPAN_IPHC_HLIM_64     = 0x2 << LOWPAN_IPHC_HLIM_SHIFT,
	LOWPAN_IPHC_HLIM_255    = 0x3 << LOWPAN_IPHC_HLIM_SHIFT,
	LOWPAN_IPHC_CID_NONE    = 0x0 << LOWPAN_IPHC_CID_SHIFT,	/* No 8-bit CID extension field */
	LOWPAN_IPHC_CID_EXT     = 0x1 << LOWPAN_IPHC_CID_SHIFT,	/* Addition 8-bit CID extension field */

	/* Source Address Compression */
	LOWPAN_IPHC_SAC_STATELESS = 0x0 << LOWPAN_IPHC_SAC_SHIFT,
	LOWPAN_IPHC_SAC_STATEFUL  = 0x1 << LOWPAN_IPHC_SAC_SHIFT,

	/* SAC=0: Stateless (SL) Source Addressing Mode */
	LOWPAN_IPHC_SAM_SL_128 = 0x0 << LOWPAN_IPHC_SAM_SHIFT,
	LOWPAN_IPHC_SAM_SL_64  = 0x1 << LOWPAN_IPHC_SAM_SHIFT,
	LOWPAN_IPHC_SAM_SL_16  = 0x2 << LOWPAN_IPHC_SAM_SHIFT,
	LOWPAN_IPHC_SAM_SL_0   = 0x3 << LOWPAN_IPHC_SAM_SHIFT,

	/* SAC=1: Stateful (SF) Source Addressing Mode */
	LOWPAN_IPHC_SAM_SF_UNSPEC = 0x0 << LOWPAN_IPHC_SAM_SHIFT,
	LOWPAN_IPHC_SAM_SF_64     = 0x1 << LOWPAN_IPHC_SAM_SHIFT,
	LOWPAN_IPHC_SAM_SF_16     = 0x2 << LOWPAN_IPHC_SAM_SHIFT,
	LOWPAN_IPHC_SAM_SF_0      = 0x3 << LOWPAN_IPHC_SAM_SHIFT,

	LOWPAN_IPHC_SAM_64        = 0x1 << LOWPAN_IPHC_SAM_SHIFT,
	LOWPAN_IPHC_SAM_16        = 0x2 << LOWPAN_IPHC_SAM_SHIFT,
	LOWPAN_IPHC_SAM_0         = 0x3 << LOWPAN_IPHC_SAM_SHIFT,

	/* Multicast Address Compression */
	LOWPAN_IPHC_M_NOT_MULTICAST = 0x0 << LOWPAN_IPHC_M_SHIFT,
	LOWPAN_IPHC_M_MULTICAST     = 0x1 << LOWPAN_IPHC_M_SHIFT,

	/* Destination Address Compression */
	LOWPAN_IPHC_DAC_STATELESS = 0x0 << LOWPAN_IPHC_DAC_SHIFT,
	LOWPAN_IPHC_DAC_STATEFUL  = 0x1 << LOWPAN_IPHC_DAC_SHIFT,

	/* M=0, DAC=0: Not multicast, stateless (SL) destination addressing */
	LOWPAN_IPHC_DAM_SL_128 = 0x0 << LOWPAN_IPHC_DAM_SHIFT,
	LOWPAN_IPHC_DAM_SL_64  = 0x1 << LOWPAN_IPHC_DAM_SHIFT,
	LOWPAN_IPHC_DAM_SL_16  = 0x2 << LOWPAN_IPHC_DAM_SHIFT,
	LOWPAN_IPHC_DAM_SL_0   = 0x3 << LOWPAN_IPHC_DAM_SHIFT,

	/* M=0, DAC=1: Not multicast, stateful (SF) destination addressing */
	LOWPAN_IPHC_DAM_SF_64 = 0x1 << LOWPAN_IPHC_DAM_SHIFT,
	LOWPAN_IPHC_DAM_SF_16 = 0x2 << LOWPAN_IPHC_DAM_SHIFT,
	LOWPAN_IPHC_DAM_SF_0  = 0x3 << LOWPAN_IPHC_DAM_SHIFT,

	LOWPAN_IPHC_DAM_64 = 0x1 << LOWPAN_IPHC_DAM_SHIFT,
	LOWPAN_IPHC_DAM_16 = 0x2 << LOWPAN_IPHC_DAM_SHIFT,
	LOWPAN_IPHC_DAM_0  = 0x3 << LOWPAN_IPHC_DAM_SHIFT,

	/* M=1, DAC=0: Multicast, stateless (SL) destination addressing */
	LOWPAN_IPHC_DAM_MSL_128 = 0x0 << LOWPAN_IPHC_DAM_SHIFT,	/* Full 128-bit address inline */
	LOWPAN_IPHC_DAM_MSL_48  = 0x1 << LOWPAN_IPHC_DAM_SHIFT,	/* ffXX::00XX:XXXX:XXXX */
	LOWPAN_IPHC_DAM_MSL_32  = 0x2 << LOWPAN_IPHC_DAM_SHIFT,	/* ffXX::00XX:XXXX */
	LOWPAN_IPHC_DAM_MSL_8   = 0x3 << LOWPAN_IPHC_DAM_SHIFT,	/* ff02::00XX */

	/* M=1, DAC=1: Multicast (M), stateful (SF) destination addressing */
	LOWPAN_IPHC_DAM_MSF_48 = 0x0 << LOWPAN_IPHC_DAC_SHIFT,
} Lowpan_Iphc_Types;


/* LOWPAN IPHC CID (Context Identifier)
 * If the CID field is set to ’1’ in the LOWPAN_IPHC encoding, then an additional octet extends the
 * LOWPAN_IPHC encoding following the DAM bits but before the IPv6 header fields that are carried
 * in-line. The additional octet identifies the pair of contexts to be used when the IPv6 source
 * and/or destination address is compressed. The context identifier is 4 bits for each address,
 * supporting up to 16 contexts. Context 0 is the default context.
 *
 * 		  0   1   2   3   4   5   6   7
 * 		+---+---+---+---+---+---+---+---+
 * 		|      SCI      |      DCI      |
 * 		+---+---+---+---+---+---+---+---+
 *
 * 		bits 0-3	SCI Source Context Identifier. Identifies the prefix that is used when the IPv6
 * 					source address is statefully compressed.
 * 		bits 4-7	DCI: Destination Context Identifier. Identifies the prefix that is used when the
 * 					IPv6 destination address is statefully compressed. */
typedef enum {
	LOWPAN_IPHC_CID_SCI_SHIFT = 4,
	LOWPAN_IPHC_CID_DCI_SHIFT = 0,
} Lowpan_Iphc_Cid_Shift;

typedef enum {
	LOWPAN_IPHC_CID_SCI_MASK = 0xF << LOWPAN_IPHC_CID_SCI_SHIFT,
	LOWPAN_IPHC_CID_DCI_MASK = 0xF << LOWPAN_IPHC_CID_DCI_SHIFT,
} Lowpan_Iphc_Cid_Mask;


/* 	IPv6 Extension Header Compression
 *
 * 		  0   1   2   3   4   5   6   7
 * 		+---+---+---+---+---+---+---+---+
 * 		| 1 | 1 | 1 | 0 |    EID    |NH |
 * 		+---+---+---+---+---+---+---+---+
 *
 * 	EID: IPv6 Extension Header ID:
 *
 * 		0:	IPv6 Hop-by-Hop Options Header [RFC2460]
 * 		1:	IPv6 Routing Header [RFC2460]
 * 		2:	IPv6 Fragment Header [RFC2460]
 * 		3:	IPv6 Destination Options Header [RFC2460]
 * 		4:	IPv6 Mobility Header [RFC6275]
 * 		5:	Reserved
 * 		6:	Reserved
 * 		7:	IPv6 Header
 *
 * 	NH: Next Header:
 *
 * 		0:	Full 8 bits for Next Header are carried in-line.
 * 		1:	The Next Header field is elided and the next header is encoded using
 * 			LOWPAN_NHC, which is discussed in Section 4.1. */
typedef enum {
	LOWPAN_EXT_HDR_TYPE_SHIFT = 4,
	LOWPAN_EXT_HDR_EID_SHIFT  = 1,
	LOWPAN_EXT_HDR_NH_SHIFT   = 0,
} Lowpan_Ext_Hdr_Shift;

typedef enum {
	LOWPAN_EXT_HDR_TYPE_MASK = 0xF << LOWPAN_EXT_HDR_TYPE_SHIFT,
	LOWPAN_EXT_HDR_EID_MASK  = 0x7 << LOWPAN_EXT_HDR_EID_SHIFT,
	LOWPAN_EXT_HDR_NH_MASK   = 0x1 << LOWPAN_EXT_HDR_NH_SHIFT,
} Lowpan_Ext_Hdr_Mask;

typedef enum {
	LOWPAN_EXT_HDR_TYPE = 0xE0,
	LOWPAN_EXT_HDR_HBH       = 0x0 << LOWPAN_EXT_HDR_EID_SHIFT,
	LOWPAN_EXT_HDR_ROUTING   = 0x1 << LOWPAN_EXT_HDR_EID_SHIFT,
	LOWPAN_EXT_HDR_FRAG      = 0x2 << LOWPAN_EXT_HDR_EID_SHIFT,
	LOWPAN_EXT_HDR_DEST_OPTS = 0x3 << LOWPAN_EXT_HDR_EID_SHIFT,
	LOWPAN_EXT_HDR_MOBILITY  = 0x4 << LOWPAN_EXT_HDR_EID_SHIFT,
	LOWPAN_EXT_HDR_IPV6      = 0x7 << LOWPAN_EXT_HDR_EID_SHIFT,
	LOWPAN_EXT_HDR_INLINE    = 0x0 << LOWPAN_EXT_HDR_NH_SHIFT,
	LOWPAN_EXT_HDR_NHC       = 0x1 << LOWPAN_EXT_HDR_NH_SHIFT,
} Lowpan_Ext_Hdr_Types;


typedef struct {
	Ieee154_Frame* frame;
	uint8_t* start;		/* Start index of this 6LOWPAN dispatch header in the frame. */
	uint8_t* end;		/* End index of this 6LOWPAN dispatch header in the frame. */
} Lowpan;


typedef enum {
	LOWPAN_CTX_LINK_LOCAL = 0,
} Lowpan_Context_Ids;


/* Public Functions ------------------------------------------------------------------------------ */
void     lowpan_ctx_init  (void);
void     lowpan_ctx_clear (void);
unsigned lowpan_ctx_count (void);
bool     lowpan_ctx_put   (uint8_t, const struct in6_addr*);
bool     lowpan_ctx_get   (uint8_t, struct in6_addr*);
bool     lowpan_ctx_remove(uint8_t);
struct in6_addr* lowpan_ctx_search_id  (uint8_t);
uint8_t          lowpan_ctx_search_addr(const struct in6_addr*, unsigned, unsigned);


unsigned lowpan_compress(struct net_pkt* pkt, Bits* frags, uint32_t fragid, Ieee154_Frame* frame);
struct net_pkt* lowpan_decompress(struct net_if* iface, Ieee154_Frame* frame);
// bool     lowpan_decompress(struct net_pkt* pkt, Bits* frags, Ieee154_Frame* frame);


#ifdef __cplusplus
}
#endif

#endif // LOWPAN_H
/******************************************* END OF FILE *******************************************/
