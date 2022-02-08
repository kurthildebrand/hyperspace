/************************************************************************************************//**
 * @file		ieee_802_15_4.h
 *
 * @copyright	Copyright 2022 Kurt Hildebrand.
 * @license		Licensed under the Apache License, Version 2.0 (the "License"); you may not use this
 * 				file except in compliance with the License. You may obtain a copy of the License at
 *
 * 				http://www.apache.org/licenses/LICENSE-2.0
 *
 * 				Unless required by applicable law or agreed to in writing, software distributed under
 * 				the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF
 * 				ANY KIND, either express or implied. See the License for the specific language
 * 				governing permissions and limitations under the License.
 *
 * @brief		This file implements building and parsing IEEE 802.15.4 frames.
 * @TODO		switch to write/read functions instead of random access.
 * @TODO		Add
 *
 * 					bool ieee154_prepend_hie(Ieee154_IE*, uint16_t, const void*, unsigned);
 * 					bool ieee154_prepend_pie(Ieee154_IE*, uint16_t, const void*, unsigned);
 * 					bool ieee154_prepend_nie(Ieee154_IE*, uint16_t, const void*, unsigned);
 *
 * @TODO		Security (ASH).
 * @note		Bit ordering: see https://mailarchive.ietf.org/arch/msg/6tisch/2pXzQH7n_6EjvVo05QlOXnzSG3c
 * @note		This file for little-endian procesors only.
 * @desc		Reading Frame Options:
 * 				1.	Initialize frame and buffer, read into buffer, parse
 *
 * 					ieee154_frame_init
 * 					ieee154_set_length
 * 					ieee154_parse
 *
 * 				2.	Read into buffer, initialize frame and length, parse
 *
 * 					memmove(data, radio, flen)
 * 					ieee154_frame_init
 * 					ieee154_parse
 *
 * 				[IEEE802.15.4]
 * 				-------------------------------------------------------------------------------------
 * 				Frame Structure
 * 				-------------------------------------------------------------------------------------
 * 				The MAC frames are passed to the PHY as the PHY service data unit (PSDU), which
 * 				becomes the PHY payload. Information Elements (IEs) are used to transfer formatted
 * 				data between layers and between devices. IEs consist of an identification, a length,
 * 				and the IE content. Devices can accept or discard a particular element if the ID is
 * 				known, and skip over elements with unknown IDs.
 *
 * 				+-------------+-----+------------+---------------------------------------+
 * 				| Sync Header | SFD | PHY Header | PHY Payload                           |
 * 				| SHR         |     | PHR        | PSDU                                  |
 * 				+-------------+-----+------------+------------+-------------+------------+
 * 				                                 | MAC Header | MAC Payload | MAC Footer |
 * 				                                 | MHR        |             | MFR        |
 * 				                                 +------------+-------------+------------+
 *
 *
 * 				-------------------------------------------------------------------------------------
 * 				General IEEE 802.15.4 MAC Frame Format
 * 				-------------------------------------------------------------------------------------
 * 				The general MAC frame for Frame Type values other than fragment and extended, as
 * 				defined in Table 7-1 shall be formatted as illustrated:
 *
 * 				+------------------------------------------------------------+---------------+-----+
 * 				| MHR                                                        | MAC Payload   | MFR |
 * 				+-------------+----------------------------------+-----+-----+-----+---------+-----+
 * 				|             | Addressing Fields                |     | HIE | PIE |         |     |
 * 				+-------+-----+--------+-------+--------+--------+-----+-----+-----+---------+-----+
 * 				| Frame | Seq | Dest   | Dest  | Source | Source | Aux | IE        | Payload | FCS |
 * 				| Ctrl  | Num | PAN ID | Addr  | PAN ID | Addr   | Sec |           |         |     |
 * 				+=======+=====+========+=======+========+========+=====+=====+=====+=========+=====+
 * 				| 1/2   | 0/1 | 0/2    | 0/2/8 | 0/2    | 0/2/8  | var | var       | var     | 2/4 |
 * 				+=======+=====+========+=======+========+========+=====+===========+=========+=====+
 *
 * 				2 Bytes             Frame Control                   MHR
 * 				1 Byte              Sequence Number                 MHR
 * 				0/2 Bytes           Destination PAN Identifier      MHR
 * 				0/2/8 Bytes         Destination Address             MHR
 * 				0/2 Bytes           Source PAN Identifier           MHR
 * 				0/2/8 Bytes         Source Address                  MHR
 * 				0/5/6/10/14 Bytes   Auxiliary Security Header       MHR
 * 				Variable Bytes      Header IE                       MHR         IE
 * 				Variable Bytes      Payload IE                      MAC Payload IE
 * 				Variable Bytes      Payload                         MAC Payload
 * 				2/4 Bytes           FCS                             MFR
 *
 * 				Frame Control Field:
 * 				bit 0-2             Frame Type
 * 				bit 3               Security Enabled
 * 				bit 4               Frame Pending
 * 				bit 5               Acknowledge Request
 * 				bit 6               PAN ID Compression
 * 				bit 7               Reserved
 * 				bit 8               Sequence Number Suppression
 * 				bit 9               Information Element (IE) Present Field
 * 				bit 10-11           Destination Addressing Mode
 * 				bit 12-13           Frame Version
 * 				bit 14-15           Source Addressing Mode
 *
 * 				Frame Type Field:
 *				000     Beacon
 *				001     Data
 *				010     Acknowledgment
 *				011     MAC command
 *				100     Reserved
 *				101     Multipurpose
 *				110     Fragment or Fraka
 *				111     Extended
 *
 * 				Security Enabled Field:
 * 				0   Frame not protected by MAC sublayer.
 * 				1   Frame protected by MAC sublayer. Aux Security header is present.
 *
 * 				Frame Pending Field:
 * 				0   Device does not have more data to transmit.
 * 				1   Device does have more data to transmit.
 *
 * 				Acknowledgment Request Field:
 *				The AR field specifies whether an acknowledgment is required from the recipient
 *				device on receipt of a data or MAC command frame. If this field is set to one, the
 *				recipient device shall send an acknowledgment frame only if, upon reception, the
 *				frame passes the filtering. If this field is set to zero, the recipient device shall
 *				not send an acknowledgment frame.
 *
 * 				PAN ID Compression Field:
 * 				Dest. Addr.     Source Addr.    Dest PAN ID     Source PAN ID   PAN ID Compression
 * 				Not Present     Not Present     Not Present     Not Present     0
 * 				Not Present     Not Present     Present         Not Present     1
 * 				Not Present     Not Present     Not Present     Present            invalid
 * 				Not Present     Not Present     Present         Present            invalid
 * 				Short           Not Present     Present         Not Present     0
 * 				Extended        Not Present     Present         Not Present     0
 * 				Short           Not Present     Not Present     Not Present     1
 * 				Extended        Not Present     Not Present     Not Present     1
 * 				Short           Not Present     Present         Present            invalid
 * 				Extended        Not Present     Present         Present            invalid
 * 				Short           Not Present     Not Present     Present            invalid
 * 				Extended        Not Present     Not Present     Present            invalid
 * 				-------------------------------------------------------------------------------------
 * 				Not Present     Short           Not Present     Present         0
 * 				Not Present     Extended        Not Present     Present         0
 * 				Not Present     Short           Not Present     Not Present     1
 * 				Not Present     Extended        Not Present     Not Present     1
 * 				Not Present     Short           Present         Present            invalid
 * 				Not Present     Extended        Present         Present            invalid
 * 				Not Present     Short           Present         Not Present        invalid
 * 				Not Present     Extended        Present         Not Present        invalid
 * 				-------------------------------------------------------------------------------------
 * 				Extended        Extended        Present         Not Present     0
 * 				Extended        Extended        Not Present     Not Present     1
 * 				Extended        Extended        Present         Present            invalid
 * 				Extended        Extended        Not Present     Present            invalid
 * 				Short           Short           Present         Present         0
 * 				Short           Extended        Present         Present         0
 * 				Extended        Short           Present         Present         0
 * 				Short           Short           Present         Not Present     1
 * 				Short           Extended        Present         Not Present     1
 * 				Extended        Short           Present         Not Present     1
 * 				Short           Short           Not Present     Present            invalid
 * 				Short           Extended        Not Present     Present            invalid
 * 				Extended        Short           Not Present     Present            invalid
 * 				Short           Short           Not Present     Not Present        invalid
 * 				Short           Extended        Not Present     Not Present        invalid
 * 				Extended        Short           Not Present     Not Present        invalid
 *
 *				Sequence Number Suppression:
 *				0       Sequence Number field is present (always 0 is frame version is 0b00, 0b01).
 *				1       Sequence Number field is omitted.
 *
 *				IE Present Field:
 *				0       Frame does not contain IEs (always 0 if frame version is 0b00, 0b01).
 *				1       Frame does contain IEs.
 *
 *				Destination Addressing Mode Field:
 *				00      PAN identifier and address fields are not present
 *				01      Reserved
 *				10      Address field contains short (16-bit) address
 *				11      Address field contains extended (64-bit) address
 *
 * 				Frame Version Field:
 * 				                0b00            0b01            0b10        0b11
 *				Beacon          802.15.4-2003   802.15.4-2006   802.15.4    Reserved
 *				Data            802.15.4-2003   802.15.4-2006   802.15.4    Reserved
 *				Acknowledgment  802.15.4-2003   802.15.4-2006   802.15.4    Reserved
 *				MAC Command     802.15.4-2003   802.15.4-2006   802.15.4    Reserved
 *				Reserved        —               —               —           —
 *				Multipurpose    802.15.4        Reserved        Reserved    Reserved
 *				Fragment        Frame Version field not present in frame
 *				Extended        Frame Version field not present in frame
 *
 * 				Source Addressing Mode Field:
 *				00      PAN identifier and address fields are not present
 *				01      Reserved
 *				10      Address field contains short (16-bit) address
 *				11      Address field contains extended (64-bit) address
 *
 * 				Sequence Number Field:
 *				The Sequence Number field specifies the sequence identifier for the frame. For a
 *				beacon frame, the Sequence Number field shall specify a BSN. For a data,
 *				acknowledgment, or MAC command frame, the Sequence Number field shall specify a DSN
 *				that is used to match an acknowledgment frame to the data or MAC command frame.
 *
 * 				Destination PAN Identifier Field:
 *				The Destination PAN Identifier field, when present, specifies the unique PAN
 *				identifier of the intended recipient of the frame. A value of 0xffff in this field
 *				shall represent the broadcast PAN identifier, which shall be accepted as a valid PAN
 *				identifier by all devices currently listening to the channel. This field shall be
 *				included in the MAC frame only if the Destination Addressing Mode field is nonzero.
 *
 * 				Destination Address Field:
 *				The Destination Address field, when present, specifies the address of the intended
 *				recipient of the frame. A value of 0xffff in this field shall represent the broadcast
 *				short address, which shall be accepted as a valid address by all devices currently
 *				listening to the channel. This field shall be included in the MAC frame only if the
 *				Destination Addressing Mode field is nonzero.
 *
 * 				Source PAN Identifier Field:
 *				The Source PAN Identifier field, when present, specifies the unique PAN identifier of
 *				the originator of the frame. This field shall be included in the MAC frame only if
 *				the Source Addressing Mode field is nonzero and the PAN ID Compression field is equal
 *				to zero. The PAN identifier of a device is initially determined during association on
 *				a PAN but may change following a PAN identifier conflict resolution.
 *
 *				Source Address Field:
 *				The Source Address field, when present, specifies the address of the originator of
 *				the frame. This field shall be included in the MAC frame only if the Source
 *				Addressing Mode field is nonzero.
 *
 * 				Auxiliary Security Header Field:
 *				The Auxiliary Security Header field specifies information required for security
 *				processing. This field shall be present only if the Security Enabled field is set to
 *				one. The formatting of the Auxiliary Security Header field is described in 7.4.
 *
 *				IE Field:
 *				The IE field is variable length and contains one or more IE. This field is comprised
 *				of the Header IE and Payload IE subfields. This field shall be present only if the IE
 *				Present field in the Frame Control field is set to one. Each IE consists of a
 *				descriptor and an optional payload. Header IEs, if present, follow the Auxiliary
 *				Security Header and are part of the MHR. Header IEs, if present, may require
 *				termination.
 *
 *				Payload IEs, if present, follow the MHR and are considered part of the MAC payload,
 *				i.e., they may be encrypted. A set of payload IEs may require termination.
 *
 * 				Frame Payload Field:
 *				The Frame Payload field contains information specific to individual frame types. If
 *				the Security Enabled field is set to one, the frame payload the frame may be
 *				cryptographically protected, as described in Clause 7.
 *
 * 				FCS Field:
 *				The FCS field contains a 16-bit ITU-T CRC. The FCS is calculated over the MHR and MAC
 *				payload parts of the frame. The FCS shall be calculated using the following standard
 *				generator polynomial of degree 16:
 *
 *					G16(x) = x^16 + x^12 + x^5 + 1
 *
 *
 *
 *
 * 				-------------------------------------------------------------------------------------
 *				Frame Format Types
 *				-------------------------------------------------------------------------------------
 *				Beacon Frame:
 *				2 Bytes             Frame Control                   MHR
 *				1 Byte              Sequence Number                 MHR
 *				4/10 Bytes          Addressing Fields               MHR
 *				Variable Bytes      Auxiliary Security Fields       MHR
 *				2 Bytes             Superframe Specification        MAC Payload
 *				Variable Bytes      GTS Fields                      MAC Payload
 *				Variable Bytes      Pending Address Fields          MAC Payload
 *				Variable Bytes      Payload                         MAC Payload
 *				2 Bytes             FCS                             MFR
 *
 * 				Enhanced Beacon Frame: (version 0b10)
 * 				2 Bytes             Frame Control                   MHR
 * 				0/1 Byte            Sequence Number                 MHR
 * 				Variable Bytes      Addressing Fields               MHR
 * 				Variable Bytes      Auxiliary Security Fields       MHR
 * 				Variable Bytes      Header IE Fields                MHR
 * 				Variable Bytes      Payload IE Fields               MAC Payload
 * 				Variable Bytes      Beacon Payload                  MAC Payload
 * 				2/4 Bytes           FCS                             MFR
 *
 * 				Data Frame:
 * 				2 Bytes             Frame Control                   MHR
 * 				0/1 Bytes           Sequence Number                 MHR
 * 				Variable Bytes      Addressing Fields               MHR
 * 				Variable Bytes      Auxiliary Security Fields       MHR
 * 				Variable Bytes      Header IE Fields                MHR
 * 				Variable Bytes      Payload IE Fields               MAC Payload
 * 				Variable Bytes      Data Payload                    MAC Payload
 * 				2/4 Bytes           FCS                             MFR
 *
 * 				Imm Ack Frame:
 * 				2 Bytes             Frame Control                   MHR
 * 				1 Byte              Sequence Number                 MHR
 * 				2/4 Bytes           FCS                             MFR
 *
 * 				Enh Ack Frame: (version number 0b10)
 * 				2 Bytes             Frame Control                   MHR
 * 				0/1 Bytes           Sequence Number                 MHR
 * 				0/2 Bytes           Dest. PAN ID                    MHR
 * 				0/2/8 Bytes         Dest. Address                   MHR
 * 				0/2/ Bytes          Source PAN ID                   MHR
 * 				0/2/8 Bytes         Source Address                  MHR
 * 				Variable Bytes      Auxiliary Security Fields       MHR
 * 				Variable Bytes      Header IE Fields                MHR
 * 				Variable Bytes      Payload IE Fields               MAC Payload
 * 				Variable Bytes      Payload                         MAC Payload
 * 				2/4 Bytes           FCS                             MFR
 *
 * 				MAC Command Frame:
 * 				2 Bytes             Frame Control                   MHR
 * 				0/1 Bytes           Sequence Number                 MHR
 * 				Variable Bytes      Addressing Fields               MHR
 * 				Variable Bytes      Auxiliary Security Fields       MHR
 * 				Variable Bytes      Header IE Fields                MHR
 * 				Variable Bytes      Payload IE Fields               MAC Payload
 * 				1 Byte              Command ID                      MAC Payload
 * 				Variable Bytes      Payload                         MAC Payload
 * 				2/4                 FCS                             MFR
 *
 * 				MAC Command IDs
 * 				Command ID  Command name                            RFD
 *				0x01        Association Request command             TX
 *				0x02        Association Response command            RX
 *				0x03        Disassociation Notification command     TX RX
 *				0x04        Data Request command                    TX
 *				0x05        PAN ID Conflict Notification command    TX
 *				0x06        Orphan Notification command             TX
 *				0x07        Beacon Request command
 *				0x08        Coordinator realignment command            RX
 *				0x09        GTS request command
 *				0x0a        TRLE Management Request command
 *				0x0b        TRLE Management Response command
 *				0x0c–0x12   Reserved
 *				0x13        DSME Association Request command
 *				0x14        DSME Association Response command
 *				0x15        DSME GTS Request command
 *				0x16        DSME GTS Response command
 *				0x17        DSME GTS Notify command
 *				0x18        DSME Information Request command
 *				0x19        DSME Information Response command
 *				0x1a        DSME Beacon Allocation Notification command
 *				0x1b        DSME Beacon Collision Notification command
 *				0x1c        DSME Link Report command
 *				0x1d–0x1f   Reserved
 *				0x20        RIT Data Request command
 *				0x21        DBS Request command
 *				0x22        DBS Response command
 *				0x23        RIT Data Response command
 *				0x24        Vendor Specific command
 *				0x25–0xff   Reserved
 *
 * 				Multipurpose Frame:
 * 				1/2 Bytes           MP  Frame Control               MHR
 * 				0/1 Bytes           Sequence Number                 MHR
 * 				0/2 Bytes           Destination PAN ID              MHR
 * 				0/2/8 Bytes         Destination Address             MHR
 * 				0/2 Bytes           Source PAN ID                   MHR
 * 				0/2/8 Bytes         Source Address                  MHR
 * 				Variable Bytes      Auxiliary Security Fields       MHR
 * 				Variable Bytes      Header IE Fields                MHR
 * 				Variable Bytes      Payload IE Fields               MAC Payload
 * 				Variable Bytes      Payload                         MAC Payload
 * 				2/4 Bytes           FCS                             MFR
 *
 * 				MP Frame Control Bits:
 * 				bits 0-2            Frame Type
 * 				bit 3               Long Frame Control
 * 				bit 4-5             Destination Addressing Mode
 * 				bit 6-7             Source Addressing Mode
 * 				bit 8               PAN ID Present
 * 				bit 9               Security Enabled
 * 				bit 10              Sequence Number Suppression
 * 				bit 11              Frame Pending
 * 				bit 12-13           Frame Version
 * 				bit 14              ACK Request
 * 				bit 15              IE Present
 *
 *
 * 				-------------------------------------------------------------------------------------
 * 				Auxiliary Security Header
 * 				-------------------------------------------------------------------------------------
 * 				1 Byte              Security Control
 * 				0/4 Bytes           Frame Counter
 * 				0/1/5/9 Bytes       Key Identifier
 *
 * 				Security Control Field:
 * 				bits 0-2            Security Level
 * 				bits 3-4            Key Identifier Mode
 * 				bit 5               Frame Counter Suppression
 * 				bit 6               ASN in nonce
 * 				bit 7               reserved
 *
 * 				Security Level:
 * 				Security level  Attributes      Confidentiality     Authenticity    Length (octets)
 *				0 (000)         None            OFF                 NO              0
 *				1 (001)         MIC-32          OFF                 YES             4
 *				2 (010)         MIC-64          OFF                 YES             8
 *				3 (011)         MIC-128         OFF                 YES             16
 *				4 (100)
 *				5 (101)         ENC-MIC-32      ON                  YES             4
 *				6 (110)         ENC-MIC-64      ON                  YES             8
 *				7 (111)         ENC-MIC-128     ON                  YES             16
 *
 * 				Key Identifier Mode:
 * 				0x00            Key is determined implicitly from the originator    0
 * 				                and recipient(s) of the frame, as indicated in the
 * 				                frame header.
 * 				0x01            Key is determined from the Key Index field.         1
 * 				0x02            Key is determined explicitly from the 4-octet Key   5
 * 				                Source field and the Key Index field.
 * 				0x03            Key is determined explicitly from the 8-octet Key   9
 * 				                Source field and the Key Index field.
 *
 *
 * 				-------------------------------------------------------------------------------------
 * 				IEs
 * 				-------------------------------------------------------------------------------------
 * 				IE List Termination:
 * 				Header Termination 1 IE is present if Payload IEs immediately follow Header IEs.
 * 				Header Termination 2 IE is present if Payload only (no Payload IEs) follow Header
 * 				IEs.
 *
 * 				Header   | Payload | Data    | HT IE   | PT IE   | Notes
 * 				IEs      | IEs     | Payload | Present | Present |
 * 				Present  |         | Present |         |         |
 * 				=========+=========+=========+=========+=========+===================================
 * 				No       | No      | No      | No      | No      | No IE lists present, hence no
 * 				         |         |         |         |         | termination.
 * 				---------|---------|---------|---------|---------|-----------------------------------
 * 				Yes      | No      | No      | No      | No      | No termination is required because
 * 				         |         |         |         |         | the end of the frame can be
 * 				         |         |         |         |         | determined from the frame length
 * 				         |         |         |         |         | and FCS length.
 * 				---------|---------|---------|---------|---------|-----------------------------------
 * 				No       | Yes     | No      | HT1     | Opt.    | Header Termination IE 1 is
 * 				         |         |         |         |         | required to signal end of the MHR
 * 				         |         |         |         |         | and beginning of the Payload IE
 * 				         |         |         |         |         | list.
 * 				---------|---------|---------|---------|---------|-----------------------------------
 * 				Yes      | Yes     | No      | HT1     | Opt.    | Header IE Termination 1 is
 * 				         |         |         |         |         | required while the Payload IE
 * 				         |         |         |         |         | Termination is not required, but
 * 				         |         |         |         |         | is allowed.
 * 				         |         |         |         |         |
 * 				---------|---------|---------|---------|---------|-----------------------------------
 * 				No       | No      | Yes     | No      | No      | No IE lists present, hence no
 * 				         |         |         |         |         | termination.
 * 				---------|---------|---------|---------|---------|-----------------------------------
 * 				Yes      | No      | Yes     | HT2     | No      | Header Termination IE 2 is used in
 * 				         |         |         |         |         | to signal end of the MHR and
 * 				         |         |         |         |         | beginning of the MAC Payload.
 * 				---------|---------|---------|---------|---------|-----------------------------------
 * 				No       | Yes     | Yes     | HT1     | PT      |
 * 				---------|---------|---------|---------|---------|-----------------------------------
 * 				Yes      | Yes     | Yes     | HT1     | PT      |
 * 				---------|---------|---------|---------|---------|-----------------------------------
 *
 * 				Header IE:
 * 				bits 0-6            Length (terminate list by setting length to 0).
 * 				bits 7-14           Element ID
 * 				bit  15             Type = 0
 * 				Variable Bytes      Content
 *
 * 				Payload IE:
 * 				bits 0-10           Length (terminate list by setting length to 0).
 * 				bits 11-14          Group ID
 * 				bit  15             Type = 1
 * 				Variable Bytes      Content
 *
 *
 * 				7.4.2 Header IEs
 * 				Element ID  | Name               |
 * 				============+====================+=======+==========+==================+======+======
 * 				0x00        | Vendor Specific    | XXXXX | 7.4.2.2  | 6.12.2           | UL   | MAC
 * 				            | Header IE          |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x01–0x19   | Reserved           | ----- | -        | -                | -    | -
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x1A        | CSL IE             | XXXX- | 7.4.2.3  | 6.12.2           | MAC  | MAC
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x1B        | RIT IE             | X-X-X | 7.4.2.4  | 6.12.3           | MAC  | MAC
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x1C        | DSME PAN           | X---- | 7.4.2.5  | 6.11.2           | UL   | UL
 * 				            | Descriptor IE      |       |          |                  | MAC  |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x1D        | Rendezvous Time IE | -X-X- | 7.4.2.6  | 6.12.2           | MAC  | MAC
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x1E        | Time Correction IE | -X--- | 7.4.2.7  | 6.5.4.1, 6.7.4.2 | MAC  | MAC
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x1F-0x20   | Reserved
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x21        | Extended DSME PAN  | x---- | 7.4.2.8  | 6.11.2           | ULC  | MAC
 * 				            | Descriptor IE      |       |          |                  | MAC  |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x22        | Fragment Sequence  | ---XX | 7.4.2.9  | 23.3.1           | MAC  | MAC
 * 				            | Context            |       |          |                  |      |
 * 				            | Description (FSCD) |       |          |                  |      |
 * 				            | IE                 |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x23        | Simplified         | X---- | 7.4.2.10 | 6.2.3, [B3]      | MAC  | MAC
 * 				            | Superframe         |       |          |                  |      |
 * 				            | Specification IE   |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x24        | Simplified GTS     | X---- | 7.4.2.11 | 6.2.3, [B3]      | MAC  | MAC
 * 				            | Specification IE   |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x25        | LECIM Capabilities | X-XXX | 7.4.2.12 | 10.1.2.10        | UL   | UL
 * 				            | IE                 |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x26        | TRLE Descriptor IE | XXXXX | F.5.1.1  | F.4.2, F.4.3     | MAC  | MAC
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x27        | RCC Capabilities   | X-XX- | 7.4.2.13 | 6.2.9, [B3]      | UL   | UL
 * 				            | IE                 |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x28        | RCCN Descriptor IE | X---- | 7.4.2.14 | 6.2.9, [B3]      | UL   | UL
 * 				            |                    |       |          |                  | MAC  |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x29        | Global Time IE     | X---- | 7.4.2.15 | -                | UL   | UL
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x2A        | Assigned to external organization [B1]
 * 				------------+--------------------+-------+----------+------------------+------+------
 *				0x2B        | DA IE              | X---- | 7.4.2.16 | 6.7.9            | UL   | UL
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x2B–0x7D   | Reserved
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x7E        | Header Termination | XXXXX | 7.4.2.17 | 7.4.1            | MAC  | MAC
 * 				            | 1 IE               |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 *				0x7F        | Header Termination | XXXXX | 7.4.2.18 | 7.4.1            | MAC  | MAC
 * 				            | 2 IE               |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x80-0xFF   | Reserved
 * 				------------+--------------------+-------+----------+------------------+------+------
 *
 *
 * 				7.4.3 Payload IEs
 * 				Element ID  | Name               |
 * 				============+====================+=======+==========+==================+======+======
 * 				0x0         | Encapsulated       | X-XXX | 7.4.3.1  | 7.4.3.1          | UL   | UL
 * 				            | Service Data Unit  |       |          |                  |      |
 * 				            | (ESDU) IE          |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x1         | MLME               | XXXXX | 7.4.3.2  | 7.4.3.2          | UL   | UL
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x2         | Vendor Specific    | XXXXX | 7.4.4.30 | -                | UL   | UL
 * 				            | Nested IE          |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x3-0xE     | Reserved
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0xF         | Payload            | XXXXX | 7.4.3.3  | 7.4.1            | MAC  | MAC
 * 				            | Termination IE     |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 *
 *
 * 				7.4.3.2 MLME IE
 * 				The Nested IEs formats are defined in 7.4.4.1.
 * 				Variable Bytes          Nested IE
 * 				Variable Bytes          Nested IE
 * 				...
 * 				Variable Bytes          Nested IE
 *
 *
 * 				7.4.3.3 Payload Termination IE
 * 				The Payload Termination IE shall have a zero-length Content field.
 *
 *
 * 				7.4.4 Nested IE
 * 				Short Nested IEs
 * 				bits 0-7            Length
 * 				bits 8-14           Sub ID
 * 				bit  15             Type = 0
 * 				Variable Bytes      Content
 *
 * 				Long Nested IEs
 * 				bits 0-10           Length
 * 				bits 11-14          Sub ID
 * 				bit  15             Type = 1
 * 				Variable Bytes      Content
 *
 * 				Sub-ID allocation for Short Nested IEs
 * 				Element ID  | Name               |
 * 				============+====================+=======+==========+==================+======+======
 * 				0x00-0x0F   | Reserved for Long Nested IEs
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x10-0x19   | Reserved
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x1A        | TSCH               | X---- | 7.4.4.2  | 6.3.6            | MAC  | MAC
 * 				            | Synchronization IE |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x1B        | TSCH Slotframe and | X---- | 7.4.4.3  | 6.3.6            | UL   | UL
 * 				            | Link IE            |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x1C        | TSCH Timeslot IE   | X---- | 7.4.4.4  | 6.3.6, 6.5.4     | UL   | MAC
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x1D        | Hopping Timing IE  | X---- | 7.4.4.5  | 6.2.10           | MAC  | MAC
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x1E        | Enhanced Beacon    | ----X | 7.4.4.6  | 6.3.1.2, 6.3.4   | MAC  | UL
 * 				            | Filter IE          |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x1F        | MAC Metrics IE     | X-XX- | 7.4.4.7  | 8.4.2.6          | UL   | MAC
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x20        | All MAC Metrics IE | X-XX- | 7.4.4.8  | 8.4.2.6          | UL   | MAC
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x21        | Coexistence        | X---- | 7.4.4.9  | 6.2.3, 6.3.3.1,  | UL   | MAC
 * 				            | Specification IE   |       |          | 6.14             |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x22        | SUN Device         | --XX- | 7.4.4.10 | 10.1.2.8,        | UL   | MAC
 * 				            | Capabilities IE    |       |          | 20.2.1.2, 20.3.4 | MAC  |
 * 				            |                    |       |          | 20.3.4, 20.5,    |      |
 * 				            |                    |       |          | [B3]             |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x23        | SUN FSK Generic    | X-XXX | 7.4.4.11 | 10.1.2.8, 20.3   | UL   | MAC
 * 				            | PHY IE             |       |          |                  | MAC  |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x24        | Mode Switch        | X-XXX | 7.4.4.12 | 20.2.3, 20.5     | MAC  | UL
 * 				            | Parameter IE       |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x25        | PHY Parameter      | X--X- | 7.4.4.13 | 6.10             | MAC  | UL
 * 				            | Change IE          |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x26        | O-QPSK PHY Mode IE | --XX- | 7.4.4.14 | 6.10             | MAC  | UL
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x27        | PCA Allocation IE  | X---- | 7.4.4.15 | 6.2.5.4          | MAC  | UL
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x28        | LECIM DSS          | --XX- | 7.4.4.16 | 6.10             | MAC  | UL
 * 				            | Operating Mode IE  |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x29        | LECIM FSK          | --XX- | 7.4.4.17 | 6.10             | MAC  | UL
 * 				            | Operating Mode IE  |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x2A        | Reserved
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x2B        | TVWS PHY Operating | ---X- | 7.4.4.18 | 6.15             | MAC  | UL
 * 				            | Mode Description   |       |          |                  |      |
 * 				            | IE                 |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x2C        | TVWS Device        | X-XX- | 7.4.4.19 | 6.15             | UL   | MAC
 * 				            | Capabilities IE    |       |          |                  | MAC  |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x2D        | TVWS Device        | X---- | 7.4.4.20 | 6.15             | UL   | UL
 * 				            | Category IE        |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x2E        | TVWS Device        | X---- | 7.4.4.21 | 6.15             | UL   | UL
 * 				            | Identification IE  |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x2F        | TVWS Device        | X---- | 7.4.4.22 | 6.15             | UL   | UL
 * 				            | Location IE        |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x30        | TVWS Channel       | X---- | 7.4.4.23 | 6.15             | UL   | UL
 * 				            | Information Query  |       |          |                  |      |
 * 				            | IE                 |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x31        | TVWS Channel       | X---- | 7.4.4.24 | 6.15             | UL   | UL
 * 				            | Information Source |       |          |                  |      |
 * 				            | IE                 |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x32        | CTM IE             | X---- | 7.4.4.25 | 6.9.5            | UL   | UL
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x33        | Timestamp IE       | X---- | 7.4.4.26 | 6.9.5            | MAC  | MAC
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x34        | Timestamp          | X---- | 7.4.4.27 | 6.9.5, 6.7.4.2   | MAC  | MAC
 * 				            | Difference IE      |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x35        | TMCTP              | X---- | 7.4.4.28 | 6.13             | UL   | UL
 * 				            | Specification IE   |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x36        | RCC PHY Operating  | --XX- | 7.4.4.29 | 6.10             | MAC  | UL
 * 				            | Mode IE            |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x37-0x7F   | Reserved
 * 				------------+--------------------+-------+----------+------------------+------+------
 *
 *
 * 				Sub-ID allocation for Long Nested IEs
 * 				Element ID  | Name               |
 * 				============+====================+=======+==========+==================+======+======
 * 				0x0-0x7     | Reserved
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x8         | Vendor Specific    | XXXXX | 7.4.4.30 | --               | UL   | UL
 * 				            | Nested IE          |       |          |                  |      |
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0x9         | Channel Hopping IE | X---- | 7.4.4.31 | 6.2.10, 6.3.6    | MAC  | MAC
 * 				------------+--------------------+-------+----------+------------------+------+------
 * 				0xA-0xF     | Reserved
 * 				------------+--------------------+-------+----------+------------------+------+------
 *
 ***************************************************************************************************/
