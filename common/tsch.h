/************************************************************************************************//**
 * @file		tsch.h
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
 * @desc
 * @TODO		Add support for IETF subtypes in PIEs.
 *
 ***************************************************************************************************/
#ifndef TSCH_H
#define TSCH_H

#if __STDC_VERSION__ < 199901L
#error Compile with C99 or higher!
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* Public Includes ------------------------------------------------------------------------------- */
#include <net/net_if.h>

// #include "backoff.h"
#include "bayesian.h"
#include "ieee_802_15_4.h"


/* Public Macros --------------------------------------------------------------------------------- */
// #define TSCH_DEFAULT_NUM_SLOTS (40)
#define TSCH_DEFAULT_NUM_SLOTS (100)


// ----------------------------------------------------------------------------------------------- //
// TSCH Version Numbers                                                                            //
// ----------------------------------------------------------------------------------------------- //
#define TSCH_VERSION        (0)
#define TSCH_IE             (5)
#define TSCH_SUBTYPE_6TOP   (1)

#define TSCH_SSID_IE        (70)
#define TSCH_SYNC_IE        (71)
#define TSCH_HYPERBEACON_ID (72)
#define TSCH_TRESP_IE       (73)


// ----------------------------------------------------------------------------------------------- //
// TSCH Message Types                                                                              //
// ----------------------------------------------------------------------------------------------- //
#define TSCH_TYPE_REQUEST	(0x0)
#define TSCH_TYPE_RESPONSE	(0x1)
#define TSCH_TYPE_CONFIRM	(0x2)


// ----------------------------------------------------------------------------------------------- //
// TSCH Command IDs                                                                                //
// ----------------------------------------------------------------------------------------------- //
#define TSCH_CMD_ADD		(1)
#define TSCH_CMD_DELETE		(2)
#define TSCH_CMD_RELOCATE	(3)
#define TSCH_CMD_COUNT		(4)
#define TSCH_CMD_LIST		(5)
#define TSCH_CMD_SIGNAL		(6)
#define TSCH_CMD_CLEAR		(7)
#define TSCH_CMD_CONNECT	(128)


// ----------------------------------------------------------------------------------------------- //
// TSCH Return Codes                                                                               //
// ----------------------------------------------------------------------------------------------- //
#define TSCH_RC_SUCCESS			(0)	/*  No Error. Operation succeeded    */
#define TSCH_RC_EOL				(1)	/*  No Error. End of list            */
#define TSCH_RC_ERR				(2)	/* Yes Error. Generic error          */
#define TSCH_RC_RESET			(3)	/* Yes Error. Critical error, reset  */
#define TSCH_RC_ERR_VERSION		(4)	/* Yes Error. Unsupported 6P version */
#define TSCH_RC_ERR_SFID		(5)	/* Yes Error. Unsupported SFID       */
#define TSCH_RC_ERR_SEQNUM		(6)	/* Yes Error. Schedule inconsistency */
#define TSCH_RC_ERR_CELLLIST	(7)	/* Yes Error. CellList error         */
#define TSCH_RC_ERR_BUSY		(8)	/* Yes Error. Busy                   */
#define TSCH_RC_ERR_LOCKED		(9)	/* Yes Error. Cells are locked       */


// ----------------------------------------------------------------------------------------------- //
// TSCH Cell Options Bitmap                                                                        //
// ----------------------------------------------------------------------------------------------- //
#define TSCH_TX		(0x01)
#define TSCH_RX		(0x02)
#define TSCH_SHARED	(0x04)
#define TSCH_LOC	(0x08)
#define TSCH_SCAN	(0x80)



