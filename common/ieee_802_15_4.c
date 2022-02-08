/************************************************************************************************//**
 * @file		ieee_802_15_4.c
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
 ***************************************************************************************************/
#include "byteorder.h"
#include "ieee_802_15_4.h"


/* Private Macros -------------------------------------------------------------------------------- */
#define IEEE154_SEQNUM_FIELD     (0)
#define IEEE154_DEST_PANID_FIELD (1)
#define IEEE154_DEST_ADDR_FIELD  (2)
#define IEEE154_SRC_PANID_FIELD  (3)
#define IEEE154_SRC_ADDR_FIELD   (4)
#define IEEE154_SEC_FIELD        (5)
#define IEEE154_IE_FIELD         (6)
#define IEEE154_PAYLOAD_FIELD    (7)


/* Private Functions ----------------------------------------------------------------------------- */
static        void        ieee154_set_fctrl        (Ieee154_Frame*, uint16_t);
static        void*       ieee154_field            (const Ieee154_Frame*, unsigned);
// static        void        ieee154_ie_set_length    (Ieee154_IE*, uint16_t);
static        void        ieee154_ie_set_ctrl      (Ieee154_IE*, uint16_t);
static inline Ieee154_IE* ieee154_ie_parent        (const Ieee154_IE*);
static        uint16_t    ieee154_ie_read_type     (const Buffer*, const uint8_t*);
static        uint16_t    ieee154_ie_read_length   (const Ieee154_IE*, uint16_t);
static        bool        fctrl_pan_id_compress    (uint16_t);
static        unsigned    fctrl_length_seqnum      (uint16_t);
static        unsigned    fctrl_length_dest_pan_id (uint16_t);
static        unsigned    fctrl_length_dest_addr   (uint16_t);
static        unsigned    fctrl_length_src_pan_id  (uint16_t);
static        unsigned    fctrl_length_src_addr    (uint16_t);
// static        unsigned    fctrl_length_ash         (uint16_t);
static        bool        fctrl_dest_pan_id_present(uint16_t);
static        bool        fctrl_src_pan_id_present (uint16_t);
static        bool        fctrl_addr_mode_valid    (uint16_t, uint16_t, bool, bool);
static        bool        fctrl_set_addr_mode      (uint16_t*, uint16_t, uint16_t, bool, bool);


/* ieee154_frame_init ***************************************************************************//**
 * @brief		Initializes an empty IEEE 802.15.4 Frame. */
void* ieee154_frame_init(Ieee154_Frame* frame, void* data, unsigned count, unsigned size)
{
	/* Frame Control Field:
	 * bit 0-2				Frame Type
	 * bit 3				Security Enabled
	 * bit 4				Frame Pending
	 * bit 5				Acknowledge Request
	 * bit 6				PAN ID Compression
	 * bit 7				Reserved
	 * bit 8				Sequence Number Suppression
	 * bit 9				Information Element (IE) Present Field
	 * bit 10-11			Destination Addressing Mode
	 * bit 12-13			Frame Version
	 * bit 14-15			Source Addressing Mode */
	count = (count < 2) ? IEEE154_IE_CTRL_LENGTH : count;

	void* ptr = buffer_init(&frame->buffer, data, count, size);

	frame->fields[IEEE154_SEQNUM_FIELD]     = IEEE154_IE_CTRL_LENGTH;
	frame->fields[IEEE154_DEST_PANID_FIELD] = IEEE154_IE_CTRL_LENGTH;
	frame->fields[IEEE154_DEST_ADDR_FIELD]  = IEEE154_IE_CTRL_LENGTH;
	frame->fields[IEEE154_SRC_PANID_FIELD]  = IEEE154_IE_CTRL_LENGTH;
	frame->fields[IEEE154_SRC_ADDR_FIELD]   = IEEE154_IE_CTRL_LENGTH;
	frame->fields[IEEE154_SEC_FIELD]        = IEEE154_IE_CTRL_LENGTH;
	frame->fields[IEEE154_IE_FIELD]         = IEEE154_IE_CTRL_LENGTH;
	frame->fields[IEEE154_PAYLOAD_FIELD]    = IEEE154_IE_CTRL_LENGTH;

	return ptr;
}



/* ieee154_beacon_frame_init ********************************************************************//**
 * @brief		Initializes an IEEE 802.15.4 Beacon Frame. */
bool ieee154_beacon_frame_init(Ieee154_Frame* frame, void* data, unsigned size)
{
	uint16_t ctrl =
		IEEE154_FRAME_VERSION_2015 |
		IEEE154_FRAME_TYPE_BEACON  |
		IEEE154_SEQ_NUM_SUPPRESS;

	ieee154_frame_init(frame, data, 0, size);
	ieee154_set_fctrl(frame, ctrl);
	return true;
}


/* ieee154_data_frame_init **********************************************************************//**
 * @brief		Initializes an IEEE 802.15.4 data frame. */
bool ieee154_data_frame_init(Ieee154_Frame* frame, void* data, unsigned size)
{
	uint16_t ctrl =
		IEEE154_FRAME_VERSION_2015 |
		IEEE154_FRAME_TYPE_DATA    |
		IEEE154_SEQ_NUM_SUPPRESS;

	ieee154_frame_init(frame, data, 0, size);
	ieee154_set_fctrl(frame, ctrl);
	return true;
}


/* ieee154_ack_frame_init ***********************************************************************//**
 * @brief		Initializes an IEEE 802.15.4 enhanced ack frame. */
bool ieee154_ack_frame_init(Ieee154_Frame* frame, void* data, unsigned size)
{
	uint16_t ctrl =
		IEEE154_FRAME_VERSION_2015 |
		IEEE154_FRAME_TYPE_ACK     |
		IEEE154_SEQ_NUM_SUPPRESS;

	ieee154_frame_init(frame, data, 0, size);
	ieee154_set_fctrl(frame, ctrl);
	return true;
}


/* ieee154_ptr_start ****************************************************************************//**
 * @brief		Returns a pointer to the start of the IEEE 802.15.4 frame buffer. */
void* ieee154_ptr_start(const Ieee154_Frame* frame)
{
	return buffer_start(&frame->buffer);
}


/* ieee154_set_length ***************************************************************************//**
 * @brief		Initializes the length of the frame and returns a pointer to the start of the
 * 				frame's buffer. Call ieee154_parse after copying data into the reserved bytes. */
void* ieee154_set_length(Ieee154_Frame* frame, unsigned length)
{
	return buffer_set_length(&frame->buffer, length);
}


/* ieee154_parse ********************************************************************************//**
 * @brief		*/
void ieee154_parse(Ieee154_Frame* frame)
{
	/* TODO: read through IEs once to set payload index? */

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
	 * Indices 0     1        2       3        4        5     6           7
	 */
	uint16_t fctrl = ieee154_fctrl(frame);
	uint8_t  total = 0;
	frame->fields[IEEE154_SEQNUM_FIELD]     = (total += ieee154_length_fctrl    ());
	frame->fields[IEEE154_DEST_PANID_FIELD] = (total += fctrl_length_seqnum     (fctrl));
	frame->fields[IEEE154_DEST_ADDR_FIELD]  = (total += fctrl_length_dest_pan_id(fctrl));
	frame->fields[IEEE154_SRC_PANID_FIELD]  = (total += fctrl_length_dest_addr  (fctrl));
	frame->fields[IEEE154_SRC_ADDR_FIELD]   = (total += fctrl_length_src_pan_id (fctrl));
	frame->fields[IEEE154_SEC_FIELD]        = (total += fctrl_length_src_addr   (fctrl));
	frame->fields[IEEE154_IE_FIELD]         = (total += ieee154_length_ash      (frame));
	frame->fields[IEEE154_PAYLOAD_FIELD]    = total;
}





// ----------------------------------------------------------------------------------------------- //
// IEEE 802.15.4 Field Length                                                                      //
// ----------------------------------------------------------------------------------------------- //
/* ieee154_size *********************************************************************************//**
 * @brief		Returns the total capacity of the IEEE 802.15.4 frame's buffer in bytes. */
unsigned ieee154_size(const Ieee154_Frame* frame)
{
	return buffer_size(&frame->buffer);
}


/* ieee154_length *******************************************************************************//**
 * @brief		Returns the length in bytes of the frame. */
unsigned ieee154_length(const Ieee154_Frame* frame)
{
	return buffer_length(&frame->buffer);
}