#ifndef IEEE_802_15_4_H
#define IEEE_802_15_4_H

#if __STDC_VERSION__ < 199901L
#error Compile with C99 or higher!
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* Includes -------------------------------------------------------------------------------------- */
#include <stdbool.h>
#include <stdint.h>
#include <zephyr.h>

#include "buffer.h"
#include "calc.h"


/* Public Constants ------------------------------------------------------------------------------ */
#define IEEE154_STD_PACKET_LENGTH       (127u)
#define IEEE154_IE_CTRL_LENGTH          (2u)
#define IEEE154_FCS_LENGTH              (2u)


/* Public Types ---------------------------------------------------------------------------------- */
#define __packed	__attribute__((__packed__))


#define IEEE154_EMPTY_LENGTH               (0)
#define IEEE154_FCTRL_LENGTH               (2)
#define IEEE154_SEQ_NUM_LENGTH             (1)
#define IEEE154_DEST_PAN_ID_LENGTH         (2)
#define IEEE154_DEST_SHORT_LENGTH          (2)
#define IEEE154_DEST_EXTENDED_LENGTH       (8)
#define IEEE154_SRC_PAN_ID_LENGTH          (2)
#define IEEE154_SRC_SHORT_LENGTH           (2)
#define IEEE154_SRC_EXTENDED_LENGTH        (8)