/* Public Types ---------------------------------------------------------------------------------- */
/* Generic TSCH Message Format
 * 		                     1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |     SeqNum    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		| Other Fields...
 * 		+-+-+-+-+-+-+-+-+-
 *
 * 		Version: the version of TSCH. Only version 0 is defined.
 *
 * 		Type (T): the type of message
 *
 * 		Reserved (R): Reserved bits. SHOULD be set to zero when sending the message and MUST be
 * 		ignored upon reception.
 *
 * 		Code: the code field coontains a TSCH command identifier when the TSCH has a Type value of
 * 		REQUEST. The Code field contains a TSCH return code when the TSCH message has a Type value of
 * 		RESPONSE or CONFIRMATION.
 *
 * 		Scheduling Function Identifier (SFID): The identifier of the SFto use to handle this message.
 *
 * 		SeqNum: The sequence number associated with the TSCH Transaction. Used to match TSCH Request,
 * 		TSCH Response, and TSCH Confirmation of the same TSCH Transaction. The value of SeqNum MUST
 * 		be different for each new TSCH Request issued to the same neighbor and using the smae SF. The
 * 		SeqNum is also used to ensure consistency between the schedules of the two neighbors.
 *
 *
 *
 * 		CellOptions
 * 		An 8-bit CellOptions bitmap is present in the following TSCH Requests: ADD, DELETE, LIST, and
 * 		RELOCATE. The format and meaning of this field MAY be redefined by the SF.
 *
 * 			1.	In ADD Requests, the CellOptions bitmap is used to specify what type of cell to add.
 * 			2.	In DELETE Requests, the CellOptions bitmap is used to specify what type of cell to
 * 				delete.
 * 			3.	In RELOCATE Requests, the CellOptions bitmap is used to specify what type of cell to
 * 				relocate.
 * 			4.	In COUNT and LIST Requests the CellOptions bitmap is used as a selector of a
 * 				particular type of cell.
 *
 *
 * 			Note: Assume that node A issues the command to node B.
 * 			+-------------+-----------------------------------------------------+
 * 			| CellOptions | The type of cells B adds/deletes/relocates to its   |
 * 			| Value       | schedule when receiving a 6P ADD/DELETE/RELOCATE    |
 * 			|             | Request from A                                      |
 * 			+-------------+-----------------------------------------------------+
 * 			|TX=0,RX=0,S=0| Invalid combination.  RC_ERR is returned            |
 * 			+-------------+-----------------------------------------------------+
 * 			|TX=1,RX=0,S=0| Add/delete/relocate RX cells at B (TX cells at A)   |
 * 			+-------------+-----------------------------------------------------+
 * 			|TX=0,RX=1,S=0| Add/delete/relocate TX cells at B (RX cells at A)   |
 * 			+-------------+-----------------------------------------------------+
 * 			|TX=1,RX=1,S=0| Add/delete/relocate TX|RX cells at B (and at A)     |
 * 			+-------------+-----------------------------------------------------+
 * 			|TX=0,RX=0,S=1| Invalid combination.  RC_ERR is returned            |
 * 			+-------------+-----------------------------------------------------+
 * 			|TX=1,RX=0,S=1| Add/delete/relocate RX|SHARED cells at B            |
 * 			|             | (TX|SHARED cells at A)                              |
 * 			+-------------+-----------------------------------------------------+
 * 			|TX=0,RX=1,S=1| Add/delete/relocate TX|SHARED cells at B            |
 * 			|             | (RX|SHARED cells at A)                              |
 * 			+-------------+-----------------------------------------------------+
 * 			|TX=1,RX=1,S=1| Add/delete/relocate TX|RX|SHARED cells at B         |
 * 			|             | (and at A)                                          |
 * 			+-------------+-----------------------------------------------------+
 *
 * 			Note: Assume that node A issues the command to node B.
 * 			+-------------+-----------------------------------------------------+
 * 			| CellOptions | The type of cells B selects from its schedule when  |
 * 			| Value       | receiving a 6P COUNT or LIST Request from A,        |
 * 			|             | from all the cells B has scheduled with A           |
 * 			+-------------+-----------------------------------------------------+
 * 			|TX=0,RX=0,S=0| All cells                                           |
 * 			+-------------+-----------------------------------------------------+
 * 			|TX=1,RX=0,S=0| All cells marked as RX only                         |
 * 			+-------------+-----------------------------------------------------+
 * 			|TX=0,RX=1,S=0| All cells marked as TX only                         |
 * 			+-------------+-----------------------------------------------------+
 * 			|TX=1,RX=1,S=0| All cells marked as TX and RX only                  |
 * 			+-------------+-----------------------------------------------------+
 * 			|TX=0,RX=0,S=1| All cells marked as SHARED (regardless of TX, RX)   |
 * 			+-------------+-----------------------------------------------------+
 * 			|TX=1,RX=0,S=1| All cells marked as RX and SHARED only              |
 * 			+-------------+-----------------------------------------------------+
 * 			|TX=0,RX=1,S=1| All cells marked as TX and SHARED only              |
 * 			+-------------+-----------------------------------------------------+
 * 			|TX=1,RX=1,S=1| All cells marked as TX, RX, and SHARED              |
 * 			+-------------+-----------------------------------------------------+
 *
 * 		CellList
 * 		A CellList field MAY be present in a 6P ADD Request, a 6P DELETE Request, a 6P RELOCATE
 * 		Request, a 6P Response, or a 6P Confirmation. It is composed of a concatenation of zero or
 * 		more 6P Cells as defined in Figure 9. The content of the CellOptions field specifies the
 * 		options associated with all cells in the CellList. This necessarily means that the same
 * 		options are associated with all cells in the CellList.
 *
 * 		A 6P Cell is a 4-byte field; its default format is:
 * 			                     1                   2                   3
 * 			 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 			+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 			|          slotOffset           |         channelOffset         |
 * 			+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * 			slotOffset: The slot offset of the cell.
 *
 * 			channelOffset: The channel offset of the cell.
 */