/* ieee154_length_fctrl *************************************************************************//**
 * @brief		Returns the length in bytes of the frame control field. */
unsigned ieee154_length_fctrl(void)
{
	return 2;
}


/* ieee154_length_seqnum ************************************************************************//**
 * @brief		Returns the length in bytes of the seqnum field. */
unsigned ieee154_length_seqnum(const Ieee154_Frame* frame)
{
	return fctrl_length_seqnum(ieee154_fctrl(frame));
}


/* ieee154_length_dest_pan_id *******************************************************************//**
 * @brief		Returns the length in bytes of the destination PAN ID field. */
unsigned ieee154_length_dest_pan_id(const Ieee154_Frame* frame)
{
	return fctrl_length_dest_pan_id(ieee154_fctrl(frame));
}


/* ieee154_length_dest_addr *********************************************************************//**
 * @brief		Returns the length in bytes of the destination address field. */
unsigned ieee154_length_dest_addr(const Ieee154_Frame* frame)
{
	return fctrl_length_dest_addr(ieee154_fctrl(frame));
}


/* ieee154_length_src_pan_id ********************************************************************//**
 * @brief		Returns the length in bytes of the source PAN ID field. */
unsigned ieee154_length_src_pan_id(const Ieee154_Frame* frame)
{
	return fctrl_length_src_pan_id(ieee154_fctrl(frame));
}


/* ieee154_length_src_addr **********************************************************************//**
 * @brief		Returns the length in bytes of the source address field. */
unsigned ieee154_length_src_addr(const Ieee154_Frame* frame)
{
	return fctrl_length_src_addr(ieee154_fctrl(frame));
}


/* ieee154_length_ash *****************************************************************************//**
 * @brief		Returns the length in bytes of the source address field. */
unsigned ieee154_length_ash(const Ieee154_Frame* frame)
{
	/* @TODO */
	// return fctrl_length_ash(ieee154_fctrl(frame));
	(void)(frame);
	return 0;
}


/* ieee154_free *********************************************************************************//**
 * @brief		Returns the number of unused bytes in the frame. */
unsigned ieee154_free(const Ieee154_Frame* frame)
{
	return buffer_size(&frame->buffer) - ieee154_length(frame);
}





// ----------------------------------------------------------------------------------------------- //
// IEEE 802.15.4 Field Read                                                                        //
// ----------------------------------------------------------------------------------------------- //
/* ieee154_frame_type ***************************************************************************//**
 * @brief		Returns the frame's type. */
uint16_t ieee154_frame_type(const Ieee154_Frame* frame)
{
	return ieee154_fctrl(frame) & IEEE154_TYPE_MASK;
}


/* ieee154_frame_version ************************************************************************//**
 * @brief		Returns the frame version. */
uint16_t ieee154_frame_version(const Ieee154_Frame* frame)
{
	return ieee154_fctrl(frame) & IEEE154_FRAME_VERSION_MASK;
}


/* ieee154_fctrl ********************************************************************************//**
 * @brief		Returns the frame control. */
uint16_t ieee154_fctrl(const Ieee154_Frame* frame)
{
	return le_get_u16(buffer_start(&frame->buffer));
}


/* ieee154_seqnum *******************************************************************************//**
 * @brief		Returns the frame's sequence number. Returns 0 if no sequence number is present. */
uint8_t ieee154_seqnum(const Ieee154_Frame* frame)
{
	return be_get_u8(ieee154_field(frame, IEEE154_SEQNUM_FIELD));
}


/* ieee154_dest_panid ***************************************************************************//**
 * @brief		Returns the frame's destination pan id. Returns 0 if no destination pan id is
 * 				present. */
uint16_t ieee154_dest_panid(const Ieee154_Frame* frame)
{
	return be_get_u16(ieee154_field(frame, IEEE154_DEST_PANID_FIELD));
}


/* ieee154_dest_addr ****************************************************************************//**
 * @brief		Returns a pointer to the destination address. Returns null if the destination
 * 				address is not present. */
void* ieee154_dest_addr(const Ieee154_Frame* frame)
{
	return ieee154_field(frame, IEEE154_DEST_ADDR_FIELD);
}


/* ieee154_src_panid ****************************************************************************//**
 * @brief		Returns the frame's source pan id. Returns 0 if no source pan id is present. */
uint16_t ieee154_src_panid(const Ieee154_Frame* frame)
{
	return be_get_u16(ieee154_field(frame, IEEE154_SRC_PANID_FIELD));
}


/* ieee154_src_addr *****************************************************************************//**
 * @brief		Returns a pointer to the source address. Returns null if the source address is not
 * 				present. */
void* ieee154_src_addr(const Ieee154_Frame* frame)
{
	return ieee154_field(frame, IEEE154_SRC_ADDR_FIELD);
}



/* ieee154_payload_start ************************************************************************//**
 * @brief		Returns the index of the start of the IEEE 802.15.4 frame's payload.
 * @warning		If called on a received frame, traverse all IEs by calling ieee154_ie_first and
 * 				ieee154_ie_next as ieee154_ie_next sets the field index once it reaches the end of
 * 				the IEs. */
unsigned ieee154_payload_start(const Ieee154_Frame* frame)
{
	return frame->fields[IEEE154_PAYLOAD_FIELD];
}


/* ieee154_payload_ptr **************************************************************************//**
 * @brief		Returns a pointer to the payload of the IEEE 802.15.4 frame.
 * @warning		If called on a received frame, traverse all IEs by calling ieee154_ie_first and
 * 				ieee154_ie_next as ieee154_ie_next sets the field index once it reaches the end of
 * 				the IEs. */
uint8_t* ieee154_payload_ptr(const Ieee154_Frame* frame)
{
	return ieee154_field(frame, IEEE154_PAYLOAD_FIELD);
}


/* ieee154_reset_buffer *************************************************************************//**
 * @brief		Returns the frame's current buffer. The buffer's read and write pointers are reset so
 * 				that bytes can be read from the start of the payload and written to the end of the
 * 				payload. */
Buffer* ieee154_reset_buffer(Ieee154_Frame* frame)
{
	buffer_read_seek(&frame->buffer, frame->fields[IEEE154_PAYLOAD_FIELD]);
	return &frame->buffer;
}





// ----------------------------------------------------------------------------------------------- //
// IEEE 802.15.4 Field Write                                                                       //
// ----------------------------------------------------------------------------------------------- //
/* ieee154_set_fctrl ****************************************************************************//**
 * @brief		Overwrites the frame control field. */
static void ieee154_set_fctrl(Ieee154_Frame* frame, uint16_t fctrl)
{
	fctrl = le_u16(fctrl);

	buffer_replace_at(&frame->buffer, &fctrl, buffer_start(&frame->buffer), IEEE154_FCTRL_LENGTH);
}


/* ieee154_set_ack_request **********************************************************************//**
 * @brief		Sets the ack request bit. */
bool ieee154_set_ack_request(Ieee154_Frame* frame, bool req)
{
	uint16_t fctrl = ieee154_fctrl(frame);
	fctrl &= ~(IEEE154_ACK_REQUEST_MASK);
	fctrl |=  (req << IEEE154_ACK_REQUEST_SHIFT);
	ieee154_set_fctrl(frame, fctrl);
	return true;
}


/* ieee154_set_seqnum ***************************************************************************//**
 * @brief		Writes the sequence number to the frame. */
bool ieee154_set_seqnum(Ieee154_Frame* frame, uint8_t seqnum)
{
	uint16_t fctrl = ieee154_fctrl(frame);

	if((fctrl & IEEE154_SEQ_NUM_SUPPRESS_MASK) == IEEE154_SEQ_NUM_SUPPRESS)
	{
		uint8_t* ptr = buffer_offset(&frame->buffer, frame->fields[0]);
		buffer_write_at(&frame->buffer, &seqnum, ptr, sizeof(seqnum));
		ieee154_set_fctrl(frame, fctrl & ~(IEEE154_SEQ_NUM_SUPPRESS));

		frame->fields[IEEE154_DEST_PANID_FIELD] += IEEE154_SEQ_NUM_LENGTH;
		frame->fields[IEEE154_DEST_ADDR_FIELD]  += IEEE154_SEQ_NUM_LENGTH;
		frame->fields[IEEE154_SRC_PANID_FIELD]  += IEEE154_SEQ_NUM_LENGTH;
		frame->fields[IEEE154_SRC_ADDR_FIELD]   += IEEE154_SEQ_NUM_LENGTH;
		frame->fields[IEEE154_SEC_FIELD]        += IEEE154_SEQ_NUM_LENGTH;
		frame->fields[IEEE154_IE_FIELD]         += IEEE154_SEQ_NUM_LENGTH;
		frame->fields[IEEE154_PAYLOAD_FIELD]    += IEEE154_SEQ_NUM_LENGTH;

		return true;
	}
	else
	{
		return false;
	}
}