#define IEEE154_TYPE_SHIFT                 (0)
#define IEEE154_SECURITY_ENABLED_SHIFT     (3)
#define IEEE154_FRAME_PENDING_SHIFT        (4)
#define IEEE154_ACK_REQUEST_SHIFT          (5)
#define IEEE154_PAN_ID_COMPRESS_SHIFT      (6)
#define IEEE154_SEQ_NUM_SUPPRESS_SHIFT     (8)
#define IEEE154_IE_PRESENT_SHIFT           (9)
#define IEEE154_DAM_SHIFT                  (10)
#define IEEE154_FRAME_VERSION_SHIFT        (12)
#define IEEE154_SAM_SHIFT                  (14)

#define IEEE154_TYPE_MASK                  (0x7 << IEEE154_TYPE_SHIFT)
#define IEEE154_SECURITY_ENABLED_MASK      (0x1 << IEEE154_SECURITY_ENABLED_SHIFT)
#define IEEE154_FRAME_PENDING_MASK         (0x1 << IEEE154_FRAME_PENDING_SHIFT)
#define IEEE154_ACK_REQUEST_MASK           (0x1 << IEEE154_ACK_REQUEST_SHIFT)
#define IEEE154_PAN_ID_COMPRESS_MASK       (0x1 << IEEE154_PAN_ID_COMPRESS_SHIFT)
#define IEEE154_SEQ_NUM_SUPPRESS_MASK      (0x1 << IEEE154_SEQ_NUM_SUPPRESS_SHIFT)
#define IEEE154_IE_PRESENT_MASK            (0x1 << IEEE154_IE_PRESENT_SHIFT)
#define IEEE154_DAM_MASK                   (0x3 << IEEE154_DAM_SHIFT)
#define IEEE154_FRAME_VERSION_MASK         (0x3 << IEEE154_FRAME_VERSION_SHIFT)
#define IEEE154_SAM_MASK                   (0x3 << IEEE154_SAM_SHIFT)