typedef struct {
	uint8_t tx:1;
	uint8_t rx:1;
	uint8_t s:1;
} Tsch_Cell_Options;


typedef struct __packed {
	uint16_t slot_offset;
	uint8_t  channel_offset;
	uint8_t  flags;
} Tsch_Cell;


typedef struct __packed {
	uint8_t version:4;
	uint8_t type:2;
	uint8_t code;
	uint8_t sfid;
	uint8_t seqnum;
} Tsch_Header;


/* ADD Request
 * 		                     1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |     SeqNum    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|           Metadata            |  CellOptions  |   NumCells    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		| CellList ...
 * 		+-+-+-+-+-+-+-+-+-
 */
typedef struct __packed {
	uint8_t  version:4;
	uint8_t  type:2;
	uint8_t  code;
	uint8_t  sfid;
	uint8_t  seqnum;
	uint16_t metadata;
	uint8_t  cell_options;
	uint8_t  num_cells;
	Tsch_Cell cell_list[0];
} Tsch_Add_Request;


/* Add Response and Confirmation
 * 		                     1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |     SeqNum    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		| CellList ...
 * 		+-+-+-+-+-+-+-+-+-
 */
typedef struct __packed {
	uint8_t version:4;
	uint8_t type:2;
	uint8_t code;
	uint8_t sfid;
	uint8_t seqnum;
	Tsch_Cell cell_list[0];
} Tsch_Add_Response;

typedef Tsch_Add_Response Tsch_Add_Confirm;


/* Delete Request
 * 		                     1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |    SeqNum     |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|           Metadata            |  CellOptions  |   NumCells    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		| CellList ...
 * 		+-+-+-+-+-+-+-+-+-
 */
typedef struct __packed {
	uint8_t  version:4;
	uint8_t  type:2;
	uint8_t  code;
	uint8_t  sfid;
	uint8_t  seqnum;
	uint16_t metadata;
	uint8_t  cell_options;
	uint8_t  num_cells;
	Tsch_Cell cell_list[0];
} Tsch_Delete_Request;


/* Delete Response and Confirmation
 * 		                     1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |     SeqNum    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		| CellList ...
 * 		+-+-+-+-+-+-+-+-+-
 */
typedef struct __packed {
	uint8_t version:4;
	uint8_t type:2;
	uint8_t code;
	uint8_t sfid;
	uint8_t seqnum;
	Tsch_Cell cell_list[0];
} Tsch_Delete_Response;

typedef Tsch_Delete_Response Tsch_Delete_Confirm;


/* Relocate Request
 * 		                     1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |     SeqNum    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|           Metadata            |  CellOptions  |   NumCells    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		| Relocation CellList          ...
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 * 		| Candidate CellList           ...
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 */
typedef struct __packed {
	uint8_t  version:4;
	uint8_t  type:2;
	uint8_t  code;
	uint8_t  sfid;
	uint8_t  seqnum;
	uint16_t metadata;
	uint8_t  cell_options;
	uint8_t  num_cells;
	Tsch_Cell cell_list[0];
} Tsch_Relocate_Request;


/* Relocate Response and Confirmation
 * 		                     1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |     SeqNum    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		| CellList ...
 * 		+-+-+-+-+-+-+-+-+-
 */
typedef struct __packed {
	uint8_t version:4;
	uint8_t type:2;
	uint8_t code;
	uint8_t sfid;
	uint8_t seqnum;
	Tsch_Cell cell_list[0];
} Tsch_Relocate_Response;

typedef Tsch_Relocate_Response Tsch_Relocate_Confirm;