/* ieee154_set_addr *****************************************************************************//**
 * @brief		Writes the dest PAN ID, dest address, src PAN ID, and src address to the frame. */
bool ieee154_set_addr(
	Ieee154_Frame* frame,
	uint16_t dest_panid, const void* dest, unsigned dest_len,
	uint16_t src_panid,  const void* src,  unsigned src_len)
{
	uint16_t fctrl = ieee154_fctrl(frame);
	uint16_t dest_panid_len = 2 * (dest_panid != 0);
	uint16_t src_panid_len  = 2 * (src_panid  != 0);
	uint16_t dest_addr_mode;
	uint16_t src_addr_mode;

	/* Check that addressing fields haven't been set already */
	if(fctrl_length_dest_pan_id(fctrl) != 0 || fctrl_length_dest_addr(fctrl) != 0 ||
	   fctrl_length_src_pan_id(fctrl)  != 0 || fctrl_length_src_addr(fctrl) != 0)
	{
		return false;
	}

	/* Get the dest address fctrl bits */
	switch(dest_len) {
	case 0: dest_addr_mode = IEEE154_DAM_NOT_PRESENT; break;
	case 2: dest_addr_mode = IEEE154_DAM_SHORT;       break;
	case 8: dest_addr_mode = IEEE154_DAM_EXTENDED;    break;
	default: return false;
	}

	/* Get the src address fctrl bits */
	switch(src_len) {
	case 0: src_addr_mode = IEEE154_SAM_NOT_PRESENT; break;
	case 2: src_addr_mode = IEEE154_SAM_SHORT;       break;
	case 8: src_addr_mode = IEEE154_SAM_EXTENDED;    break;
	default: return false;
	}

	/* Check that there is enough free space in the frame */
	if(ieee154_free(frame) < dest_panid_len + dest_len + src_panid_len + src_len)
	{
		return false;
	}

	/* Set the addressing mode */
	if(!fctrl_set_addr_mode(&fctrl, dest_addr_mode, src_addr_mode, dest_panid_len, src_panid_len))
	{
		return false;
	}

	ieee154_set_fctrl(frame, fctrl);

	dest_panid = be_u16(dest_panid);
	src_panid  = be_u16(src_panid);

	/* Update field indices */
	uint8_t total = 0;
	frame->fields[IEEE154_DEST_ADDR_FIELD] += (total += dest_panid_len);
	frame->fields[IEEE154_SRC_PANID_FIELD] += (total += dest_len);
	frame->fields[IEEE154_SRC_ADDR_FIELD]  += (total += src_panid_len);
	frame->fields[IEEE154_SEC_FIELD]       += (total += src_len);
	frame->fields[IEEE154_IE_FIELD]        += total;
	frame->fields[IEEE154_PAYLOAD_FIELD]   += total;

	buffer_write_offset(&frame->buffer, &dest_panid,
		frame->fields[IEEE154_DEST_PANID_FIELD], dest_panid_len);
	buffer_write_offset(&frame->buffer, dest,
		frame->fields[IEEE154_DEST_ADDR_FIELD],  dest_len);
	buffer_write_offset(&frame->buffer, &src_panid,
		frame->fields[IEEE154_SRC_PANID_FIELD],  src_panid_len);
	buffer_write_offset(&frame->buffer, src,
		frame->fields[IEEE154_SRC_ADDR_FIELD],   src_len);

	return true;
}





// ----------------------------------------------------------------------------------------------- //
// IEEE 802.15.4 IE                                                                                //
// ----------------------------------------------------------------------------------------------- //
/* ieee154_ie_parent ****************************************************************************//**
 * @brief		*/
static inline Ieee154_IE* ieee154_ie_parent(const Ieee154_IE* ie)
{
	if(buffer_parent(&ie->buffer) == &ie->frame->buffer)
	{
		return 0;
	}
	else
	{
		return (Ieee154_IE*)buffer_parent(&ie->buffer);
	}
}


/* ieee154_ie_is_hie ****************************************************************************//**
 * @brief		Returns true if the IE is a Header IE. */
bool ieee154_ie_is_hie(const Ieee154_IE* ie)
{
	return !ieee154_ie_parent(ie) && (ie->ctrl & IEEE154_IE_TYPE_MASK) == IEEE154_HIE_TYPE;
}


/* ieee154_ie_is_pie ****************************************************************************//**
 * @brief		Returns true if the IE is a Payload IE. */
bool ieee154_ie_is_pie(const Ieee154_IE* ie)
{
	return !ieee154_ie_parent(ie) && (ie->ctrl & IEEE154_IE_TYPE_MASK) == IEEE154_PIE_TYPE;
}


/* ieee154_ie_is_nie_short **********************************************************************//**
 * @brief		Returns true if the IE is a short Nested IE. */
bool ieee154_ie_is_nie_short(const Ieee154_IE* ie)
{
	return ieee154_ie_parent(ie) && (ie->ctrl & IEEE154_IE_TYPE_MASK) == IEEE154_SHORT_NIE_TYPE;
}


/* ieee154_ie_is_nie_long ***********************************************************************//**
 * @brief		Returns true if the IE is a long Nested IE. */
bool ieee154_ie_is_nie_long(const Ieee154_IE* ie)
{
	return ieee154_ie_parent(ie) && (ie->ctrl & IEEE154_IE_TYPE_MASK) == IEEE154_LONG_NIE_TYPE;
}


/* ieee154_ie_max_length ************************************************************************//**
 * @brief		Returns the maximum content length of the IE. */
unsigned ieee154_ie_max_length(const Ieee154_IE* ie)
{
	if(!ieee154_ie_parent(ie))
	{
		if(ieee154_ie_is_hie(ie))
		{
			return (IEEE154_HIE_LENGTH_MASK) >> IEEE154_HIE_LENGTH_SHIFT;
		}
		else if(ieee154_ie_is_pie(ie))
		{
			return (IEEE154_PIE_LENGTH_MASK) >> IEEE154_PIE_LENGTH_SHIFT;
		}
	}
	else
	{
		if(ieee154_ie_is_nie_short(ie))
		{
			return (IEEE154_SHORT_NIE_LENGTH_MASK) >> IEEE154_SHORT_NIE_LENGTH_SHIFT;
		}
		else if(ieee154_ie_is_nie_long(ie))
		{
			return (IEEE154_LONG_NIE_LENGTH_MASK) >> IEEE154_LONG_NIE_LENGTH_SHIFT;
		}
	}

	return 0;
}


/* ieee154_ie_first *****************************************************************************//**
 * @brief		Returns the first IE in the frame. */
Ieee154_IE ieee154_ie_first(Ieee154_Frame* frame)
{
	Ieee154_IE ie;
	uint16_t fctrl = ieee154_fctrl(frame);
	uint8_t* fidx  = buffer_offset(&frame->buffer, frame->fields[IEEE154_IE_FIELD]);

	if((fctrl & IEEE154_IE_PRESENT) != IEEE154_IE_PRESENT || !fidx)
	{
		ie.frame = frame;
		ie.ctrl  = IEEE154_IE_INVALID;
		buffer_slice    (&ie.buffer, &frame->buffer, fidx, 0);
		buffer_read_seek(&ie.buffer, IEEE154_IE_CTRL_LENGTH);
	}
	else
	{
		ie.frame = frame;
		ie.ctrl  = ieee154_ie_read_type(&frame->buffer, fidx);
		buffer_slice(&ie.buffer, &frame->buffer, fidx, ieee154_ie_read_length(0, ie.ctrl));
		buffer_read_seek(&ie.buffer, IEEE154_IE_CTRL_LENGTH);
	}

	return ie;
}


/* ieee154_nie_first ****************************************************************************//**
 * @brief		Returns the first nested IE in the parent ie. */