/* Bits 0-2: Frame Type */
#define IEEE154_FRAME_TYPE_BEACON          (0x0 << IEEE154_TYPE_SHIFT)
#define IEEE154_FRAME_TYPE_DATA            (0x1 << IEEE154_TYPE_SHIFT)
#define IEEE154_FRAME_TYPE_ACK             (0x2 << IEEE154_TYPE_SHIFT)
#define IEEE154_FRAME_TYPE_MAC             (0x3 << IEEE154_TYPE_SHIFT)
#define IEEE154_FRAME_TYPE_MULTI           (0x5 << IEEE154_TYPE_SHIFT)
#define IEEE154_FRAME_TYPE_FRAG            (0x6 << IEEE154_TYPE_SHIFT)
#define IEEE154_FRAME_TYPE_EXT             (0x7 << IEEE154_TYPE_SHIFT)

/* Bit 3: Security Enabled */
#define IEEE154_SECURITY_ENABLED           (0x1 << IEEE154_SECURITY_ENABLED_SHIFT)

/* Bit 4: Frame Pending */
#define IEEE154_FRAME_PENDING              (0x1 << IEEE154_FRAME_PENDING_SHIFT)

/* Bit 5: Acknowledge Request */
#define IEEE154_ACK_REQUEST                (0x1 << IEEE154_ACK_REQUEST_SHIFT)