/* Count Request
 * 		                     1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |     SeqNum    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|           Metadata            |  CellOptions  |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct __packed {
	uint8_t  version:4;
	uint8_t  type:2;
	uint8_t  code;
	uint8_t  sfid;
	uint8_t  seqnum;
	uint16_t metadata;
	uint8_t  cell_options;
} Tsch_Count_Request;


/* Count Response
 * 		                      1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |     SeqNum    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|           NumCells            |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct __packed {
	uint8_t  version:4;
	uint8_t  type:2;
	uint8_t  code;
	uint8_t  sfid;
	uint8_t  seqnum;
	uint16_t num_cells;
} Tsch_Count_Response;


/* List Request
 * 		                     1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |     SeqNum    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|           Metadata            |  CellOptions  |   Reserved    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|           Offset              |          MaxNumCells          |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct __packed {
	uint8_t  version:4;
	uint8_t  type:2;
	uint8_t  code;
	uint8_t  sfid;
	uint8_t  seqnum;
	uint16_t metadata;
	uint8_t  cell_options;
	uint8_t  reserved;
	uint16_t offset;
	uint16_t max_num_cells;
} Tsch_List_Request;


/* List Response
 * 		                     1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |     SeqNum    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		| CellList ...
 * 		+-+-+-+-+-+-+-+-+-
 */
typedef struct __packed {
	uint8_t  version:4;
	uint8_t  type:2;
	uint8_t  code;
	uint8_t  sfid;
	uint8_t  seqnum;
	Tsch_Cell cell_list[0];
} Tsch_List_Response;


/* Clear Request
 * 		                     1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |     SeqNum    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|           Metadata            |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct __packed {
	uint8_t  version:4;
	uint8_t  type:2;
	uint8_t  code;
	uint8_t  sfid;
	uint8_t  seqnum;
	uint16_t metadata;
} Tsch_Clear_Request;


/* Clear Response
 * 		                     1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |     SeqNum    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct __packed {
	uint8_t  version:4;
	uint8_t  type:2;
	uint8_t  code;
	uint8_t  sfid;
	uint8_t  seqnum;
} Tsch_Clear_Response;


/* Signal Request
 * 		                     1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |     SeqNum    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|           Metadata            |  payload ...
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct __packed {
	uint8_t  version:4;
	uint8_t  type:2;
	uint8_t  code;
	uint8_t  sfid;
	uint8_t  seqnum;
	uint16_t metadata;
	uint8_t  payload[0];
} Tsch_Signal_Request;


/* Signal Response
 * 		                     1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |     SeqNum    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		| payload ...
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct __packed {
	uint8_t  version:4;
	uint8_t  type:2;
	uint8_t  code;
	uint8_t  sfid;
	uint8_t  seqnum;
} Tsch_Signal_Response;


/* Custom: Connect Request
 * 		                     1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |     SeqNum    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|           Metadata            |  Reserved     |   NumCells    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct __packed {
	uint8_t  version:4;
	uint8_t  type:2;
	uint8_t  code;
	uint8_t  sfid;
	uint8_t  seqnum;
	uint16_t metadata;
	uint8_t  _r;
	uint8_t  num_cells;
} Tsch_Connect_Request;


/* Custom: Connect Confirm
 * 		                     1                   2                   3
 * 		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|Version| T | R |     Code      |     SFID      |     SeqNum    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		| Hyper Coordinate Radius                                       |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		| Hyper Coordinate Theta                                        |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		|           Metadata            |  CellOptions  |   NumCells    |
 * 		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 		| CellList ...
 * 		+-+-+-+-+-+-+-+-+-
 */
typedef struct __packed {
	uint8_t   version:4;
	uint8_t   type:2;
	uint8_t   code;
	uint8_t   sfid;
	uint8_t   seqnum;
	float     radius;
	float     theta;
	float     open;
	float     close;
	uint16_t  metadata;
	uint8_t   cell_options;
	uint8_t   num_cells;
	Tsch_Cell cell_list[0];
} Tsch_Connect_Confirm;


// typedef struct {
// 	uint64_t timeout;	/* ASN this procedure times out */
// 	HyperNbr* nbr;
// 	uint8_t code;
// 	uint8_t state;		/* REQUEST, RESPONSE, CONFIRMATION, STOPPED */
// 	uint8_t seqnum;
// 	// void* frame;
// 	Ieee154_IE ie;		/* Store the next tx ie */