Ieee154_IE ieee154_nie_first(Ieee154_IE* parent)
{
	Ieee154_IE ie;

	uint8_t* ptr = buffer_offset(&parent->buffer, IEEE154_IE_CTRL_LENGTH);

	if(!ieee154_ie_is_valid(parent) || !ptr)
	{
		ie.frame = parent->frame;
		ie.ctrl  = IEEE154_IE_INVALID;
		buffer_slice    (&ie.buffer, &parent->buffer, ptr, 0);
		buffer_read_seek(&ie.buffer, IEEE154_IE_CTRL_LENGTH);
	}
	else
	{
		ie.frame = parent->frame;
		ie.ctrl  = ieee154_ie_read_type(&parent->buffer, ptr);
		buffer_slice    (&ie.buffer, &parent->buffer, ptr, ieee154_ie_read_length(parent, ie.ctrl));
		buffer_read_seek(&ie.buffer, IEEE154_IE_CTRL_LENGTH);
	}

	return ie;
}


/* ieee154_ie_next ******************************************************************************//**
 * @brief		Updates the IE to the next IE in a frame if it exists. */
bool ieee154_ie_next(Ieee154_IE* ie)
{
	if(!ieee154_ie_is_valid(ie))
	{
		return false;
	}

	uint8_t* ptr = buffer_write(&ie->buffer);

	if(ieee154_ie_is_last(ie))
	{
		buffer_slice    (&ie->buffer, buffer_parent(&ie->buffer), ptr, 0);
		buffer_read_seek(&ie->buffer, IEEE154_IE_CTRL_LENGTH);

		/* Don't update the payload field index if this is a nested IE. Nested IEs have their parent
		 * field set while header and payload IEs have a null parent field. */
		if(!ieee154_ie_parent(ie))
		{
			/* Update payload index */
			ie->frame->fields[IEEE154_PAYLOAD_FIELD] =
				buffer_offsetof(buffer_parent(&ie->buffer), buffer_write(&ie->buffer));
		}
	}
	else
	{
		uint16_t ctrl = ieee154_ie_read_type(buffer_parent(&ie->buffer), ptr);
		uint16_t len  = ieee154_ie_read_length(ieee154_ie_parent(ie), ctrl);

		ie->ctrl = ctrl;

		buffer_slice    (&ie->buffer, buffer_parent(&ie->buffer), ptr, len);
		buffer_read_seek(&ie->buffer, IEEE154_IE_CTRL_LENGTH);
	}

	return true;
}


/* ieee154_ie_is_valid *************************************************************************//**
 * @brief		Returns true if the IE is valid. */
bool ieee154_ie_is_valid(const Ieee154_IE* ie)
{
	if(!ie->frame)
	{
		return false;
	}
	else if(buffer_length(&ie->buffer) == 0)
	{
		return false;
	}
	else
	{
		return true;
	}
}


/* ieee154_ie_is_last ***************************************************************************//**
 * @brief		Returns true if the IE is the last IE in it's range. */
bool ieee154_ie_is_last(const Ieee154_IE* ie)
{
	Buffer* parent = buffer_parent(&ie->buffer);

	/* Check if no more room in frame for another IE */
	if(buffer_offsetof(parent, buffer_write(parent)) -
	   buffer_offsetof(parent, buffer_write(&ie->buffer)) < IEEE154_IE_CTRL_LENGTH)
	{
		return true;
	}
	/* Check if HT2 which signals end of Header IE, immediately followed by payload */
	else if(ieee154_ie_is_hie(ie) && ieee154_ie_type(ie) == IEEE154_HT2_IE)
	{
		return true;
	}
	/* Check if PT, which signals end of Payload IE, immediately followed by payload */
	else if(ieee154_ie_is_pie(ie) && ieee154_ie_type(ie) == IEEE154_PT_IE)
	{
		return true;
	}
	/* This is not the last IE */
	else
	{
		return false;
	}
}


/* ieee154_ie_type ******************************************************************************//**
 * @brief		Returns the type (TODO: rename to ID? Called type to match the naming convention of
 * 				IPv6) of the IE. */
uint16_t ieee154_ie_type(const Ieee154_IE* ie)
{
	if(!ieee154_ie_parent(ie))
	{
		if((ie->ctrl & IEEE154_IE_TYPE_MASK) == IEEE154_HIE_TYPE)
		{
			return (ie->ctrl & IEEE154_HIE_ID_MASK) >> IEEE154_HIE_ID_SHIFT;
		}
		else if((ie->ctrl & IEEE154_IE_TYPE_MASK) == IEEE154_PIE_TYPE)
		{
			return (ie->ctrl & IEEE154_PIE_ID_MASK) >> IEEE154_PIE_ID_SHIFT;
		}
	}
	else
	{
		if((ie->ctrl & IEEE154_IE_TYPE_MASK) == IEEE154_SHORT_NIE_TYPE)
		{
			return (ie->ctrl & IEEE154_SHORT_NIE_ID_MASK) >> IEEE154_SHORT_NIE_ID_SHIFT;
		}
		else if((ie->ctrl & IEEE154_IE_TYPE_MASK) == IEEE154_LONG_NIE_TYPE)
		{
			return (ie->ctrl & IEEE154_LONG_NIE_ID_MASK) >> IEEE154_LONG_NIE_ID_SHIFT;
		}
	}

	return 0;
}


/* ieee154_ie_length ****************************************************************************//**
 * @brief		Returns the total length of the IE.*/
uint16_t ieee154_ie_length(const Ieee154_IE* ie)
{
	return buffer_length(&ie->buffer);
}


/* ieee154_ie_length_content ********************************************************************//**
 * @brief		Returns the length of the content in the IE. */
uint16_t ieee154_ie_length_content(const Ieee154_IE* ie)
{
	uint16_t len = ieee154_ie_length(ie);

	if(len > IEEE154_IE_CTRL_LENGTH)
	{
		return len - IEEE154_IE_CTRL_LENGTH;
	}
	else
	{
		return 0;
	}
}


/* ieee154_ie_read_type *************************************************************************//**
 * @brief		Read an IE's ctrl field from the underlying buffer. */
static uint16_t ieee154_ie_read_type(const Buffer* b, const uint8_t* ptr)
{
	ptr = buffer_peek_at(b, ptr, 2);

	if(ptr)
	{
		return le_get_u16(ptr);
	}
	else
	{
		return IEEE154_IE_INVALID;
	}
}


/* ieee154_ie_read_length ***********************************************************************//**
 * @brief		Returns the total length of an IE as specified by the IE's ctrl value. */
static uint16_t ieee154_ie_read_length(const Ieee154_IE* parent, uint16_t type)
{
	uint16_t length = 0;

	if(type == IEEE154_IE_INVALID)
	{
		return 0;
	}
	else if(!parent)
	{
		if((type & IEEE154_IE_TYPE_MASK) == IEEE154_HIE_TYPE)
		{
			length = (type & IEEE154_HIE_LENGTH_MASK) >> IEEE154_HIE_LENGTH_SHIFT;
		}
		else if((type & IEEE154_IE_TYPE_MASK) == IEEE154_PIE_TYPE)
		{
			length = (type & IEEE154_PIE_LENGTH_MASK) >> IEEE154_PIE_LENGTH_SHIFT;
		}
	}
	else
	{
		if((type & IEEE154_IE_TYPE_MASK) == IEEE154_SHORT_NIE_TYPE)
		{
			length = (type & IEEE154_SHORT_NIE_LENGTH_MASK) >> IEEE154_SHORT_NIE_LENGTH_SHIFT;
		}
		else if((type & IEEE154_IE_TYPE_MASK) == IEEE154_LONG_NIE_TYPE)
		{
			length = (type & IEEE154_LONG_NIE_LENGTH_MASK) >> IEEE154_LONG_NIE_LENGTH_SHIFT;
		}
	}

	return length + IEEE154_IE_CTRL_LENGTH;
}


/* ieee154_hie_append ***************************************************************************//**
 * @brief		Appends a Header IE to a frame.
 * @param[out]	ie: the newly appended IE
 * @param[in]	id: the id of the IE.
 * @param[in]	ptr: pointer to the data to append.
 * @param[in]	len: length of the data to append.
 * @retval		true if the new IE was appended successfully.
 * @retval		false if the frame is full. */