/* Bit 6: Pan ID Compression */
#define IEEE154_PAN_ID_NOT_COMPRESSED      (0x0 << IEEE154_PAN_ID_COMPRESS_SHIFT)
#define IEEE154_PAN_ID_COMPRESS            (0x1 << IEEE154_PAN_ID_COMPRESS_SHIFT)

/* Bit 8: Sequence Number Suppression */
#define IEEE154_SEQ_NUM_PRESENT            (0x0 << IEEE154_SEQ_NUM_SUPPRESS_SHIFT)
#define IEEE154_SEQ_NUM_SUPPRESS           (0x1 << IEEE154_SEQ_NUM_SUPPRESS_SHIFT)

/* Bit 9: Information Element (IE) Preset */
#define IEEE154_IE_PRESENT                 (0x1 << IEEE154_IE_PRESENT_SHIFT)

/* Bits 10-11: Destination Addressing Mode */
#define IEEE154_DAM_NOT_PRESENT            (0x0 << IEEE154_DAM_SHIFT)
#define IEEE154_DAM_SHORT                  (0x2 << IEEE154_DAM_SHIFT)
#define IEEE154_DAM_EXTENDED               (0x3 << IEEE154_DAM_SHIFT)

/* Bits 12-13: Frame Version */
#define IEEE154_FRAME_VERSION_2003         (0x0 << IEEE154_FRAME_VERSION_SHIFT)
#define IEEE154_FRAME_VERSION_2006         (0x1 << IEEE154_FRAME_VERSION_SHIFT)
#define IEEE154_FRAME_VERSION_2015         (0x2 << IEEE154_FRAME_VERSION_SHIFT)