// 	// uint8_t numcells[2];
// 	// union {
// 	// 	Tsch_Header            tsch_header;
// 	// 	Tsch_Add_Request       tsch_add_request;
// 	// 	Tsch_Add_Response      tsch_add_response;
// 	// 	Tsch_Add_Confirm       tsch_add_confirm;
// 	// 	Tsch_Delete_Request    tsch_delete_request;
// 	// 	Tsch_Delete_Response   tsch_delete_response;
// 	// 	Tsch_Delete_Confirm    tsch_delete_confirm;
// 	// 	Tsch_Relocate_Request  tsch_relocate_request;
// 	// 	Tsch_Relocate_Response tsch_relocate_response;
// 	// 	Tsch_Relocate_Confirm  tsch_relocate_confirm;
// 	// 	Tsch_Count_Request     tsch_count_request;
// 	// 	Tsch_Count_Response    tsch_count_response;
// 	// 	Tsch_List_Request      tsch_list_request;
// 	// 	Tsch_List_Response     tsch_list_response;
// 	// 	Tsch_Clear_Request     tsch_clear_request;
// 	// 	Tsch_Clear_Response    tsch_clear_response;
// 	// 	Tsch_Signal_Request    tsch_signal_request;
// 	// 	Tsch_Signal_Response   tsch_signal_response;
// 	// };
// 	// Tsch_Cell cells[2][8];
// } Tsch_Procedure;


// typedef struct {
// 	sys_dnode_t node;
// 	uint8_t addr[8];
// 	int64_t last_seen;	/* Timestamp this neighbor was last seen */
// 	float   radius;
// 	float   theta;
// 	uint8_t height;
// 	bool    connected;
// 	Tsch_Procedure proc;
// } Tsch_Neighbor;


// typedef struct __packed {
// 	float   radius;
// 	float   theta;
// 	uint8_t height;
// 	uint8_t connected;
// 	uint8_t max_connected;
// } Tsch_Hyperbeacon;


typedef struct {
	struct net_if_api iface_api;
} tsch_api;


typedef enum {
	TSCH_CELL_IDLE_STATE,
	TSCH_CELL_ADV_STATE,
	TSCH_CELL_TX_STATE,
	// TSCH_CELL_FLOOD_STATE,
	TSCH_CELL_RX_STATE,
	TSCH_CELL_COOL_OFF_STATE,
} Tsch_Shared_Cell_State;


// typedef struct {
// 	float    open;
// 	float    close;
// 	uint8_t  is_root;
// 	uint8_t  shared_cell_state;
// 	uint8_t  dsn;
// 	uint8_t  ebsn;
// 	atomic_t connected;
// 	int      max_connected;
// 	// sys_dlist_t neighbors;
// 	sys_dlist_t procedures;

// 	/* Bayesian Broadcast */
// 	float v;

// 	/* Addresses */
// 	uint8_t addr[8];
// 	uint8_t bcast[8];

// 	/* Callbacks */
// 	/* TODO: when to call these? Add To / Remove? */
// 	// void (*on_connect_cb)(void);
// 	// void (*on_disconnect_cb)(void);
// 	bool (*on_scan_cb)(HyperNbr*, Ieee154_Frame*);
// 	// void (*on_desync)(void);
// } Tsch;


typedef struct {
	uint8_t dsn;
	uint8_t ebsn;
	uint8_t state;
	uint8_t next_state;
	uint8_t shared_cell_state;

	uint8_t addr[8];
	uint8_t bcast[8];
	// Backoff backoff;
	Bayesian bayes_bcast;
	bool (*on_scan_cb)(Ieee154_Frame*);
	struct k_mutex state_mutex;
	struct net_mgmt_event_callback prefix_cb;
	struct k_work_delayable timeout_work;
	struct k_work_delayable ra_work;

	/* For mesh root, do nothing. For nodes: restart every time sync occurs. If timeout, then sync
	 * has been lost: disconnect from the mesh. */
	struct k_work_delayable sync_lost_work;
} Tsch;


/* Public Functions ------------------------------------------------------------------------------ */
void* tsch_bcast_addr    (void);

void  tsch_init          (void);
void  tsch_enable        (void);
void  tsch_create_network(void);
void  tsch_power_down    (void);
void  tsch_power_up      (void);
void  tsch_start_scan    (bool (*on_scan_cb)(Ieee154_Frame*));
void  tsch_stop_scan     (void);
// void tsch_sync          (Ieee154_Frame*);
void  tsch_meas_dist     (const uint8_t*);

// void tsch_notify_on_connect     (void (*on_connect)(void));
// void tsch_notify_on_scan        (bool (*on_scan)(HyperNbr*, Ieee154_Frame*));
// void tsch_notify_on_scan_stopped(void (*on_scan_stopped)(void));
// void tsch_notify_on_disconnect  (void (*on_disconnect)(void));
// void tsch_notify_on_desync      (void (*on_desync)(void));


#ifdef __cplusplus
}
#endif

#endif // TSCH_H
/******************************************* END OF FILE *******************************************/