bool ieee154_hie_append(Ieee154_IE* ie, uint16_t id, const void* ptr, unsigned len)
{
	/* Check that IE is actually attached to a frame */
	if(!ie->frame)
	{
		return false;
	}
	/* Check that there is enough space in the frame */
	else if(ieee154_free(ie->frame) < IEEE154_IE_CTRL_LENGTH + len)
	{
		return false;
	}

	uint16_t fctrl = ieee154_fctrl(ie->frame);
	ieee154_set_fctrl(ie->frame, fctrl | IEEE154_IE_PRESENT);

	uint16_t ctrl = IEEE154_HIE_TYPE |
		((len << IEEE154_HIE_LENGTH_SHIFT) & IEEE154_HIE_LENGTH_MASK) |
		((id  << IEEE154_HIE_ID_SHIFT) & IEEE154_HIE_ID_MASK);

	ie->ctrl = ctrl;

	ie->frame->fields[IEEE154_PAYLOAD_FIELD] += len + IEEE154_IE_CTRL_LENGTH;

	buffer_slice(&ie->buffer, buffer_parent(&ie->buffer), buffer_write(&ie->buffer), 0);

	buffer_reserve_at(&ie->buffer, buffer_start(&ie->buffer), len + IEEE154_IE_CTRL_LENGTH);
	buffer_replace_at(&ie->buffer, ptr, buffer_start(&ie->buffer) + IEEE154_IE_CTRL_LENGTH, len);
	buffer_read_seek (&ie->buffer, IEEE154_IE_CTRL_LENGTH);
	ieee154_ie_set_ctrl(ie, ctrl);
	return true;
}


/* ieee154_pie_append ***************************************************************************//**
 * @brief		Appends a Payload IE to a frame.
 * @param[out]	ie: the newly appended IE.
 * @param[in]	id: the id of the IE.
 * @param[in]	ptr: pointer to the data to append.
 * @param[in]	len: length of the data to append.
 * @retval		true if the new IE was appended successfully.
 * @retval		false if the frame is full. */
bool ieee154_pie_append(Ieee154_IE* ie, uint16_t id, const void* ptr, unsigned len)
{
	/* Check that IE is actually attached to a frame */
	if(!ie->frame)
	{
		return false;
	}
	/* Check that there is enough space in the frame */
	else if(ieee154_free(ie->frame) < IEEE154_IE_CTRL_LENGTH + len)
	{
		return false;
	}

	uint16_t fctrl = ieee154_fctrl(ie->frame);
	ieee154_set_fctrl(ie->frame, fctrl | IEEE154_IE_PRESENT);

	uint16_t ctrl = IEEE154_PIE_TYPE |
		((len << IEEE154_PIE_LENGTH_SHIFT) & IEEE154_PIE_LENGTH_MASK) |
		((id << IEEE154_PIE_ID_SHIFT) & IEEE154_PIE_ID_MASK);

	ie->ctrl = ctrl;

	ie->frame->fields[IEEE154_PAYLOAD_FIELD] += len + IEEE154_IE_CTRL_LENGTH;

	buffer_slice(&ie->buffer, buffer_parent(&ie->buffer), buffer_write(&ie->buffer), 0);

	buffer_reserve_at(&ie->buffer, buffer_start(&ie->buffer), len + IEEE154_IE_CTRL_LENGTH);
	buffer_replace_at(&ie->buffer, ptr, buffer_start(&ie->buffer) + IEEE154_IE_CTRL_LENGTH, len);
	buffer_read_seek (&ie->buffer, IEEE154_IE_CTRL_LENGTH);
	ieee154_ie_set_ctrl(ie, ctrl);
	return true;
}


/* ieee154_nie_append ***************************************************************************//**
 * @brief		Appends a Nested IE to the parent IE.
 * @param[out]	ie: the newly appended IE.
 * @param[in]	id: the id of the new nested IE.
 * @param[in]	ptr: pointer to the data to append.
 * @param[in]	len: length of the data to append.
 * @retval		true if the new IE was appended successfully.
 * @retval		false if the frame is full. */
bool ieee154_nie_append(Ieee154_IE* ie, uint16_t id, const void* ptr, unsigned len)
{
	/* Check that IE is actually attached to a frame */
	if(!ie->frame)
	{
		return false;
	}
	/* Check that there is enough space in the frame */
	else if(ieee154_free(ie->frame) < IEEE154_IE_CTRL_LENGTH + len)
	{
		return false;
	}

	uint16_t ctrl;

	if(id <= 0x0F)
	{
		ctrl = IEEE154_LONG_NIE_TYPE |
			((len << IEEE154_LONG_NIE_LENGTH_SHIFT) & IEEE154_LONG_NIE_LENGTH_MASK) |
			((id  << IEEE154_LONG_NIE_ID_SHIFT)     & IEEE154_LONG_NIE_ID_MASK);
	}
	else
	{
		ctrl = IEEE154_SHORT_NIE_TYPE |
			((len << IEEE154_SHORT_NIE_LENGTH_SHIFT) & IEEE154_SHORT_NIE_LENGTH_MASK) |
			((id  << IEEE154_SHORT_NIE_ID_SHIFT)     & IEEE154_SHORT_NIE_ID_MASK);
	}

	ie->ctrl = ctrl;

	ie->frame->fields[IEEE154_PAYLOAD_FIELD] += len + IEEE154_IE_CTRL_LENGTH;

	buffer_slice(&ie->buffer, buffer_parent(&ie->buffer), buffer_write(&ie->buffer), 0);

	buffer_reserve_at(&ie->buffer, buffer_start(&ie->buffer), len + IEEE154_IE_CTRL_LENGTH);
	buffer_replace_at(&ie->buffer, ptr, buffer_start(&ie->buffer) + IEEE154_IE_CTRL_LENGTH, len);
	buffer_read_seek (&ie->buffer, IEEE154_IE_CTRL_LENGTH);
	ieee154_ie_set_ctrl(ie, ctrl);

	/* Update lengths */
	Ieee154_IE* it;
	for(it = ieee154_ie_parent(ie); it != 0; it = ieee154_ie_parent(it))
	{
		/* @TODO: check if len overflows? */
		it->ctrl += len + IEEE154_IE_CTRL_LENGTH;

		ieee154_ie_set_ctrl(it, it->ctrl);
	}

	return true;
}


// /* ieee154_ie_set_length ************************************************************************//**
//  * @brief		Sets the length of the IE. */
// static void ieee154_ie_set_length(Ieee154_IE* ie, uint16_t len)
// {
// 	if(!ie->parent)
// 	{
// 		if((ie->ctrl & IEEE154_IE_TYPE_MASK) == IEEE154_IE_TYPE_HEADER)
// 		{
// 			ie->ctrl = (ie->ctrl & ~IEEE154_HEADER_IE_LENGTH_MASK) | len;
// 		}
// 		else if((ie->ctrl & IEEE154_IE_TYPE_MASK) == IEEE154_IE_TYPE_PAYLOAD)
// 		{
// 			ie->ctrl = (ie->ctrl & ~IEEE154_PAYLOAD_IE_LENGTH_MASK) | len;
// 		}
// 	}
// 	else
// 	{
// 		if((ie->ctrl & IEEE154_IE_TYPE_MASK) == IEEE154_SHORT_NESTED_IE_TYPE)
// 		{
// 			ie->ctrl = (ie->ctrl & ~IEEE154_SHORT_NESTED_IE_LENGTH_MASK) | len;
// 		}
// 		else if((ie->ctrl & IEEE154_IE_TYPE_MASK) == IEEE154_LONG_NESTED_IE_TYPE)
// 		{
// 			ie->ctrl = (ie->ctrl & ~IEEE154_LONG_NESTED_IE_LENGTH_MASK) | len;
// 		}
// 	}

// 	uint8_t buf[2] = { (ie->ctrl >> 0) & 0xFF, (ie->ctrl >> 8) & 0xFF };
// 	memmove(&ie->frame->data[ie->start], buf, IEEE154_IE_CTRL_LENGTH);
// }


/* ieee154_ie_ptr_start *************************************************************************//**
 * @brief		Returns a pointer to the start of the ie. */
void* ieee154_ie_ptr_start(const Ieee154_IE* ie)
{
	if(!ieee154_ie_is_valid(ie))
	{
		return 0;
	}
	else
	{
		return buffer_peek_offset(&ie->buffer, 0, 0);
	}
}


/* ieee154_ie_content ***************************************************************************//**
 * @brief		Returns a pointer to the content of the ie. */
void* ieee154_ie_ptr_content(const Ieee154_IE* ie)
{
	if(!ieee154_ie_is_valid(ie))
	{
		return 0;
	}
	else
	{
		return buffer_peek_offset(&ie->buffer, IEEE154_IE_CTRL_LENGTH, 0);
	}
}