/* Bits 14-15: Source Addressing Mode */
#define IEEE154_SAM_NOT_PRESENT            (0x0 << IEEE154_SAM_SHIFT)
#define IEEE154_SAM_SHORT                  (0x2 << IEEE154_SAM_SHIFT)
#define IEEE154_SAM_EXTENDED               (0x3 << IEEE154_SAM_SHIFT)


/* IEEE154_IE_TYPE_MASK helps classify if an IE is a Header IE or Payload IE, or if a Nested IE is
 * short or long. It exploits the fact that bit 15 is different between Header and Payload IEs, and
 * between Short and Long nested IEs. However, it cannot tell between IE and Nested IEs.
 *
 * 	Header IE:  0xxx xxxx xxxx xxxx
 *  Payload IE: 1xxx xxxx xxxx xxxx
 * 	Short NIE:  0xxx xxxx xxxx xxxx
 * 	Long NIE:   1xxx xxxx xxxx xxxx
 */
#define IEEE154_IE_TYPE_MASK    (1 << 15)
#define IEEE154_IE_INVALID      (0xFFFF)


/* Header IE:
 * bits 0-6				Length (terminate list by setting length to 0).
 * bits 7-14			Element ID
 * bit  15				Type = 0
 * Variable Bytes		Content */
#define IEEE154_HIE_LENGTH_SHIFT               (0)
#define IEEE154_HIE_ID_SHIFT                   (7)
#define IEEE154_HIE_TYPE_SHIFT                 (15)

#define IEEE154_HIE_LENGTH_MASK                (0x7F << IEEE154_HIE_LENGTH_SHIFT)
#define IEEE154_HIE_ID_MASK                    (0xFF << IEEE154_HIE_ID_SHIFT)
#define IEEE154_HIE_TYPE_MASK                  (0x1  << IEEE154_HIE_TYPE_SHIFT)

#define IEEE154_HIE_TYPE                       (0 << IEEE154_HIE_TYPE_SHIFT)
#define IEEE154_VENDOR_SPECIFIC_HEADER_IE      (0x00)
#define IEEE154_CSL_IE                         (0x1A)
#define IEEE154_RIT_IE                         (0x1B)
#define IEEE154_DSME_PAN_DESCRIPTOR_IE         (0x1C)
#define IEEE154_RENDEZVOUS_TIME_IE             (0x1D)
#define IEEE154_TIME_CORRECTION_IE             (0x1E)
#define IEEE154_EXT_DSME_PAN_DESCRIPTOR_IE     (0x21)
#define IEEE154_FRAG_SEQ_CONTEXT_DESCRIPTOR_IE (0x22)
#define IEEE154_SIMPLE_SUPERFRAME_SPEC_IE      (0x23)
#define IEEE154_SIMPLE_GTS_SPEC_IE             (0x24)
#define IEEE154_TRLE_DESCRIPTOR_IE             (0x26)
#define IEEE154_RCC_CAPABILITIES_IE            (0x27)
#define IEEE154_RCCN_DESCRIPTOR_IE             (0x28)
#define IEEE154_GLOBAL_TIME_IE                 (0x29)
#define IEEE154_DA_IE                          (0x2B)

/* HT1: Payload IE is present. */
#define IEEE154_HT1_IE                         (0x7E)

/* HT2: Payload IE is not present. */
#define IEEE154_HT2_IE                         (0x7F)


/* 	Payload IE:
 * 	bits 0-10			Length (terminate list by setting length to 0).
 * 	bits 11-14			Group ID
 * 	bit  15				Type = 1
 * 	Variable Bytes		Content */
#define IEEE154_PIE_LENGTH_SHIFT           (0)
#define IEEE154_PIE_ID_SHIFT               (11)
#define IEEE154_PIE_TYPE_SHIFT             (15)

#define IEEE154_PIE_LENGTH_MASK            (0x7FF << IEEE154_PIE_LENGTH_SHIFT)
#define IEEE154_PIE_ID_MASK                (0xF   << IEEE154_PIE_ID_SHIFT)
#define IEEE154_PIE_TYPE_MASK              (0x1   << IEEE154_PIE_TYPE_SHIFT)

#define IEEE154_PIE_TYPE                   (0x1 << IEEE154_PIE_TYPE_SHIFT)
#define IEEE154_ESDU_IE                    (0x0)
#define IEEE154_MLME_IE                    (0x1)
#define IEEE154_VENDOR_SPECIFIC_PAYLOAD_IE (0x2)

/* Payload Termination IE */
#define IEEE154_PT_IE                      (0xF)


/* 	Short Nested IEs
 * 	bits 0-7			Length
 * 	bits 8-14			Sub ID
 * 	bit  15				Type = 0
 * 	Variable Bytes		Content
 *
 * 	Long Nested IEs
 * 	bits 0-10			Length
 * 	bits 11-14			Sub ID
 * 	bit  15				Type = 1
 * 	Variable Bytes		Content */
#define IEEE154_SHORT_NIE_LENGTH_SHIFT       (0)
#define IEEE154_SHORT_NIE_ID_SHIFT           (8)
#define IEEE154_SHORT_NIE_TYPE_SHIFT         (15)
#define IEEE154_LONG_NIE_LENGTH_SHIFT        (0)
#define IEEE154_LONG_NIE_ID_SHIFT            (11)
#define IEEE154_LONG_NIE_TYPE_SHIFT          (15)
#define IEEE154_NIE_TYPE_SHIFT               (15)

#define IEEE154_SHORT_NIE_LENGTH_MASK        (0xFF  << IEEE154_SHORT_NIE_LENGTH_SHIFT)
#define IEEE154_SHORT_NIE_ID_MASK            (0x7F  << IEEE154_SHORT_NIE_ID_SHIFT)
#define IEEE154_SHORT_NIE_TYPE_MASK          (0x1   << IEEE154_SHORT_NIE_TYPE_SHIFT)
#define IEEE154_LONG_NIE_LENGTH_MASK         (0x7FF << IEEE154_LONG_NIE_LENGTH_SHIFT)
#define IEEE154_LONG_NIE_ID_MASK             (0xF   << IEEE154_LONG_NIE_ID_SHIFT)
#define IEEE154_LONG_NIE_TYPE_MASK           (0x1   << IEEE154_LONG_NIE_TYPE_SHIFT)
#define IEEE154_NESTED_IE_TYPE_MASK          (0x1   << IEEE154_NIE_TYPE_SHIFT)