/* ieee154_ie_set_ctrl **************************************************************************//**
 * @brief		Sets the IE's ctrl field. */
static void ieee154_ie_set_ctrl(Ieee154_IE* ie, uint16_t ctrl)
{
	ie->ctrl = ctrl;

	le_set_u16(buffer_peek_at(&ie->buffer, buffer_start(&ie->buffer), 2), ctrl);
}


/* ieee154_ie_reset_buffer **********************************************************************//**
 * @brief		Returns the IE's buffer. The buffer's read and write pointers are reset so that bytes
 * 				can be read from the start of the IE's content and written to the end of the IE's
 * 				content.
 * @warning		Call ieee154_ie_finalize after writing bytes to the IE. */
Buffer* ieee154_ie_reset_buffer(Ieee154_IE* ie)
{
	buffer_read_seek(&ie->buffer, IEEE154_IE_CTRL_LENGTH);
	return &ie->buffer;
}


/* ieee154_ie_finalize **************************************************************************//**
 * @brief		Performs final tasks after writing to an IE.
 * @desc		1.	Updates the length of this IE and all parent IEs. */
void ieee154_ie_finalize(Ieee154_IE* ie)
{
	uint16_t ctrl = ieee154_ie_read_type(&ie->buffer, buffer_start(&ie->buffer));
	unsigned grow = ieee154_ie_length(ie) - ieee154_ie_read_length(ieee154_ie_parent(ie), ctrl);

	ie->frame->fields[IEEE154_PAYLOAD_FIELD] += grow;

	/* Update lengths */
	while(ie)
	{
		ie->ctrl += grow;	/* Exploit the fact that length is the low order bits */

		ieee154_ie_set_ctrl(ie, ie->ctrl);

		ie = ieee154_ie_parent(ie);
	}
}





// ----------------------------------------------------------------------------------------------- //
// Private Functions                                                                               //
// ----------------------------------------------------------------------------------------------- //
/* ieee154_field ********************************************************************************//**
 * @brief		Returns a pointer to the specified field if the field exists. Returns null if the
 * 				field does not exist (length is zero). Field is one of:
 *
 * 					IEEE154_SEQ_NUM_FIELD
 * 					IEEE154_DEST_PANID_FIELD
 * 					IEEE154_DEST_ADDR_FIELD
 * 					IEEE154_SRC_PANID_FIELD
 * 					IEEE154_SRC_ADDR_FIELD
 * 					IEEE154_SEC_FIELD
 * 					IEEE154_IE_FIELD
 * 					IEEE154_PAYLOAD_FIELD
 */
static void* ieee154_field(const Ieee154_Frame* frame, unsigned field)
{
	uint8_t* ptr = buffer_offset(&frame->buffer, frame->fields[field]);

	if(field == IEEE154_PAYLOAD_FIELD)
	{
		return buffer_peek_at(&frame->buffer, ptr, 0);
	}

	uint16_t len = frame->fields[field + 1] - frame->fields[field];

	if(len)
	{
		return buffer_peek_at(&frame->buffer, ptr, 0);
	}
	else
	{
		return 0;
	}
}


/* fctrl_pan_id_compress ************************************************************************//**
 * @brief		Returns the value of the pan id compression bit given an IEEE 802.15.4 frame control
 * 				field. */
static bool fctrl_pan_id_compress(uint16_t fctrl)
{
	return (fctrl & IEEE154_PAN_ID_COMPRESS_MASK) == IEEE154_PAN_ID_COMPRESS;
}


/* fctrl_length_sequnum *************************************************************************//**
 * @brief		Returns the length in bytes of the seqnum field given an IEEE 802.15.4 frame control
 * 				field. */
static unsigned fctrl_length_seqnum(uint16_t fctrl)
{
	if(fctrl & IEEE154_SEQ_NUM_SUPPRESS)
	{
		return 0;
	}
	else
	{
		return IEEE154_SEQ_NUM_LENGTH;
	}
}


/* fctrl_length_dest_pan_id *********************************************************************//**
 * @brief		Returns the length in bytes of the destination PAN ID field given an IEEE 802.15.4
 * 				frame control field. */
static unsigned fctrl_length_dest_pan_id(uint16_t fctrl)
{
	return fctrl_dest_pan_id_present(fctrl) * IEEE154_DEST_PAN_ID_LENGTH;
}


/* fctrl_length_dest_addr ************************************************************************//**
 * @brief		Returns the length in bytes of the destination address field given an IEEE 802.15.4
 * 				frame control field. */
static unsigned fctrl_length_dest_addr(uint16_t fctrl)
{
	if((fctrl & IEEE154_DAM_MASK) == IEEE154_DAM_SHORT)
	{
		return IEEE154_DEST_SHORT_LENGTH;
	}
	else if((fctrl & IEEE154_DAM_MASK) == IEEE154_DAM_EXTENDED)
	{
		return IEEE154_DEST_EXTENDED_LENGTH;
	}
	else
	{
		return 0;
	}
}


/* fctrl_length_src_pan_id **********************************************************************//**
 * @brief		Returns the length in bytes of the source PAN ID field given an IEEE 802.15.4 frame
 * 				control field. */
static unsigned fctrl_length_src_pan_id(uint16_t fctrl)
{
	return fctrl_src_pan_id_present(fctrl) * IEEE154_SRC_PAN_ID_LENGTH;
}


/* fctrl_length_src_addr ************************************************************************//**
 * @brief		Returns the length in bytes of the source address field given an IEEE 802.15.4 frame
 * 				control field. */
static unsigned fctrl_length_src_addr(uint16_t fctrl)
{
	if((fctrl & IEEE154_SAM_MASK) == IEEE154_SAM_SHORT)
	{
		return IEEE154_SRC_SHORT_LENGTH;
	}
	else if((fctrl & IEEE154_SAM_MASK) == IEEE154_SAM_EXTENDED)
	{
		return IEEE154_SRC_EXTENDED_LENGTH;
	}
	else
	{
		return 0;
	}
}


///* fctrl_length_ash *****************************************************************************//**
// * @brief		Returns the length in bytes of the source aux security header field given an IEEE
// * 				802.15.4 frame control field.
// * @TODO:		Implement. */
//static unsigned fctrl_length_ash(uint16_t fctrl)
//{
//	return 0;
//}


/* fctrl_dest_pan_id_present ********************************************************************//**
 * @brief		Returns true if the dest pan id field is present given an IEEE 802.15.4 frame control
 * 				field. */
static bool fctrl_dest_pan_id_present(uint16_t fctrl)
{
	uint16_t src_mode  = fctrl & IEEE154_SAM_MASK;
	uint16_t dest_mode = fctrl & IEEE154_DAM_MASK;

	/* Dest. Addr.     Source Addr.    Dest PAN ID     Source PAN ID   PAN ID Compression
	 * Not Present     Not Present     Not Present*    Not Present     0
	 * Not Present     Not Present     Present*        Not Present     1
	 * Short           Not Present     Present*        Not Present     0
	 * Extended        Not Present     Present*        Not Present     0
	 * Short           Not Present     Not Present*    Not Present     1
	 * Extended        Not Present     Not Present*    Not Present     1 */
	if(src_mode == IEEE154_SAM_NOT_PRESENT)
	{
		return (dest_mode != IEEE154_DAM_NOT_PRESENT) ^ fctrl_pan_id_compress(fctrl);
	}
	/* Not Present     Short           Not Present*    Present         0
	 * Not Present     Extended        Not Present*    Present         0
	 * Not Present     Short           Not Present*    Not Present     1
	 * Not Present     Extended        Not Present*    Not Present     1 */
	else if(dest_mode == IEEE154_DAM_NOT_PRESENT)
	{
		return false;
	}
	/* Extended        Extended        Present*        Not Present     0
	 * Extended        Extended        Not Present*    Not Present     1 */
	else if(src_mode == IEEE154_SAM_EXTENDED && dest_mode == IEEE154_DAM_EXTENDED)
	{
		return !fctrl_pan_id_compress(fctrl);
	}
	/* Short           Short           Present*        Present         0
	 * Short           Extended        Present*        Present         0
	 * Extended        Short           Present*        Present         0
	 * Short           Short           Present*        Not Present     1
	 * Short           Extended        Present*        Not Present     1
	 * Extended        Short           Present*        Not Present     1 */
	else
	{
		return true;
	}
}


/* fctrl_src_pan_id_present *********************************************************************//**
 * @brief		Returns true if the src pan id field is present given an IEEE 802.15.4 frame control
 * 				field. */
static bool fctrl_src_pan_id_present(uint16_t fctrl)
{
	uint16_t src_mode  = fctrl & IEEE154_SAM_MASK;
	uint16_t dest_mode = fctrl & IEEE154_DAM_MASK;

	/* Dest. Addr.     Source Addr.    Dest PAN ID     Source PAN ID   PAN ID Compression
	 * Not Present     Not Present     Not Present     Not Present*    0
	 * Not Present     Not Present     Present         Not Present*    1
	 * Short           Not Present     Present         Not Present*    0
	 * Extended        Not Present     Present         Not Present*    0
	 * Short           Not Present     Not Present     Not Present*    1
	 * Extended        Not Present     Not Present     Not Present*    1 */
	if(src_mode == IEEE154_SAM_NOT_PRESENT)
	{
		return false;
	}
	/* Not Present     Short           Not Present     Present*        0
	 * Not Present     Extended        Not Present     Present*        0
	 * Not Present     Short           Not Present     Not Present*    1
	 * Not Present     Extended        Not Present     Not Present*    1 */
	else if(dest_mode == IEEE154_DAM_NOT_PRESENT)
	{
		return (src_mode != IEEE154_SAM_NOT_PRESENT) && !fctrl_pan_id_compress(fctrl);
	}
	/* Extended        Extended        Present         Not Present*    0
	 * Extended        Extended        Not Present     Not Present*    1 */
	else if(src_mode == IEEE154_SAM_EXTENDED && dest_mode == IEEE154_DAM_EXTENDED)
	{
		return false;
	}
	/* Short           Short           Present         Present*        0
	 * Short           Extended        Present         Present*        0
	 * Extended        Short           Present         Present*        0
	 * Short           Short           Present         Not Present*    1
	 * Short           Extended        Present         Not Present*    1
	 * Extended        Short           Present         Not Present*    1 */
	else
	{
		return !fctrl_pan_id_compress(fctrl);
	}
}


/* fctrl_addr_mode_valid ************************************************************************//**
 * @brief		Returns true if the addressing mode if valid. */
static bool fctrl_addr_mode_valid(
	uint16_t dest_mode,
	uint16_t src_mode,
	bool dest_pan_id_present,
	bool src_pan_id_present)
{
	/* Dest. Addr.     Source Addr.    Dest PAN ID     Source PAN ID   PAN ID Compression
	 * Not Present     Not Present     Not Present     Present            invalid
	 * Not Present     Not Present     Present         Present            invalid */
	if(dest_mode == IEEE154_DAM_NOT_PRESENT &&
	   src_mode  == IEEE154_SAM_NOT_PRESENT && src_pan_id_present)
	{
		return false;
	}
	/* Short           Not Present     Present         Present            invalid
	 * Extended        Not Present     Present         Present            invalid
	 * Short           Not Present     Not Present     Present            invalid
	 * Extended        Not Present     Not Present     Present            invalid */
	else if(dest_mode != IEEE154_DAM_NOT_PRESENT &&
	        src_mode  == IEEE154_SAM_NOT_PRESENT && src_pan_id_present)
	{
		return false;
	}
	/* Not Present     Short           Present         Present            invalid
	 * Not Present     Extended        Present         Present            invalid
	 * Not Present     Short           Present         Not Present        invalid
	 * Not Present     Extended        Present         Not Present        invalid */
	else if(dest_mode == IEEE154_DAM_NOT_PRESENT &&
	        src_mode  != IEEE154_SAM_NOT_PRESENT && dest_pan_id_present)
	{
		return false;
	}
	/* Extended        Extended        Present         Not Present     0
	 * Extended        Extended        Not Present     Not Present     1
	 * Extended        Extended        Present         Present            invalid
	 * Extended        Extended        Not Present     Present            invalid */
	else if(dest_mode == IEEE154_DAM_EXTENDED &&
	        src_mode  == IEEE154_SAM_EXTENDED)
	{
		return !src_pan_id_present;
	}
	/* Short           Short           Not Present     Present            invalid
	 * Short           Extended        Not Present     Present            invalid
	 * Extended        Short           Not Present     Present            invalid
	 * Short           Short           Not Present     Not Present        invalid
	 * Short           Extended        Not Present     Not Present        invalid
	 * Extended        Short           Not Present     Not Present        invalid */
	else if(dest_mode != IEEE154_DAM_NOT_PRESENT &&
	        src_mode  != IEEE154_SAM_NOT_PRESENT && !dest_pan_id_present)
	{
		return false;
	}
	else
	{
		return true;
	}
}


/* fctrl_set_addr_mode **************************************************************************//**
 * @brief		Sets the fctrl field for the given addressing mode if valid. */
static bool fctrl_set_addr_mode(
	uint16_t* fctrl,
	uint16_t dest_mode,
	uint16_t src_mode,
	bool dest_pan_id_present,
	bool src_pan_id_present)
{
	if(!fctrl_addr_mode_valid(dest_mode, src_mode, dest_pan_id_present, src_pan_id_present))
	{
		return false;
	}

	*fctrl &= ~(IEEE154_DAM_MASK | IEEE154_SAM_MASK | IEEE154_PAN_ID_COMPRESS_MASK);

	*fctrl |= (dest_mode | src_mode);

	/* Dest. Addr.     Source Addr.    Dest PAN ID     Source PAN ID   PAN ID Compression
	 * Not Present     Not Present     Not Present     Not Present     0
	 * Not Present     Not Present     Present         Not Present     1 */
	if(dest_mode == IEEE154_DAM_NOT_PRESENT && src_mode == IEEE154_SAM_NOT_PRESENT)
	{
		*fctrl |= (dest_pan_id_present) << IEEE154_PAN_ID_COMPRESS_SHIFT;
	}
	/* Not Present     Short           Not Present     Present         0
	 * Not Present     Extended        Not Present     Present         0
	 * Not Present     Short           Not Present     Not Present     1
	 * Not Present     Extended        Not Present     Not Present     1 */
	else if(dest_mode == IEEE154_DAM_NOT_PRESENT && src_mode != IEEE154_SAM_NOT_PRESENT)
	{
		*fctrl |= (!src_pan_id_present) << IEEE154_PAN_ID_COMPRESS_SHIFT;
	}
	/* Short           Not Present     Present         Not Present     0
	 * Extended        Not Present     Present         Not Present     0
	 * Short           Not Present     Not Present     Not Present     1
	 * Extended        Not Present     Not Present     Not Present     1 */
	else if(dest_mode != IEEE154_DAM_NOT_PRESENT && src_mode == IEEE154_SAM_NOT_PRESENT)
	{
		*fctrl |= (!dest_pan_id_present) << IEEE154_PAN_ID_COMPRESS_SHIFT;
	}
	/* Extended        Extended        Present         Not Present     0
	 * Extended        Extended        Not Present     Not Present     1 */
	else if(dest_mode == IEEE154_DAM_EXTENDED && src_mode == IEEE154_SAM_EXTENDED)
	{
		*fctrl |= (!dest_pan_id_present) << IEEE154_PAN_ID_COMPRESS_SHIFT;
	}
	/* Short           Short           Present         Present         0
	 * Short           Extended        Present         Present         0
	 * Extended        Short           Present         Present         0
	 * Short           Short           Present         Not Present     1
	 * Short           Extended        Present         Not Present     1
	 * Extended        Short           Present         Not Present     1 */
	else
	{
		*fctrl |= (!src_pan_id_present) << IEEE154_PAN_ID_COMPRESS_SHIFT;
	}

	return true;
}






// ----------------------------------------------------------------------------------------------- //
// IEs                                                                                             //
// ----------------------------------------------------------------------------------------------- //
/* 0x1A - TSCH Synchronization IE ---------------------------------------------------------------- */
/* tsch_sync_ie_append **************************************************************************//**
 * @brief  		*/
bool tsch_sync_ie_append(Ieee154_IE* nie, uint64_t asn, uint8_t join_metric)
{
	/* uint16_t ie_fctrl;
	 * uint8_t  asn[5];
	 * uint8_t  join_metric; */

	asn = le_u64(asn);

	return ieee154_nie_append(nie, IEEE154_TSCH_SYNC_IE, 0, 0) &&
	       buffer_push_mem(&nie->buffer, &asn, 5)              &&
	       buffer_push_u8 (&nie->buffer, join_metric);
}