#define IEEE154_SHORT_NIE_TYPE               (0x0 << IEEE154_NIE_TYPE_SHIFT)
#define IEEE154_LONG_NIE_TYPE                (0x1 << IEEE154_NIE_TYPE_SHIFT)

/* Long Nested IEs */
#define IEEE154_VENDOR_SPECIFIC_NESTED_IE    (0x8)
#define IEEE154_CHANNEL_HOPPING_IE           (0x9)

/* Short Nested IEs */
#define IEEE154_TSCH_SYNC_IE                 (0x1A)
#define IEEE154_TSCH_SLOTFRAME_AND_LINK_IE   (0x1B)
#define IEEE154_TSCH_TIMESLOT_IE             (0x1C)
#define IEEE154_HOPPING_TIMING_IE            (0x1D)
#define IEEE154_ENH_BEACON_FILTER_IE         (0x1E)
#define IEEE154_MAC_METRICS_IE               (0x1F)
#define IEEE154_ALL_MAC_METRICS_IE           (0x20)
#define IEEE154_COEXIST_SPEC_IE              (0x21)
#define IEEE154_SUN_DEVICE_CAPABILITIES_IE   (0x22)
#define IEEE154_SUN_FSK_GENERIC_PHY_IE       (0x23)
#define IEEE154_MODE_SWITCH_PARAM_IE         (0x24)
#define IEEE154_PHY_PARAM_CHANGE_IE          (0x25)
#define IEEE154_O_QPSK_PHY_MODE_IE           (0x26)
#define IEEE154_PCA_ALLOC_IE                 (0x27)
#define IEEE154_LECIM_DSS_OP_MODE_IE         (0x28)
#define IEEE154_LECIM_FSK_OP_MODE_IE         (0x29)
#define IEEE154_TVWS_PHY_OP_MODE_DESC_IE     (0x2B)
#define IEEE154_TVWS_DEVICE_CAPABILITIES_IE  (0x2C)
#define IEEE154_TVWS_DEVICE_CATEGORY_IE      (0x2D)
#define IEEE154_TVWS_DEVICE_ID_IE            (0x2E)
#define IEEE154_TVWS_DEVICE_LOCATION_IE      (0x2F)
#define IEEE154_TVWS_CHAN_INFO_QUERY_IE      (0x30)
#define IEEE154_TVWS_CHAN_INFO_SRC_IE        (0x31)
#define IEEE154_CTM_IE                       (0x32)
#define IEEE154_TIMESTAMP_IE                 (0x33)
#define IEEE154_TIMESTAMP_DIFF_IE            (0x34)
#define IEEE154_TMCTP_SPEC_IE                (0x35)
#define IEEE154_RCC_PHY_OP_MODE_IE           (0x36)


/* +------------------------------------------------------------+---------------+-----+
 * | MHR                                                        | MAC Payload   | MFR |
 * +-------------+----------------------------------+-----+-----+-----+---------+-----+
 * |             | Addressing Fields                |     | HIE | PIE |         |     |
 * +-------+-----+--------+-------+--------+--------+-----+-----+-----+---------+-----+
 * | Frame | Seq | Dest   | Dest  | Source | Source | Aux | IE        | Payload | FCS |
 * | Ctrl  | Num | PAN ID | Addr  | PAN ID | Addr   | Sec |           |         |     |
 * +=======+=====+========+=======+========+========+=====+=====+=====+=========+=====+
 * | 1/2   | 0/1 | 0/2    | 0/2/8 | 0/2    | 0/2/8  | var | var       | var     | 2/4 |
 * +=======+=====+========+=======+========+========+=====+=====+=====+=========+=====+
 *         ^     ^        ^       ^        ^        ^     ^           ^
 * Field   |     |        |       |        |        |     |           |
 * Indices 0     1        2       3        4        5     6           7 */
typedef struct {
	sys_snode_t node;
	Buffer  buffer;
	uint8_t fields[8];
} Ieee154_Frame;

typedef struct {
	uint8_t* dest;
	uint8_t* src;
	unsigned dest_len;
	unsigned src_len;
	uint16_t dest_pan_id;
	uint16_t src_pan_id;
} Ieee154_Addr;

typedef struct {
	Buffer   buffer;
	uint16_t ctrl;
	Ieee154_Frame* frame;
} Ieee154_IE;


/* Public Functions ------------------------------------------------------------------------------ */
void*    ieee154_frame_init        (Ieee154_Frame*, void*, unsigned, unsigned);
bool     ieee154_beacon_frame_init (Ieee154_Frame*, void*, unsigned);
bool     ieee154_data_frame_init   (Ieee154_Frame*, void*, unsigned);
bool     ieee154_ack_frame_init    (Ieee154_Frame*, void*, unsigned);
void*    ieee154_ptr_start         (const Ieee154_Frame*);
void*    ieee154_set_length        (Ieee154_Frame*, unsigned);
void     ieee154_parse             (Ieee154_Frame*);

unsigned ieee154_size              (const Ieee154_Frame*);
unsigned ieee154_length            (const Ieee154_Frame*);
unsigned ieee154_length_fctrl      (void);
unsigned ieee154_length_seqnum     (const Ieee154_Frame*);
unsigned ieee154_length_dest_pan_id(const Ieee154_Frame*);
unsigned ieee154_length_dest_addr  (const Ieee154_Frame*);
unsigned ieee154_length_src_pan_id (const Ieee154_Frame*);
unsigned ieee154_length_src_addr   (const Ieee154_Frame*);
unsigned ieee154_length_ash        (const Ieee154_Frame*);
unsigned ieee154_free              (const Ieee154_Frame*);

uint16_t ieee154_frame_type        (const Ieee154_Frame*);
uint16_t ieee154_frame_version     (const Ieee154_Frame*);
uint16_t ieee154_fctrl             (const Ieee154_Frame*);
uint8_t  ieee154_seqnum            (const Ieee154_Frame*);
uint16_t ieee154_dest_panid        (const Ieee154_Frame*);
void*    ieee154_dest_addr         (const Ieee154_Frame*);
uint16_t ieee154_src_panid         (const Ieee154_Frame*);
void*    ieee154_src_addr          (const Ieee154_Frame*);
unsigned ieee154_payload_start     (const Ieee154_Frame*);
uint8_t* ieee154_payload_ptr       (const Ieee154_Frame*);
Buffer*  ieee154_reset_buffer      (Ieee154_Frame*);

bool     ieee154_set_ack_request   (Ieee154_Frame*, bool);
bool     ieee154_set_seqnum        (Ieee154_Frame*, uint8_t);

bool ieee154_set_addr(
	Ieee154_Frame* frame,
	uint16_t dest_panid, const void* dest, unsigned dest_len,
	uint16_t src_panid,  const void* src,  unsigned src_len);


/* Ieee802.15.4 Frame IEs ------------------------------------------------------------------------ */
bool       ieee154_ie_is_hie        (const Ieee154_IE*);
bool       ieee154_ie_is_pie        (const Ieee154_IE*);
bool       ieee154_ie_is_nie_short  (const Ieee154_IE*);
bool       ieee154_ie_is_nie_long   (const Ieee154_IE*);
unsigned   ieee154_ie_max_length    (const Ieee154_IE*);

Ieee154_IE ieee154_ie_first         (Ieee154_Frame*);
Ieee154_IE ieee154_nie_first        (Ieee154_IE*);
bool       ieee154_ie_next          (Ieee154_IE*);
bool       ieee154_ie_is_valid      (const Ieee154_IE*);
bool       ieee154_ie_is_last       (const Ieee154_IE*);
uint16_t   ieee154_ie_type          (const Ieee154_IE*);
uint16_t   ieee154_ie_length        (const Ieee154_IE*);
uint16_t   ieee154_ie_length_content(const Ieee154_IE*);

bool       ieee154_hie_append       (Ieee154_IE*, uint16_t, const void*, unsigned);
bool       ieee154_pie_append       (Ieee154_IE*, uint16_t, const void*, unsigned);
bool       ieee154_nie_append       (Ieee154_IE*, uint16_t, const void*, unsigned);
void*      ieee154_ie_ptr_start     (const Ieee154_IE*);
void*      ieee154_ie_ptr_content   (const Ieee154_IE*);
Buffer*    ieee154_ie_reset_buffer  (Ieee154_IE*);
void       ieee154_ie_finalize      (Ieee154_IE*);





// ----------------------------------------------------------------------------------------------- //
// IEs                                                                                             //
// ----------------------------------------------------------------------------------------------- //
/* 0x1A - TSCH Synchronization IE ---------------------------------------------------------------- */
// typedef struct __packed {
typedef struct {
	uint8_t asn[5];
	uint8_t join_metric;
} TschSyncIE;


bool     tsch_sync_ie_append     (Ieee154_IE*, uint64_t asn, uint8_t join_metric);
uint64_t tsch_sync_ie_asn        (const Ieee154_IE*);
uint8_t  tsch_sync_ie_join_metric(const Ieee154_IE*);


/* 0x1B - TSCH Slotframe and Link IE ************************************************************//**
 * @brief		The TSCH Slotframe and Link IE is used in Enhanced Beacons to allow new devices to
 * 				obtain slotframes and links for a TSCH network. Layout:
 *
 * 					typedef struct {
 * 						uint16_t timeslot;			// 0
 * 						uint16_t offset;			// 2. Channel offset.
 * 						uint8_t  options;			// 4
 * 					} TschLinkInfo;
 *
 * 					typedef struct {
 * 						uint8_t  slotframe_handle;	// 0
 * 						uint16_t slotframe_size;	// 1
 * 						uint8_t  num_links;			// 3
 * 						TschLinkInfo links[];
 * 					} TschSFDescriptor;
 *
 * 					typedef struct {
 * 						uint8_t num_slotframes;		// 0. Number of slotframe descriptor fields.
 * 						TschSFDescriptor descriptors[];
 * 					} TschSFLinkIE;
 *
 * 				Writing Example:
 *
 * 					TschSFLinkIE sf_link_ie;
 *
 * 					tsch_sf_link_ie_append(&nie, &sf_link_ie);
 * 					tsch_sf_desc_append   (&sf_link_ie, 10, 5);
 * 					tsch_link_info_append (&sf_link_ie, 0, 0, TSCH_OPT_TX_LINK);
 * 					tsch_link_info_append (&sf_link_ie, 1, 0, TSCH_OPT_RX_LINK);
 * 					tsch_sf_desc_append   (&sf_link_ie, 11, 3);
 * 					tsch_link_info_append (&sf_link_ie, 2, 0, TSCH_OPT_SHARED_LINK);
 * 					ieee154_ie_finalize   (&nie);
 *
 * 				Reading Example:
 *
 * 					TschSFLinkIE sf_link_ie;
 *
 * 					tsch_sf_link_ie_read(&nie, &sf_link_ie);
 *
 * 					while(tsch_sf_desc_read(&sf_link_ie))
 * 					{
 * 						// tsch_sf_desc_sf_handle(&sf_link_ie);
 * 						// tsch_sf_desc_sf_size  (&sf_link_ie);
 * 						// tsch_sf_desc_num_links(&sf_link_ie);
 *
 * 						while(tsch_sf_link_info_read(&sf_link_ie))
 * 						{
 * 							// tsch_link_info_timeslot(&sf_link_ie);
 * 							// tsch_link_info_offset  (&sf_link_ie);
 * 							// tsch_link_info_options (&sf_link_ie);
 * 						}
 * 					}
 */
#define TSCH_OPT_TX_LINK     (0x1 << 0)
#define TSCH_OPT_RX_LINK     (0x1 << 1)
#define TSCH_OPT_SHARED_LINK (0x1 << 2)
#define TSCH_OPT_TIMEKEEPING (0x1 << 3)
#define TSCH_OPT_PRIORITY    (0x1 << 4)


typedef struct {
	Ieee154_IE* nie;
	uint8_t* start;		/* Pointer to the start of TSCH SF Link IE */
	uint8_t* sf_descr;	/* Pointer to the start of the current TSCH SF Descriptor */
	uint8_t* link_info;	/* Pointer to the start of the current TSCH Link Info */
	uint8_t  descr_remaining;
	uint8_t  links_remaining;
} TschSFLinkIE;


bool     tsch_sf_link_ie_append (Ieee154_IE*, TschSFLinkIE*);
bool     tsch_sf_link_ie_read   (Ieee154_IE*, TschSFLinkIE*);
uint8_t  tsch_sf_link_ie_num_sf (const TschSFLinkIE*);

bool     tsch_sf_desc_append    (TschSFLinkIE*, uint8_t, uint16_t);
bool     tsch_sf_desc_read      (TschSFLinkIE*);
uint8_t  tsch_sf_desc_sf_handle (const TschSFLinkIE*);
uint16_t tsch_sf_desc_sf_size   (const TschSFLinkIE*);
uint8_t  tsch_sf_desc_num_links (const TschSFLinkIE*);

bool     tsch_link_info_append  (TschSFLinkIE*, uint16_t, uint16_t, uint8_t);
bool     tsch_link_info_read    (TschSFLinkIE*);
uint16_t tsch_link_info_timeslot(const TschSFLinkIE*);
uint16_t tsch_link_info_offset  (const TschSFLinkIE*);
uint8_t  tsch_link_info_options (const TschSFLinkIE*);



/* 0x1C - TSCH Timeslot IE ----------------------------------------------------------------------- */
/* The TSCH Timeslot IE may be sent with only the Timeslot ID field to reduce the size of the beacon.
 * Otherwise, all the fields are included. The length of the Max TX field and Timeslot Length field
 * shall be the same and can be determined from the length of the IE. */
// typedef struct __packed {
typedef struct {
	uint8_t  timeslot_id;
	uint16_t cca_offset;
	uint16_t cca;
	uint16_t tx_offset;
	uint16_t rx_offset;
	uint16_t rx_ack_delay;
	uint16_t tx_ack_delay;
	uint16_t rx_wait;
	uint16_t ack_wait;
	uint16_t rx_tx;
	uint16_t max_ack;
	uint16_t max_tx;			/* Could be 2 or 3 bytes */
	uint16_t timeslot_length;	/* Could be 2 or 3 bytes */
} TschTimeslotIE;


/* 0x9 - Channel Hopping IE ---------------------------------------------------------------------- */
/* The Channel Hopping IE may be sent with only the Hopping Sequence ID field to reduce the  size of
 * the beacon. Otherwise, all the fields are included.
 *
 * Extended Bitmap field is only valid for channel pages 9 and 10. The Extended Bitmap field contains
 * macExtendedBitmap which is a bitmap in which a bit is set to one if a channel is to be used and is
 * set to zero otherwise. For channel pages 9 and 10, the Extended Bitmap field is
 * macNumberOfChannels bits long, with additional bits added to make it an integer number of octets.
 * For all other values of the Channel Page field, it is zero length. */
//typedef struct __packed {
//	uint8_t  hopping_sequence_id;
//	uint8_t  channel_page;
//	uint16_t number_of_channels;
//	uint32_t phy_configuration;
//	uint8_t  extended_bitmap[0];
//	uint16_t hopping_sequence_length;
//	uint16_t hopping_sequence[0];	/* Size of hopping sequence length long */
//	uint16_t current_hop;
//} ChannelHoppingIE;


#ifdef __cplusplus
}
#endif

#endif // IEEE_802_15_4_H
/******************************************* END OF FILE *******************************************/