/* tsch_sync_ie_asn *****************************************************************************//**
 * @brief  		*/
uint64_t tsch_sync_ie_asn(const Ieee154_IE* nie)
{
	uint8_t asn[5];

	buffer_read_offset(&nie->buffer, asn, 2, sizeof(asn));

	return (uint64_t)(asn[0]) << 0  |
	       (uint64_t)(asn[1]) << 8  |
	       (uint64_t)(asn[2]) << 16 |
	       (uint64_t)(asn[3]) << 24 |
	       (uint64_t)(asn[4]) << 32;
}


/* tsch_sync_ie_join_metric *********************************************************************//**
 * @brief  		*/
uint8_t tsch_sync_ie_join_metric(const Ieee154_IE* nie)
{
	uint8_t metric = 255;
	buffer_read_offset(&nie->buffer, &metric, 7, 1);
	return metric;
}





/* 0x1B - TSCH Slotframe and Link IE ------------------------------------------------------------- */
/* tsch_sf_link_ie_append ***********************************************************************//**
 * @brief		Appends a TSCH Slotframe Link IE. Call tsch_sf_desc_append and tsch_link_info_append
 * 				to add slotframe details. */
bool tsch_sf_link_ie_append(Ieee154_IE* nie, TschSFLinkIE* sf_link_ie)
{
	if(!ieee154_nie_append(nie, IEEE154_TSCH_SLOTFRAME_AND_LINK_IE, 0, 0))
	{
		return false;
	}

	sf_link_ie->nie   = nie;
	sf_link_ie->start = buffer_write(&nie->buffer);
	return buffer_push_u8(&nie->buffer, le_u8(0));	/* uint8_t num_slotframes */
}


/* tsch_sf_link_ie_read *************************************************************************//**
 * @brief		Pops a TSCH Slotframe and Link IE from the IEEE154 frame. */
bool tsch_sf_link_ie_read(Ieee154_IE* nie, TschSFLinkIE* sf_link_ie)
{
	sf_link_ie->nie   = nie;
	sf_link_ie->start = buffer_pop(&nie->buffer, 1);
	sf_link_ie->descr_remaining = tsch_sf_link_ie_num_sf(sf_link_ie);
	return sf_link_ie->start != 0;
}


/* tsch_sf_link_ie_num_sf ***********************************************************************//**
 * @brief		Returns the number of slotframes in the TSCH Slotframe and Link IE. */
uint8_t tsch_sf_link_ie_num_sf(const TschSFLinkIE* sf_link_ie)
{
	return le_get_u8(sf_link_ie->start);
}


/* tsch_sf_desc_append **************************************************************************//**
 * @brief		Appends a Slotframe Descriptor to the TSCH Slotframe and Link IE. */
bool tsch_sf_desc_append(TschSFLinkIE* sf_link_ie, uint8_t sf_handle, uint16_t sf_size)
{
	sf_link_ie->sf_descr = buffer_write(&sf_link_ie->nie->buffer);

	/* Append a descriptor (4 bytes):
	 * 		uint8_t  slotframe_handle
	 * 		uint16_t slotframe_size
	 * 		uint8_t  num_links
	 */
	if(!buffer_push_u8 (&sf_link_ie->nie->buffer, le_u8 (sf_handle)) ||
	   !buffer_push_u16(&sf_link_ie->nie->buffer, le_u16(sf_size))   ||
	   !buffer_push_u8 (&sf_link_ie->nie->buffer, le_u8 (0)))
	{
		return false;
	}

	/* Update TschSFLinkIE num_slotframes */
	le_set_u8(sf_link_ie->start + 0, tsch_sf_link_ie_num_sf(sf_link_ie) + 1);
	return true;
}


/* tsch_sf_desc_read ****************************************************************************//**
 * @brief		Pops a Slotframe Descriptor from the TSCH Slotframe and Link IE. */
bool tsch_sf_desc_read(TschSFLinkIE* sf_link_ie)
{
	/* Remove a descriptor (4 bytes):
	 * 		uint8_t  slotframe_handle
	 * 		uint16_t slotframe_size
	 * 		uint8_t  num_links
	 */
	if(!sf_link_ie->descr_remaining)
	{
		return false;
	}
	else if(!(sf_link_ie->sf_descr = buffer_pop(&sf_link_ie->nie->buffer, 4)))
	{
		return false;
	}

	sf_link_ie->links_remaining = tsch_sf_desc_num_links(sf_link_ie);
	sf_link_ie->descr_remaining -= 1;
	return true;
}


/* tsch_sf_desc_sf_handle ***********************************************************************//**
 * @brief		Returns the Slotframe Descriptor's handle. */
uint8_t tsch_sf_desc_sf_handle(const TschSFLinkIE* sf_link_ie)
{
	return le_get_u8(buffer_peek_at(&sf_link_ie->nie->buffer, sf_link_ie->sf_descr + 0, 1));
}


/* tsch_sf_desc_sf_size *************************************************************************//**
 * @brief		Returns the Slotframe Descriptor's size. */
uint16_t tsch_sf_desc_sf_size(const TschSFLinkIE* sf_link_ie)
{
	return le_get_u16(buffer_peek_at(&sf_link_ie->nie->buffer, sf_link_ie->sf_descr + 1, 2));
}


/* tsch_sf_desc_num_links ***********************************************************************//**
 * @brief		Returns the number of Link Information structs in the Slotframe Descriptor. */
uint8_t tsch_sf_desc_num_links(const TschSFLinkIE* sf_link_ie)
{
	return le_get_u8(buffer_peek_at(&sf_link_ie->nie->buffer, sf_link_ie->sf_descr + 3, 1));
}


/* tsch_link_info_append ************************************************************************//**
 * @brief		Appends a Link Information struct to the Slotframe Descriptor. */
bool tsch_link_info_append(TschSFLinkIE* sf_link_ie, uint16_t timeslot, uint16_t ch, uint8_t flags)
{
	sf_link_ie->link_info = buffer_write(&sf_link_ie->nie->buffer);

	/* Append a link info (5 bytes):
	 * 		uint16_t timeslot
	 * 		uint16_t offset
	 * 		uint8_t  options
	 */
	if(!buffer_push_u16(&sf_link_ie->nie->buffer, le_u16(timeslot)) ||
	   !buffer_push_u16(&sf_link_ie->nie->buffer, le_u16(ch))       ||
	   !buffer_push_u8 (&sf_link_ie->nie->buffer, le_u8 (flags)))
	{
		return false;
	}

	/* Update TschSFDesc num_links */
	le_set_u8(sf_link_ie->sf_descr + 3, tsch_sf_desc_num_links(sf_link_ie) + 1);
	return true;
}


/* tsch_link_info_read **************************************************************************//**
 * @brief		Pops a Link Information struct from the Slotframe Descriptor. */
bool tsch_link_info_read(TschSFLinkIE* sf_link_ie)
{
	/* Remove a link info (5 bytes):
	 * 		uint16_t timeslot
	 * 		uint16_t offset
	 * 		uint8_t  options
	 */
	if(!sf_link_ie->links_remaining)
	{
		return false;
	}
	else if(!(sf_link_ie->link_info = buffer_pop(&sf_link_ie->nie->buffer, 5)))
	{
		return false;
	}

	sf_link_ie->links_remaining -= 1;
	return true;
}


/* tsch_link_info_timeslot **********************************************************************//**
 * @brief		Returns the Link Information struct's timeslot. */
uint16_t tsch_link_info_timeslot(const TschSFLinkIE* sf_link_ie)
{
	return sf_link_ie->link_info ? le_get_u16(sf_link_ie->link_info + 0) : 0;
}


/* tsch_link_info_offset ************************************************************************//**
 * @brief		Returns the Link Information struct's channel offset. */
uint16_t tsch_link_info_offset(const TschSFLinkIE* sf_link_ie)
{
	return sf_link_ie->link_info ? le_get_u16(sf_link_ie->link_info + 2) : 0;
}


/* tsch_link_info_options ***********************************************************************//**
 * @brief		Returns the Link Information struct's options= flags field. */
uint8_t tsch_link_info_options(const TschSFLinkIE* sf_link_ie)
{
	return sf_link_ie->link_info ? le_get_u16(sf_link_ie->link_info + 4) : 0;
}


/******************************************* END OF FILE *******************************************/
