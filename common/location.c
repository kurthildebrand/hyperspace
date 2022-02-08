/************************************************************************************************//**
 * @file		location.c
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
#include <nrfx/hal/nrf_timer.h>
#include <nrfx/hal/nrf_ppi.h>
#include <stdio.h>

#include "backoff.h"
#include "byteorder.h"
#include "config.h"
#include "dw1000.h"
#include "hyperspace.h"
#include "ieee_802_15_4.h"
#include "iir.h"
#include "location.h"
#include "matrix.h"
#include "nrf52.h"
#include "timeslot.h"
#include "tsch.h"

#include "logging/log.h"
// LOG_MODULE_REGISTER(location, LOG_LEVEL_ERR);
LOG_MODULE_REGISTER(location, LOG_LEVEL_INF);
// LOG_MODULE_REGISTER(location, LOG_LEVEL_DBG);


/* Private --------------------------------------------------------------------------------------- */
#define SPEED_OF_LIGHT			(299792458.0)	/* Speed of light in m/s */
// #define LOC_FIXED_THRESHOLD		(0.2f)
#define LOC_FIXED_THRESHOLD		(0.3f)
// #define LOC_FIXED_THRESHOLD		(0.5f)
// #define LOC_FIXED_THRESHOLD		(2.0f)
// #define LATTICE_R				(5.0f)
#define LATTICE_R				(2.5f)
// #define LATTICE_R				(3.0f)
#define NBR_DROP_MAX			(6)
// #define NBR_DROP_MAX			(4)

#define LOC_MEASURE_DIST_TIMEOUT	(30000)		/* Distance measurement timeout in ms */
#define LOC_UPDATE_TIMEOUT			(60000)
#define LOC_SEARCH_NBRHOOD_COUNT	(4*4)		/* Number of cells required to build a nbrhood */
#define LOC_KS						(1.0f)
// #define LOC_KG					(LOC_KS) / (5.0f*LATTICE_R - 1.0f)
#define LOC_KG						(0.2f)
// #define LOG_KG						(0.06f)
#define LOC_B						(2.0f)
#define LOC_M						(1.0f)
#define LOC_DT						(0.01f)


/* Private Types --------------------------------------------------------------------------------- */
typedef enum {
	BEACON_SILENT_STATE,
	BEACON_JOINING_STATE,
	BEACON_JOINED_STATE,
	BEACON_FORCED_STATE,
} BeaconState;

typedef enum {
	BEACON_START_EVENT,
	BEACON_STOP_EVENT,
	BEACON_SET_INDEX_EVENT,
	BEACON_FORCE_INDEX_EVENT,
	BEACON_JOINED_EVENT,
	BEACON_ALLOW_EVENT,
	BEACON_DISALLOW_EVENT,
} BeaconEvent;

typedef enum {
	LOCATION_INIT_STATE,
	LOCATION_SEARCHING_NBRHOOD_STATE,
	LOCATION_SEARCHING_STATE,
	LOCATION_MEASURE_DIST_STATE,
	LOCATION_JOINED_STATE,
} LocState;

typedef enum {
	LOCATION_START_EVENT,
	LOCATION_START_ROOT_EVENT,
	LOCATION_STOP_EVENT,
	LOCATION_JOINED_EVENT,
	LOCATION_LOST_EVENT,
	LOCATION_CELL_DONE_EVENT,
	LOCATION_CELL_SKIP_EVENT,
	LOCATION_TIMEOUT_EVENT,	/* Todo */
	LOCATION_DIST_MEASURED_EVENT,
} LocEvent;

typedef enum {
	LOCATION_UPDATED,               /* Location updated sucessfully.         */
	LOCATION_NOT_UPDATED,           /* Location not updated.                 */
	LOCATION_SKIP_NUM_BEACONS,      /* Skip: insufficient number of beacons. */
	LOCATION_SKIP_INACCURATE,       /* Skip: inaccurate location.            */
	LOCATION_SKIP_INVALID_DIR_SLOT, /* Skip: invalid dir or slot.            */
	LOCATION_SKIP_BINDEX_NOT_SET,   /* Skip: beacon index not set.           */
	LOCATION_TOA_NONFINITE,
	LOCATION_TDOA_NONFINITE,
} LocStatus;

typedef struct {
	BeaconState current_state;
	BeaconState next_state;
	unsigned    start_timer;   /* Todo: beacon timer based on distance */
	unsigned    index;
	uint32_t    tx_hist;       /* Bits [0-31] indicating this node's beacon tx history */
	bool        allow_beaconing;
	Backoff     backoff;
} Beacon;

// typedef struct {
// 	uint8_t    address[8];  /* Address of this neighbor */
// 	Vec3       loc;         /* The most recent estimate of this neighbor's 3D location */
// 	Hypercoord hcoord;      /* Hyperspace coordinate */
// 	uint32_t   nbrhood;     /* The beacon slots that this neighbor is listening to */
// 	uint8_t    class;
// } Neighbor;

typedef struct {
	uint8_t  dir;           /* This location update's direction                             */
	uint8_t  slot;          /* This location update's slot                                  */
	uint8_t  offset;        /* The offset for when this beacon should transmit              */
	uint8_t  conflicts;     /* Bits [0-5] indicating which beacons conflicted               */
	uint8_t  shouldtx;      /* Boolean indicating if this node should transmit in this slot */
	uint32_t new_nbrhood;   /* Bits [0-5] indicating which new_nbrs are valid               */
	uint32_t adj;           /* Bits [0-13] indicating which tstamps are valid               */
	Neighbor new_nbrs[6];   /* Neighbors received during this location update               */
	int32_t  tstamps[21];   /* Compact, upper-triangular, column-wise matrix of timestamps  */
} LocUpdate;

typedef struct {
	LocState  current_state;
	LocState  next_state;
	unsigned  search_count;
	struct k_work_delayable timeout_work;

	DW1000*   dw1000;
	uint8_t   address[8];
	Beacon    beacon;

	Vec3      vel;               /* Velocity of the location */
	iir       fx, fy, fz;        /* Filtered x, y, and z coordinates */
	float     r, t;              /* Hyperspace coordinates */

	uint32_t  all_nbrhood;       /* Bits [0-19] indicating which neighbors[20] are valid */
	uint32_t  local_nbrhood;     /* Bits [0-19] indicating which neighbors report local locations */
	Neighbor  neighbors[20];
	uint8_t   dropcount[20];
	LocUpdate update;            /* Temporary loc update */
} Location;

typedef struct {
	uint8_t addr[8];
	int32_t tstamp;
} LocTstamp;

typedef struct {
	uint8_t   version;
	uint8_t   class;
	uint8_t   dir_slot_offset;
	uint8_t   _reserved;
	float     x,y,z;
	float     r,t;
	uint32_t  nbrhood;
	LocTstamp nbrs[];
} LocBeacon;


/* Private Functions ----------------------------------------------------------------------------- */
static        void loc_set           (Location*, float, float, float);
static        bool loc_filter        (Location*, float, float, float);
static        void loc_clear         (Location*);
static        Vec3 loc_get           (Location*);
static inline bool loc_is_finite     (Location*);
static        void loc_handle_timeout(struct k_work*);

static void     loc_handle     (Location*, LocEvent, LocUpdate*);
static void     create_tx_frame(Location*, LocUpdate*, Ieee154_Frame*, uint8_t*, unsigned);
static uint64_t loc_start_tx   (Location*, LocUpdate*, unsigned, Ieee154_Frame*, uint64_t);
static void     loc_start_rx   (Location*, unsigned, uint64_t);
static uint64_t rx_start_read  (Location*, LocUpdate*, unsigned, uint32_t, Ieee154_Frame*, uint64_t);
static bool     rx_finish_read (Location*, LocUpdate*, unsigned, uint32_t, Ieee154_Frame*, float);
static uint32_t wait_for_trx   (Location*);

static void      prepare_tstamps         (LocUpdate*);
static void      update_neighbors        (Location*, LocUpdate*);
static LocStatus update_location         (Location*, LocUpdate*);
static void      update_beacon           (Location*, LocUpdate*);

static void      verify_distances        (LocUpdate*);
static uint32_t  local_outliers          (Location*, LocUpdate*);
static uint32_t  update_outliers         (Location*, LocUpdate*);
static uint32_t  update_mutual_nbrhood   (LocUpdate*);
static bool      update_is_coplanar      (LocUpdate*, uint32_t);
static bool      is_root                 (Location*);
static bool      nbrs_with_root          (Location*);
// static uint32_t  recenter_nbrhood        (Location*);
static void      optimize_beacons        (Location*);
static float     compare_distances       (Location*, unsigned, Vec3);
static void      join_beacons            (Location*);

static Vec3      quantize_to_grid        (Vec3);
static unsigned  index_from_point        (Vec3);
static unsigned  asn_to_slot             (TsSlotframe*, uint64_t);
static unsigned  asn_to_dir              (TsSlotframe*, uint64_t);

static unsigned  compact_triu_index      (unsigned, unsigned);
static LocStatus compute_springs_location(Location*, LocUpdate*);
static LocStatus compute_1line_location  (Location*, LocUpdate*);
static LocStatus compute_2circle_location(Location*, LocUpdate*);
static LocStatus compute_3sphere_location(Location*, LocUpdate*);
static LocStatus compute_toa_location    (Location*, LocUpdate*);
static LocStatus compute_tdoa_location   (Location*, LocUpdate*);

// static uint8_t   frame_get_version  (const Ieee154_Frame*);
// static uint8_t   frame_get_class    (const Ieee154_Frame*);
// static uint8_t   frame_get_dir      (const Ieee154_Frame*);
// static uint8_t   frame_get_slot     (const Ieee154_Frame*);
static void      frame_set_offset   (Ieee154_Frame*, uint8_t);
// static uint8_t   frame_get_offset   (const Ieee154_Frame*);
// static Vec3      frame_get_location (const Ieee154_Frame*);
// static uint32_t  frame_get_nbrhood  (const Ieee154_Frame*);
// static void      frame_push_addr    (Ieee154_Frame*, const uint8_t*);
// static void      frame_push_tstamp  (Ieee154_Frame*, int32_t);
static void      frame_update_addr  (Ieee154_Frame*, const void*, unsigned);
static void      frame_update_tstamp(Ieee154_Frame*, int32_t, unsigned);

static void      beacon_init       (Beacon*);
static unsigned  beacon_index      (Beacon*);
static unsigned  beacon_offset     (Beacon*, unsigned, unsigned);
static bool      beacon_enabled    (Beacon*);
static bool      beacon_get_tx_hist(const Beacon*, uint8_t, uint8_t);
static void      beacon_set_tx_hist(Beacon*, uint8_t, uint8_t, bool);
static bool      beacon_try        (Beacon*);
static bool      beacon_start      (Beacon*, unsigned, float);
static void      beacon_stop       (Beacon*);
static void      beacon_set_index  (Beacon*, unsigned);
static void      beacon_backoff    (Beacon*);
static void      beacon_success    (Beacon*);
static void      beacon_handle     (Beacon*, BeaconEvent, unsigned, float);
static unsigned  beacon_rand       (float);


/* Private Variables ----------------------------------------------------------------------------- */
/* 	Z-Level 0        Z-Level +1       Z-Level -1
 * 	4-----3-----2    4-----3-----2    4-----3-----2
 * 	|\    |    /|    |\   /|\   /|    |\   /|\   /|
 * 	| \   |   / |    | \ / | \ / |    | \ / | \ / |
 * 	|  \  |  /  |    | 10-----9  |    | 14-----13 |
 * 	|   \ | /   |    | /|\ | /|\ |    | /|\ | /|\ |
 * 	|    \|/    |    |/ | \|/ | \|    |/ | \|/ | \|
 * 	5-----0-----1    5--|--0--|--1    5--|--0--|--1
 * 	|    /|\    |    |\ | /|\ | /|    |\ | /|\ | /|
 * 	|   / | \   |    | \|/ | \|/ |    | \|/ | \|/ |
 * 	|  /  |  \  |    | 11-----12 |    | 15-----16 |
 * 	| /   |   \ |    | / \ | / \ |    | / \ | / \ |
 * 	|/    |    \|    |/   \|/   \|    |/   \|/   \|
 * 	6-----7-----8    6-----7-----8    6-----7-----8 */
static const Vec3 vectors[] = {
	{ 0.0f,       0.0f,       0.0f },	/* 0  */
	{ 1.0f,       0.0f,       0.0f },	/* 1  */
	{ 1.0f,       1.0f,       0.0f },	/* 2  */
	{ 0.0f,       1.0f,       0.0f },	/* 3  */
	{-1.0f,       1.0f,       0.0f },	/* 4  */
	{-1.0f,       0.0f,       0.0f },	/* 5  */
	{-1.0f,      -1.0f,       0.0f },	/* 6  */
	{ 0.0f,      -1.0f,       0.0f },	/* 7  */
	{ 1.0f,      -1.0f,       0.0f },	/* 8  */
	{ 1.0f/2.0f,  1.0f/2.0f,  1.0f },	/* 9  */
	{-1.0f/2.0f,  1.0f/2.0f,  1.0f },	/* 10 */
	{-1.0f/2.0f, -1.0f/2.0f,  1.0f },	/* 11 */
	{ 1.0f/2.0f, -1.0f/2.0f,  1.0f },	/* 12 */
	{ 1.0f/2.0f,  1.0f/2.0f, -1.0f },	/* 13 */
	{-1.0f/2.0f,  1.0f/2.0f, -1.0f },	/* 14 */
	{-1.0f/2.0f, -1.0f/2.0f, -1.0f },	/* 15 */
	{ 1.0f/2.0f, -1.0f/2.0f, -1.0f },	/* 16 */
	{ 0.0f,       0.0f,       0.0f },	/* 17 (not in neighborhood) */
};

static const uint8_t relpos[20][20] = {
	/* 0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15  16  17  18  19        |
	 * --------------------------------------------------------------------------------------+--- */
	{  0,  17, 17, 17, 1,  4,  11, 15, 6,  3,  12, 16, 7,  2,  14, 10, 8,  5,  13, 9,  }, /* | 0  */
	{  17, 0,  17, 17, 4,  1,  15, 11, 3,  6,  16, 12, 2,  7,  10, 14, 5,  8,  9,  13, }, /* | 1  */
	{  17, 17, 0,  17, 15, 11, 1,  4,  16, 12, 6,  3,  10, 14, 7,  2,  9,  13, 8,  5,  }, /* | 2  */
	{  17, 17, 17, 0,  11, 15, 4,  1,  12, 16, 3,  6,  14, 10, 2,  7,  13, 9,  5,  8,  }, /* | 3  */
	{  5,  8,  9,  13, 0,  17, 17, 17, 1,  4,  11, 15, 6,  3,  12, 16, 7,  2,  14, 10, }, /* | 4  */
	{  8,  5,  13, 9,  17, 0,  17, 17, 4,  1,  15, 11, 3,  6,  16, 12, 2,  7,  10, 14, }, /* | 5  */
	{  13, 9,  5,  8,  17, 17, 0,  17, 15, 11, 1,  4,  16, 12, 6,  3,  10, 14, 7,  2,  }, /* | 6  */
	{  9,  13, 8,  5,  17, 17, 17, 0,  11, 15, 4,  1,  12, 16, 3,  6,  14, 10, 2,  7,  }, /* | 7  */
	{  2,  7,  10, 14, 5,  8,  9,  13, 0,  17, 17, 17, 1,  4,  11, 15, 6,  3,  12, 16, }, /* | 8  */
	{  7,  2,  14, 10, 8,  5,  13, 9,  17, 0,  17, 17, 4,  1,  15, 11, 3,  6,  16, 12, }, /* | 9  */
	{  14, 10, 2,  7,  13, 9,  5,  8,  17, 17, 0,  17, 15, 11, 1,  4,  16, 12, 6,  3,  }, /* | 10 */
	{  10, 14, 7,  2,  9,  13, 8,  5,  17, 17, 17, 0,  11, 15, 4,  1,  12, 16, 3,  6,  }, /* | 11 */
	{  3,  6,  16, 12, 2,  7,  10, 14, 5,  8,  9,  13, 0,  17, 17, 17, 1,  4,  11, 15, }, /* | 12 */
	{  6,  3,  12, 16, 7,  2,  14, 10, 8,  5,  13, 9,  17, 0,  17, 17, 4,  1,  15, 11, }, /* | 13 */
	{  12, 16, 3,  6,  14, 10, 2,  7,  13, 9,  5,  8,  17, 17, 0,  17, 15, 11, 1,  4,  }, /* | 14 */
	{  16, 12, 6,  3,  10, 14, 7,  2,  9,  13, 8,  5,  17, 17, 17, 0,  11, 15, 4,  1,  }, /* | 15 */
	{  4,  1,  15, 11, 3,  6,  16, 12, 2,  7,  10, 14, 5,  8,  9,  13, 0,  17, 17, 17, }, /* | 16 */
	{  1,  4,  11, 15, 6,  3,  12, 16, 7,  2,  14, 10, 8,  5,  13, 9,  17, 0,  17, 17, }, /* | 17 */
	{  11, 15, 4,  1,  12, 16, 3,  6,  14, 10, 2,  7,  13, 9,  5,  8,  17, 17, 0,  17, }, /* | 18 */
	{  15, 11, 1,  4,  16, 12, 6,  3,  10, 14, 7,  2,  9,  13, 8,  5,  17, 17, 17, 0,  }, /* | 19 */
};

static const uint8_t beacon_order[8][4][6] = {	/* Indexed: beacon_order[dir][slot][offset] */
	/* 0   1   2   3   4   5     Beacon Offset */
	{{ 0,  4,  13, 9,  18, 19 },      /* NE, 0 */
	 { 1,  5,  12, 8,  19, 18 },      /* NE, 1 */
	 { 2,  6,  15, 11, 17, 16 },      /* NE, 2 */
	 { 3,  7,  14, 10, 16, 17 }},     /* NE, 3 */

	{{ 0,  9,  14, 18, 15, 19 },      /* N,  0 */
	 { 1,  8,  15, 19, 14, 18 },      /* N,  1 */
	 { 2,  11, 13, 17, 12, 16 },      /* N,  2 */
	 { 3,  10, 12, 16, 13, 17 }},     /* N,  3 */

	{{ 0,  9,  5,  17, 14, 15 },      /* NW, 0 */
	 { 1,  8,  4,  16, 15, 14 },      /* NW, 1 */
	 { 2,  11, 7,  19, 13, 12 },      /* NW, 2 */
	 { 3,  10, 6,  18, 12, 13 }},     /* NW, 3 */

	{{ 0,  17, 7,  14, 6,  15 },      /*  W, 0 */
	 { 1,  16, 6,  15, 7,  14 },      /*  W, 1 */
	 { 2,  19, 4,  13, 5,  12 },      /*  W, 2 */
	 { 3,  18, 5,  12, 4,  13 }},     /*  W, 3 */

	{{ 0,  17, 8,  12, 7,  6  },      /* SW, 0 */
	 { 1,  16, 9,  13, 6,  7  },      /* SW, 1 */
	 { 2,  19, 10, 14, 4,  5  },      /* SW, 2 */
	 { 3,  18, 11, 15, 5,  4  }},     /* SW, 3 */

	{{ 0,  12, 7,  11, 6,  10 },      /* S,  0 */
	 { 1,  13, 6,  10, 7,  11 },      /* S,  1 */
	 { 2,  14, 4,  8,  5,  9  },      /* S,  2 */
	 { 3,  15, 5,  9,  4,  8  }},     /* S,  3 */

	{{ 0,  12, 16, 4,  11, 10 },      /* SE, 0 */
	 { 1,  13, 17, 5,  10, 11 },      /* SE, 1 */
	 { 2,  14, 18, 6,  8,  9  },      /* SE, 2 */
	 { 3,  15, 19, 7,  9,  8  }},     /* SE, 3 */

	{{ 0,  4,  11, 18, 10, 19 },      /*  E, 0 */
	 { 1,  5,  10, 19, 11, 18 },      /*  E, 1 */
	 { 2,  6,  8,  17, 9,  16 },      /*  E, 2 */
	 { 3,  7,  9,  16, 8,  17 }},     /*  E, 3 */
};



Location location;

uint8_t loc_rx_frame_data[IEEE154_STD_PACKET_LENGTH];
uint8_t loc_tx_frame_data[IEEE154_STD_PACKET_LENGTH];
Ieee154_Frame loc_rx_frame;
Ieee154_Frame loc_tx_frame;


// uint8_t loc_rx_frames_data[7][IEEE154_STD_PACKET_LENGTH];
// Ieee154_Frame loc_rx_frames[7];


/* loc_print_tstamps ****************************************************************************//**
 * @brief		*/
static void loc_print_tstamps(const LocUpdate* update)
{
	unsigned i, j;

	for(i = 0; i < 6; i++)
	{
		printk("%02x%02x%02x%02x%02x%02x%02x%02x ",
			update->new_nbrs[i].address[0], update->new_nbrs[i].address[1],
			update->new_nbrs[i].address[2], update->new_nbrs[i].address[3],
			update->new_nbrs[i].address[4], update->new_nbrs[i].address[5],
			update->new_nbrs[i].address[6], update->new_nbrs[i].address[7]);

		printk("%*.s", i*6, " ");

		for(j = i+1; j <= 6; j++)
		{
			printk("%4d", update->tstamps[compact_triu_index(i, j)]);

			if(update->adj & (1 << compact_triu_index(i, j)))
			{
				printk("* ");
			}
			else
			{
				printk("  ");
			}
		}

		printk("\n");
	}

	printk("\nconflicts = ");

	for(i = 6; i-- > 0; )
	{
		if(update->conflicts & (1 << i))
		{
			printk("1");
		}
		else
		{
			printk("0");
		}
	}

	printk("\n");
}


/* loc_init *************************************************************************************//**
 * @brief		*/
void loc_init(DW1000* dw1000, const uint8_t* address)
{
	location.current_state = LOCATION_INIT_STATE;
	location.next_state    = LOCATION_INIT_STATE;
	location.search_count  = 0;
	location.dw1000        = dw1000;
	location.all_nbrhood   = 0;
	location.local_nbrhood = 0;

	memmove(location.address, address, 8);
	memset(location.neighbors, 0, sizeof(location.neighbors));
	memset(location.dropcount, 0, sizeof(location.dropcount));

	beacon_init(&location.beacon);
	iir_init(&location.fx, 0.965, NAN);
	iir_init(&location.fy, 0.965, NAN);
	iir_init(&location.fz, 0.965, NAN);

	k_work_init_delayable(&location.timeout_work, loc_handle_timeout);
}


/* loc_start ************************************************************************************//**
 * @brief		Start location services on this node. */
void loc_start(void)
{
	loc_handle(&location, LOCATION_START_EVENT, 0);
}


/* loc_start_root *******************************************************************************//**
 * @brief		Starts the node as the root node in the mesh. */
void loc_start_root(void)
{
	loc_handle(&location, LOCATION_START_ROOT_EVENT, 0);
}


/* loc_stop *************************************************************************************//**
 * @brief		Stops location services on this node. */
void loc_stop(void)
{
	loc_handle(&location, LOCATION_STOP_EVENT, 0);
}


/* loc_allow_beaoning ***************************************************************************//**
 * @brief		Setting allow = true allows this node to transmit beacons. Setting allow = false
 * 				prevents this node from transmitting beacons. */
void loc_allow_beaconing(bool allow)
{
	if(allow)
	{
		beacon_handle(&location.beacon, BEACON_ALLOW_EVENT, 0, 0);
	}
	else
	{
		beacon_handle(&location.beacon, BEACON_DISALLOW_EVENT, 0, 0);
	}
}


/* loc_force_index ******************************************************************************//**
 * @brief		Forces this node to take a specific index. This applies only temporarily until this
 * 				node updates its location, at which point this node's index will reflect its
 * 				location. This function is primarily useful for testing location handling. */
void loc_force_index(int index)
{
	beacon_handle(&location.beacon, BEACON_FORCE_INDEX_EVENT, index, 0);
}


/* loc_current *********************************************************************************//**
 * @brief		Returns this node's current location. */
Vec3 loc_current(void)
{
	return loc_get(&location);
}


/* loc_set_hypercoord ***************************************************************************//**
 * @brief		Updates this node's hyperspace coordinates. */
void loc_set_hypercoord(float r, float t)
{
	location.r = r;
	location.t = t;
}


/* loc_nbrs_size ********************************************************************************//**
 * @brief		Returns the size of the location neighbor table. */
unsigned loc_nbrs_size(void)
{
	return sizeof(location.neighbors) / sizeof(location.neighbors[0]);
}


/* loc_nbrs *************************************************************************************//**
 * @brief		Returns the i'th neighbor if the neighbor is in the local neighborhood. */
Neighbor* loc_nbrs(unsigned i)
{
	if(location.local_nbrhood & (1 << i))
	{
		return &location.neighbors[i];
	}
	else
	{
		return 0;
	}
}


/* loc_is_beacon ********************************************************************************//**
 * @brief		Returns true if this node transmits location beacons. */
bool loc_is_beacon(void)
{
	return beacon_enabled(&location.beacon);
}


/* loc_beacon_index *****************************************************************************//**
 * @brief		Returns this node's beacon index. */
unsigned loc_beacon_index(void)
{
	return beacon_index(&location.beacon);
}


/* loc_set **************************************************************************************//**
 * @brief		Unconditionally sets this node's current location. */
static void loc_set(Location* loc, float x, float y, float z)
{
	if(!isfinite(x) || !isfinite(y) || !isfinite(z))
	{
		LOG_WRN("nonfinite location ignored");
		return;
	}

	iir_set_value(&loc->fx, x);
	iir_set_value(&loc->fy, y);
	iir_set_value(&loc->fz, z);

	hyperspace_update(iir_value(&loc->fx), iir_value(&loc->fy), iir_value(&loc->fz));

	// Vec3 x0 = loc_get(loc);
	// Vec3 g  = quantize_to_grid(x0);
	// printf("loc = %f,%f,%f,%f,%f,%f\r\n", x0.x, x0.y, x0.z, g.x, g.y, g.z);
}


/* loc_filter ***********************************************************************************//**
 * @brief		Filters and updates this node's current location. */
static bool loc_filter(Location* loc, float x, float y, float z)
{
	if(!isfinite(x) || !isfinite(y) || !isfinite(z))
	{
		LOG_WRN("nonfinite location ignored");
		return false;
	}

	if(!loc_is_finite(loc))
	{
		iir_set_value(&loc->fx, x);
		iir_set_value(&loc->fy, y);
		iir_set_value(&loc->fz, z);
		loc->vel = make_vec3(0, 0, 0);
	}
	else
	{
		iir_filter(&loc->fx, x);
		iir_filter(&loc->fy, y);
		iir_filter(&loc->fz, z);
	}

	hyperspace_update(iir_value(&loc->fx), iir_value(&loc->fy), iir_value(&loc->fz));

	// // Vec3 x0 = loc_get(loc);
	// // Vec3 g  = quantize_to_grid(x0);
	// // printf("loc = %f,%f,%f,%f,%f,%f\r\n", x0.x, x0.y, x0.z, g.x, g.y, g.z);

	// printf("%f,%f,%f,%f,%f,%f\r",
	// 	iir_value(&loc->fx), iir_value(&loc->fy), iir_value(&loc->fz), x, y, z);

	return true;
}


/* loc_clear ************************************************************************************//**
 * @brief		Deinitializes this node's location. */
static void loc_clear(Location* loc)
{
	iir_set_value(&loc->fx, NAN);
	iir_set_value(&loc->fy, NAN);
	iir_set_value(&loc->fz, NAN);

	hyperspace_update(NAN, NAN, NAN);
}


/* loc_get **************************************************************************************//**
 * @brief		Returns this node's current location. */
static Vec3 loc_get(Location* loc)
{
	return make_vec3(iir_value(&loc->fx), iir_value(&loc->fy), iir_value(&loc->fz));
}


/* loc_is_finite ********************************************************************************//**
 * @brief		Returns true if this node's current location (x,y,z) are finite values (not NAN or
 * 				INF). */
static inline bool loc_is_finite(Location* loc)
{
	return isfinite(iir_value(&loc->fx)) &&
	       isfinite(iir_value(&loc->fy)) &&
	       isfinite(iir_value(&loc->fz));
}


/* loc_handle_timeout ***************************************************************************//**
 * @brief		Work itme which raises LOCATION_TIMEOUT_EVENT. */
static void loc_handle_timeout(struct k_work* work)
{
	LOG_DBG("timeout event");
	loc_handle(&location, LOCATION_TIMEOUT_EVENT, 0);
}


/* loc_handle ***********************************************************************************//**
 * @brief		Handles events for the location state machine. */
static void loc_handle(Location* loc, LocEvent e, LocUpdate* update)
{
	switch(loc->current_state)
	{
		/* LOCATION_INIT_STATE: The initial location state.
		 * 	Location updates slotframe does not exist which means loc_slots are not called.
		 * 	The location is cleared with loc_clear.
		 * 	Neighbors are cleared.
		 * 	Location beaconing is stopped and the beacon index is set to 20.
		 *
		 * LOCATION_START_EVENT transitions to the LOCATION_SEARCHING_STATE.
		 *
		 * LOCATION_START_ROOT_EVENT transitions to the LOCATION_JOINED_STATE. */
		case LOCATION_INIT_STATE:
			if(e == LOCATION_START_EVENT)
			{
				LOG_INF("LOCATION_INIT_STATE. Got LOCATION_START_EVENT -> LOCATION_SEARCHING_STATE");

				loc->next_state = LOCATION_SEARCHING_NBRHOOD_STATE;

				TsSlotframe* sf = ts_slotframe_add(1, TSCH_DEFAULT_NUM_SLOTS);
				ts_slot_add(sf, 0, 0 * (sf->numslots / 4) + 2, loc_slot);
				ts_slot_add(sf, 0, 1 * (sf->numslots / 4) + 2, loc_slot);
				ts_slot_add(sf, 0, 2 * (sf->numslots / 4) + 2, loc_slot);
				ts_slot_add(sf, 0, 3 * (sf->numslots / 4) + 2, loc_slot);
			}
			else if(e == LOCATION_START_ROOT_EVENT)
			{
				LOG_INF("LOCATION_INIT_STATE. Got LOCATION_START_ROOT_EVENT -> LOCATION_JOINED_STATE");

				loc->next_state = LOCATION_JOINED_STATE;
				loc_set(loc, 0, 0, 0);
				beacon_start(&loc->beacon, 0, 0);

				TsSlotframe* sf = ts_slotframe_add(1, TSCH_DEFAULT_NUM_SLOTS);
				ts_slot_add(sf, 0, 0 * (sf->numslots / 4) + 2, loc_slot);
				ts_slot_add(sf, 0, 1 * (sf->numslots / 4) + 2, loc_slot);
				ts_slot_add(sf, 0, 2 * (sf->numslots / 4) + 2, loc_slot);
				ts_slot_add(sf, 0, 3 * (sf->numslots / 4) + 2, loc_slot);
			}
			break;

		/* LOCATION_SEARCHING_NBRHOOD_STATE: This state is used to build up this node's local beacon
		 * neighborhood. RX errors could occur when beacons transmit or a beacon could be backing
		 * off. Therefore, this node needs to listen for a bit to get an idea of the local
		 * neighborhood before becoming a beacon itself.
		 *
		 * The state machine transitions to the LOCATION_SEARCHING_STATE after
		 * LOC_SEARCH_NBRHOOD_COUNT cells. This node's beaconing state is not updated in this case.
		 * However, if this node can determine it's location, the this node transitions to the
		 * LOCATION_JOINED_STATE and updates its beacon state. */
		case LOCATION_SEARCHING_NBRHOOD_STATE:
			if(e == LOCATION_START_ROOT_EVENT)
			{
				LOG_INF("LOCATION_SEARCHING_NBRHOOD_STATE. Got LOCATION_START_ROOT_EVENT -> LOCATION_JOINED_STATE");

				loc->next_state = LOCATION_JOINED_STATE;
				loc_set(loc, 0, 0, 0);
				beacon_start(&loc->beacon, 0, 0);
			}
			else if(e == LOCATION_STOP_EVENT)
			{
				LOG_INF("LOCATION_SEARCHING_NBRHOOD_STATE. Got LOCATION_STOP_EVENT -> LOCATION_INIT_STATE");
				loc->next_state = LOCATION_INIT_STATE;
			}
			else if(e == LOCATION_JOINED_EVENT)
			{
				LOG_INF("LOCATION_SEARCHING_NBRHOOD_STATE. Got LOCATION_JOINED_EVENT -> LOCATION_JOINED_STATE");
				loc->next_state = LOCATION_JOINED_STATE;
			}
			else if(e == LOCATION_CELL_DONE_EVENT)
			{
				LOG_DBG("LOCATION_SEARCHING_NBRHOOD_STATE. Got LOCATION_CELL_DONE_EVENT");

				LocStatus ret;
				      prepare_tstamps (update);
				      update_neighbors(loc, update);
				      LOG_DBG("neighbors = %X/%X", loc->local_nbrhood, loc->all_nbrhood);
				ret = update_location (loc, update);

				LOG_DBG("loc status = %d. adj = %x", ret, update->adj);

				/* Building up a local neighborhood of beacons is really only useful to prevent
				 * join_beacons from picking the wrong beacon when presented with a partial
				 * neighborhood. join_beacons is only called if this node can't compute is location.
				 * Therefore, if the location CAN be computed, then transition directly to the
				 * JOINED state. */
				if(ret == LOCATION_UPDATED)
				{
					LOG_INF("LOCATION_SEARCHING_NBRHOOD_STATE. LOCATION_UPDATED -> LOCATION_JOINED_STATE");
					update_beacon(loc, update);
					loc->next_state = LOCATION_JOINED_STATE;
				}
				else if(++loc->search_count >= LOC_SEARCH_NBRHOOD_COUNT)
				{
					LOG_INF("LOCATION_SEARCHING_NBRHOOD_STATE. LOC_SEARCH_NBRHOOD_COUNT Reached -> LOCATION_SEARCHING_STATE");
					loc->next_state = LOCATION_SEARCHING_STATE;
				}
			}
			else if(e == LOCATION_CELL_SKIP_EVENT)
			{
				if(++loc->search_count >= LOC_SEARCH_NBRHOOD_COUNT)
				{
					loc->next_state = LOCATION_SEARCHING_STATE;
				}
			}
			break;

		/* LOCATION_SEARCHING_STATE: This state is used to continue trying to determine this node's
		 * location. It is expected that this node knows it's local beacon neighborhood state as
		 * computed by LOCATION_SEARCHING_NBRHOOD_STATE.
		 *
		 * If this node can compute it's location, then it transitions to the LOCATION_JOINED_STATE.
		 * However, it may be the case that this node can't determine it's location due to Dilution
		 * of Precision, which is signaled by compute_tdoa_location returning a status of
		 * LOCATION_SKIP_INACCURATE. It may be possible to still determine this node's location by
		 * explicitly measuring the distance between this node and the prime node.
		 *
		 * If this node receives an inaccurate TDOA location, then it transitions to the
		 * LOCATION_MEASURE_DIST_STATE and starts the distance measurement process. The inaccurate
		 * update timestamps are temporarily stored to be used when the explicit distance is finally
		 * measured. */
		case LOCATION_SEARCHING_STATE:
			if(e == LOCATION_START_ROOT_EVENT)
			{
				LOG_INF("LOCATION_SEARCHING_STATE. Got LOCATION_START_ROOT_EVENT -> LOCATION_JOINED_STATE");

				loc->next_state = LOCATION_JOINED_STATE;
				loc_set(loc, 0, 0, 0);
				beacon_start(&loc->beacon, 0, 0);
			}
			else if(e == LOCATION_STOP_EVENT)
			{
				LOG_INF("LOCATION_SEARCHING_STATE. Got LOCATION_STOP_EVENT -> LOCATION_INIT_STATE");
				loc->next_state = LOCATION_INIT_STATE;
			}
			else if(e == LOCATION_JOINED_EVENT)
			{
				LOG_INF("LOCATION_SEARCHING_STATE. Got LOCATION_JOINED_EVENT -> LOCATION_JOINED_STATE");
				loc->next_state = LOCATION_JOINED_STATE;
			}
			else if(e == LOCATION_CELL_DONE_EVENT)
			{
				LOG_DBG("LOCATION_SEARCHING_STATE. Got LOCATION_CELL_DONE_EVENT.");

				LocStatus ret;
				      prepare_tstamps (update);
				      update_neighbors(loc, update);
				      LOG_DBG("neighbors = %X/%X", loc->local_nbrhood, loc->all_nbrhood);
				ret = update_location (loc, update);
				      update_beacon   (loc, update);

				LOG_DBG("beacon index = %d", beacon_index(&loc->beacon));

				// LOG_DBG("loc status = %d. adj = %x", ret, update->adj);

				/* Location joined if the location can be computed. */
				if(ret == LOCATION_UPDATED)
				{
					LOG_INF("LOCATION_SEARCHING_STATE. Got LOCATION_JOINED_EVENT -> LOCATION_JOINED_STATE");
					loc->next_state = LOCATION_JOINED_STATE;
				}
				/* TDOA may be inaccurate due to Dilution of Precision. However, TDOA
				 * measurements can be turned into TOA measurements just by measuring the
				 * distance to the prime node.
				 *
				 * Note: update->adj & (1 << compact_triu_index(0, 6) does not get set. That is, adj
				 * corresponding to the prime beacon in column 6 does not get set as it is not used
				 * for TDOA (it is not a valid timestamp). Therefore, check update->new_nbrhood & 1
				 * for a valid prime beacon.
				 *
				 * Also, TOA needs at least 4 beacons: the prime beacon and 3 others. Count
				 * update->adj column 6 for the rest of the beacons as adj keeps track of whether or
				 * not the other beacons share the same prime beacon with this node. */
				else if((ret == LOCATION_SKIP_INACCURATE || ret == LOCATION_TDOA_NONFINITE) &&
				        (update->new_nbrhood & 0x1) && (3 <= calc_popcount_u32(
				         update->adj & (0x3F << compact_triu_index(0, 6)))))
				{
					LOG_INF("LOCATION_SEARCHING_STATE. INACCURATE LOCATION -> LOCATION_MEASURE_DIST_STATE");
					loc->next_state = LOCATION_MEASURE_DIST_STATE;
				}
			}
			/* Todo: LOCATION_TIMEOUT_EVENT */
			break;

		/* LOCATION_MEASURE_DIST_STATE: This node is waiting for the distance to be measured between
		 * this node and the prime node. The distance measurement occurs in the common shared TSCH
		 * cell and not using loc_slot.
		 *
		 * The state machine transitions to the LOCATION_JOINED_STATE once this node's location is
		 * calculated. If no distance is measured after LOC_MEASURE_DIST_TIMEOUT ms, then the state
		 * machine transitions back to LOCATION_SEARCHING_STATE.
		 *
		 * Note: distance needs to be measured between this node and the prime beacon which
		 * transmitted during the location update. Location can still be computed if the prime beacon
		 * changes between starting and receiving a distance measurement. */
		case LOCATION_MEASURE_DIST_STATE:
			if(e == LOCATION_START_ROOT_EVENT)
			{
				LOG_INF("LOCATION_MEASURE_DIST_STATE. Got LOCATION_START_ROOT_EVENT -> LOCATION_JOINED_STATE");

				loc->next_state = LOCATION_JOINED_STATE;
				loc_set(loc, 0, 0, 0);
				beacon_start(&loc->beacon, 0, 0);
			}
			else if(e == LOCATION_STOP_EVENT)
			{
				LOG_INF("LOCATION_MEASURE_DIST_STATE. Got LOCATION_STOP_EVENT -> LOCATION_INIT_STATE");
				loc->next_state = LOCATION_INIT_STATE;
			}
			else if(e == LOCATION_JOINED_EVENT)
			{
				LOG_INF("LOCATION_MEASURE_DIST_STATE. Got LOCATION_JOINED_EVENT -> LOCATION_JOINED_STATE");
				loc->next_state = LOCATION_JOINED_STATE;
			}
			else if(e == LOCATION_CELL_DONE_EVENT)
			{
				/* Todo: store new LocUpdate timestamps? */
				LOG_DBG("LOCATION_MEASURE_DIST_STATE. Got LOCATION_CELL_DONE_EVENT");

				LocStatus ret;
				      prepare_tstamps (update);
				      update_neighbors(loc, update);
				      LOG_DBG("neighbors = %X/%X", loc->local_nbrhood, loc->all_nbrhood);
				ret = update_location (loc, update);
				      update_beacon   (loc, update);

				LOG_DBG("beacon index = %d", beacon_index(&loc->beacon));

				if(ret == LOCATION_UPDATED)
				{
					LOG_INF("LOCATION_MEASURE_DIST_STATE. Got LOCATION_JOINED_EVENT -> LOCATION_JOINED_STATE");
					loc->next_state = LOCATION_JOINED_STATE;
				}
			}
			else if(e == LOCATION_DIST_MEASURED_EVENT)
			{
				LocStatus ret;

				/* Exclude beacons that have moved */
				verify_distances(update);

				uint32_t mutual = update_mutual_nbrhood(update);

				/* Try and compute this node's location with the updated distances using the measured
				 * distance to the prime beacon. Don't use update_location because it checks if
				 * update->shouldtx is set in order to call compute_toa_location or
				 * compute_3sphere_location. */
				if(!update_is_coplanar(update, mutual))
				{
					ret = compute_toa_location(loc, update);
				}
				else
				{
					ret = compute_3sphere_location(loc, update);
				}

				update_beacon(loc, update);

				LOG_DBG("beacon index = %d", beacon_index(&loc->beacon));

				if(ret == LOCATION_UPDATED)
				{
					LOG_INF("LOCATION_MEASURE_DIST_STATE. Got LOCATION_JOINED_EVENT -> LOCATION_JOINED_STATE");
					loc->next_state = LOCATION_JOINED_STATE;
				}
				else
				{
					LOG_INF("LOCATION_MEASURE_DIST_STATE -> LOCATION_SEARCHING_STATE");
					loc->next_state = LOCATION_SEARCHING_STATE;
				}
			}
			else if(e == LOCATION_TIMEOUT_EVENT)
			{
				LOG_INF("LOCATION_MEASURE_DIST_STATE. Got LOCATION_TIMEOUT_EVENT -> LOCATION_SEARCHING_STATE");
				loc->next_state = LOCATION_SEARCHING_STATE;
			}
			break;

		/* LOCATION_JOINED_STATE: Location updates are nominal.
		 *
		 * If this node loses its location and/or its neighbors, then the state machine transitions
		 * back to the LOCATION_SEARCHING_NBRHOOD_STATE. */
		case LOCATION_JOINED_STATE:
			if(e == LOCATION_START_ROOT_EVENT)
			{
				LOG_INF("LOCATION_JOINED_STATE. Got LOCATION_START_ROOT_EVENT -> LOCATION_JOINED_STATE");
				loc->next_state = LOCATION_JOINED_STATE;
				loc_set(loc, 0, 0, 0);
				beacon_start(&loc->beacon, 0, 0);
			}
			else if(e == LOCATION_STOP_EVENT)
			{
				LOG_INF("LOCATION_JOINED_STATE. Got LOCATION_STOP_EVENT -> LOCATION_INIT_STATE");
				loc->next_state = LOCATION_INIT_STATE;
			}
			else if(e == LOCATION_TIMEOUT_EVENT || e == LOCATION_LOST_EVENT)
			{
				if(e == LOCATION_LOST_EVENT)
				{
					LOG_INF("LOCATION_JOINED_STATE. Got LOCATION_LOST_EVENT -> LOCATION_SEARCHING_NBRHOOD_STATE");
				}
				else if(e == LOCATION_TIMEOUT_EVENT)
				{
					LOG_INF("LOCATION_JOINED_STATE. Got LOCATION_TIMEOUT_EVENT -> LOCATION_SEARCHING_NBRHOOD_STATE");
				}

				loc->next_state = LOCATION_SEARCHING_NBRHOOD_STATE;

				/* Reset location state */
				beacon_stop(&loc->beacon);
				loc->all_nbrhood   = 0;
				loc->local_nbrhood = 0;
				loc_clear(loc);
			}
			else if(e == LOCATION_CELL_DONE_EVENT)
			{
				LOG_DBG("LOCATION_JOINED_STATE. Got LOCATION_CELL_DONE_EVENT");

				LocStatus ret;
				      prepare_tstamps (update);
				    //   print_tstamps   (update);
				      update_neighbors(loc, update);
				      LOG_DBG("neighbors = %X/%X", loc->local_nbrhood, loc->all_nbrhood);
				ret = update_location (loc, update);
				      update_beacon   (loc, update);

				LOG_DBG("beacon index = %d", beacon_index(&loc->beacon));

				/* Reset timeout if location updated successfully */
				if(ret == LOCATION_UPDATED)
				{
					k_work_reschedule(&loc->timeout_work, K_MSEC(LOC_UPDATE_TIMEOUT));
				}
				/* Detect if location is lost. */
				else if(!loc_is_finite(loc) && calc_popcount_u32(loc->all_nbrhood) < 4)
				{
					loc_handle(loc, LOCATION_LOST_EVENT, update);
				}
			}
			break;

		default: break;
	}

	if(loc->current_state == loc->next_state)
	{
		return;
	}

	loc->current_state = loc->next_state;

	/* State entry logic */
	switch(loc->next_state)
	{
		case LOCATION_INIT_STATE: {
			/* Remove the 4 location timeslot cells */
			TsSlotframe* sf = ts_slotframe_find(1);
			ts_slot_remove(ts_slot_find(sf, 0 * (sf->numslots / 4) + 2));
			ts_slot_remove(ts_slot_find(sf, 1 * (sf->numslots / 4) + 2));
			ts_slot_remove(ts_slot_find(sf, 2 * (sf->numslots / 4) + 2));
			ts_slot_remove(ts_slot_find(sf, 3 * (sf->numslots / 4) + 2));

			/* Reset location state */
			beacon_stop(&loc->beacon);
			beacon_set_index(&loc->beacon, 20);
			loc_clear(loc);
			loc->all_nbrhood   = 0;
			loc->local_nbrhood = 0;
			break;
		}

		case LOCATION_SEARCHING_NBRHOOD_STATE:
			loc->search_count = 0;
			break;

		case LOCATION_MEASURE_DIST_STATE:
			memmove(&loc->update, update, sizeof(LocUpdate));
			tsch_meas_dist(loc->update.new_nbrs[0].address);
			k_work_reschedule(&loc->timeout_work, K_MSEC(LOC_MEASURE_DIST_TIMEOUT));
			break;

		case LOCATION_JOINED_STATE:
			k_work_reschedule(&loc->timeout_work, K_MSEC(LOC_UPDATE_TIMEOUT));
			break;

		default: break;
	}
}



// void loc_start_tx(void)
// {
// 	TsSlotframe* sf = ts_slotframe_find(0);
// 	ts_slot_add(sf, 0, 1, loc_tx_slot);
// }


// void loc_start_rx(void)
// {
// 	TsSlotframe* sf = ts_slotframe_find(0);
// 	ts_slot_add(sf, 0, 1, loc_rx_slot);
// }


// void loc_tx_slot(TsSlot* ts)
// {
// 	dw1000_lock(location.dw1000);

// 	Ieee154_Frame* frame = 0;
// 	uint64_t t0 = dw1000_read_sys_timestamp(location.dw1000);
// 	uint64_t t1 = calc_addmod_u64(t0, dw1000_us_to_ticks(LOC_RX_START_TIME), DW1000_TSTAMP_PERIOD);
// 	int64_t  trx_offset = 0;
// 	uint64_t rxtstamp   = 0;
// 	uint32_t status     = 0;

// 	LocUpdate update;
// 	update.dir         = 0;
// 	update.slot        = 0;
// 	update.offset      = 0;
// 	update.conflicts   = 0;
// 	update.new_nbrhood = 0;
// 	update.adj         = 0;

// 	dw1000_set_rx_timeout(location.dw1000, LOC_RX_TIMEOUT);
// 	nrf_ppi_group_enable(NRF_PPI, NRF_PPI_CHANNEL_GROUP2);

// 	/* Transmit initial beacon */
// 	t1 = calc_addmod_u64(t0, dw1000_us_to_ticks(LOC_TX_START_TIME), DW1000_TSTAMP_PERIOD);
// 	trx_offset  = dw1000_set_trx_tstamp(location.dw1000, t1);
// 	trx_offset += dw1000_ant_delay(location.dw1000);
// 	t1          = calc_addmod_u64(t1, trx_offset, DW1000_TSTAMP_PERIOD);

// 	frame = create_prime_packet(&location, &update);
// 	dw1000_write_tx_fctrl(location.dw1000, 0, ieee154_length(frame) + 2);
// 	dw1000_start_delayed_tx(location.dw1000, false);
// 	dw1000_write_tx(location.dw1000, ieee154_ptr_start(frame), 0, ieee154_length(frame));

// 	memmove(update.new_nbrs[0].address, location.address, 8);
// 	update.new_nbrs[0].loc = location.loc;
// 	update.new_nbrs[0].nbrhood = location.nbrhood;
// 	update.new_nbrhood |= (1 << 0);

// 	dw1000_wait_for_irq(location.dw1000, -1);
// 	dw1000_handle_irq(location.dw1000);

// 	/* Receive response */
// 	dw1000_set_trx_tstamp(location.dw1000,
// 		t1 + dw1000_us_to_ticks(LOC_GRID_LENGTH * 1 - LOC_RX_GUARD_TIME));

// 	dw1000_start_delayed_rx(location.dw1000);

// 	frame  = &loc_rx_frame;
// 	status = 0;
// 	uint32_t flen = 0;

// 	while(0 == (status & (
// 		DW1000_SYS_STATUS_RXFCG   | DW1000_SYS_STATUS_RXRFTO | DW1000_SYS_STATUS_RXPTO  |
// 		DW1000_SYS_STATUS_RXPHE   | DW1000_SYS_STATUS_RXFCE  | DW1000_SYS_STATUS_RXRFSL |
// 		DW1000_SYS_STATUS_RXSFDTO | DW1000_SYS_STATUS_AFFREJ | DW1000_SYS_STATUS_LDEERR)))
// 	{
// 		dw1000_wait_for_irq(location.dw1000, -1);

// 		status = dw1000_handle_irq(location.dw1000);

// 		/* Wait for RX SFD which marks the start of the frame and generates the RMARMKER
// 		 * timestamp on the DW1000. */
// 		// if(status & DW1000_SYS_STATUS_RXSFDD)
// 		// {
// 		// 	rxtstamp = dw1000_read_rx_timestamp(location.dw1000);
// 		// 	rxtstamp = calc_submod_u64(rxtstamp,
// 		// 		dw1000_ant_delay(location.dw1000), DW1000_TSTAMP_PERIOD);
// 		// 	flen     = dw1000_read_rx_finfo(location.dw1000) & DW1000_RX_FINFO_RXFLEN_MASK;
// 		// }
// 	}

// 	/* Check if any rx timeout or rx error bits are set. Also check if frame is not good. */
// 	if(0 != (status & (
// 	   DW1000_SYS_STATUS_RXRFTO  | DW1000_SYS_STATUS_RXPTO  |
// 	   DW1000_SYS_STATUS_RXPHE   | DW1000_SYS_STATUS_RXFCE  | DW1000_SYS_STATUS_RXRFSL |
// 	   DW1000_SYS_STATUS_RXSFDTO | DW1000_SYS_STATUS_AFFREJ | DW1000_SYS_STATUS_LDEERR)) ||
// 	   0 == (status & DW1000_SYS_STATUS_RXFCG))
// 	{
// 		goto exit;
// 	}

// 	/* Read the rx packet */
// 	rxtstamp = dw1000_read_rx_timestamp(location.dw1000);
// 	rxtstamp = calc_submod_u64(rxtstamp,
// 		dw1000_ant_delay(location.dw1000), DW1000_TSTAMP_PERIOD);
// 	flen     = dw1000_read_rx_finfo(location.dw1000) & DW1000_RX_FINFO_RXFLEN_MASK;

// 	dw1000_read_rx(location.dw1000, loc_rx_frame_data, 0, flen);
// 	ieee154_receive(frame, loc_rx_frame_data, flen, sizeof(loc_rx_frame_data));

// 	/* Find the frame's payload */
// 	Ieee154_IE ie = ieee154_ie_first(frame);
// 	while(!ieee154_ie_is_last(&ie)) {
// 		ieee154_ie_next(&ie);
// 	}

// 	/* Start reading at the frame's payload */
// 	buffer_pop(&frame->buffer, ieee154_ie_start(&ie));

// 	/* Verify version */
// 	uint8_t version = buffer_pop_u8(&frame->buffer);
// 	uint8_t class   = buffer_pop_u8(&frame->buffer);
// 	uint8_t dso     = buffer_pop_u8(&frame->buffer);
// 	                  buffer_pop_u8(&frame->buffer);	/* Pull _reserved */

// 	unsigned offset = (dso >> 0) & 0x7;		/* bits [0-2]: offset */
// 	unsigned slot   = (dso >> 3) & 0x3;		/* bits [3-4]: slot   */
// 	unsigned dir    = (dso >> 5) & 0x7;		/* bits [5-7]: dir    */

// 	int32_t tstamp = rxtstamp - t1 - (offset * dw1000_us_to_ticks(LOC_GRID_LENGTH));

// 	// int32_t tstamp = calc_submod_u64(
// 	// 	rxtstamp, t1 + offset * dw1000_us_to_ticks(LOC_GRID_LENGTH), DW1000_TSTAMP_PERIOD);

// 	receive_nonprime_beacon(frame, &location, &update, offset);

// 	exit:
// 		dw1000_unlock(location.dw1000);
// 		tstamp_hist_idx++;
// }


// void loc_rx_slot(TsSlot* tx)
// {
// 	dw1000_lock(location.dw1000);

// 	Ieee154_Frame* frame = 0;
// 	uint64_t t0 = dw1000_read_sys_timestamp(location.dw1000);
// 	uint64_t t1 = calc_addmod_u64(t0, dw1000_us_to_ticks(LOC_RX_START_TIME), DW1000_TSTAMP_PERIOD);
// 	int64_t  trx_offset = 0;
// 	uint64_t rxtstamp   = 0;
// 	uint32_t status     = 0;

// 	LocUpdate update;
// 	update.dir         = 0;
// 	update.slot        = 0;
// 	update.offset      = 1;
// 	update.conflicts   = 0;
// 	update.new_nbrhood = 0;
// 	update.adj         = 0;

// 	dw1000_set_rx_timeout(location.dw1000, LOC_RX_TIMEOUT);
// 	nrf_ppi_group_enable(NRF_PPI, NRF_PPI_CHANNEL_GROUP2);

// 	/* Receive initial beacon */
// 	dw1000_set_trx_tstamp(location.dw1000, t1);
// 	dw1000_start_delayed_rx(location.dw1000);

// 	frame = &loc_rx_frame;
// 	uint32_t flen = 0;

// 	while(0 == (status & (
// 		DW1000_SYS_STATUS_RXFCG   | DW1000_SYS_STATUS_RXRFTO | DW1000_SYS_STATUS_RXPTO  |
// 		DW1000_SYS_STATUS_RXPHE   | DW1000_SYS_STATUS_RXFCE  | DW1000_SYS_STATUS_RXRFSL |
// 		DW1000_SYS_STATUS_RXSFDTO | DW1000_SYS_STATUS_AFFREJ | DW1000_SYS_STATUS_LDEERR)))
// 	{
// 		dw1000_wait_for_irq(location.dw1000, -1);

// 		status = dw1000_handle_irq(location.dw1000);

// 		/* Wait for RX SFD which marks the start of the frame and generates the RMARMKER
// 		 * timestamp on the DW1000. */
// 		// if(status & DW1000_SYS_STATUS_RXSFDD)
// 		// {

// 		// }
// 	}

// 	rxtstamp = dw1000_read_rx_timestamp(location.dw1000);
// 	rxtstamp = calc_submod_u64(rxtstamp,
// 		dw1000_ant_delay(location.dw1000), DW1000_TSTAMP_PERIOD);
// 	flen     = dw1000_read_rx_finfo(location.dw1000) & DW1000_RX_FINFO_RXFLEN_MASK;

// 	/* Read the rx packet */
// 	dw1000_read_rx(location.dw1000, loc_rx_frame_data, 0, flen);
// 	ieee154_receive(frame, loc_rx_frame_data, flen, sizeof(loc_rx_frame_data));

// 	/* Find the frame's payload */
// 	Ieee154_IE ie = ieee154_ie_first(frame);
// 	while(!ieee154_ie_is_last(&ie)) {
// 		ieee154_ie_next(&ie);
// 	}

// 	/* Start reading at the frame's payload */
// 	buffer_pop(&frame->buffer, ieee154_ie_start(&ie));

// 	/* Verify version */
// 	uint8_t version = buffer_pop_u8(&frame->buffer);
// 	uint8_t class   = buffer_pop_u8(&frame->buffer);
// 	uint8_t dso     = buffer_pop_u8(&frame->buffer);
// 	                  buffer_pop_u8(&frame->buffer);	/* Pull _reserved */

// 	unsigned offset = (dso >> 0) & 0x7;		/* bits [0-2]: offset */
// 	unsigned slot   = (dso >> 3) & 0x3;		/* bits [3-4]: slot   */
// 	unsigned dir    = (dso >> 5) & 0x7;		/* bits [5-7]: dir    */

// 	if(version != 22 || dir != update.dir || slot != update.slot || offset > 6)
// 	{
// 		goto exit;
// 	}

// 	int32_t tstamp = rxtstamp - t1 - (offset * dw1000_us_to_ticks(LOC_GRID_LENGTH));

// 	t1 = calc_submod_u64(rxtstamp, dw1000_us_to_ticks(offset * LOC_GRID_LENGTH), DW1000_TSTAMP_PERIOD);

// 	receive_prime_beacon(frame, &update);

// 	trx_offset  = dw1000_set_trx_tstamp(location.dw1000, t1 + dw1000_us_to_ticks(LOC_GRID_LENGTH));
// 	trx_offset += dw1000_ant_delay(location.dw1000);

// 	update.tstamps[compact_triu_index(0, 6)] = trx_offset;

// 	frame = create_nonprime_packet(&location, &update);
// 	dw1000_write_tx_fctrl(location.dw1000, 0, ieee154_length(frame)+2);
// 	dw1000_start_delayed_tx(location.dw1000, false);
// 	dw1000_write_tx(location.dw1000, ieee154_ptr_start(frame), 0, ieee154_length(frame));

// 	dw1000_wait_for_irq(location.dw1000, -1);
// 	dw1000_handle_irq(location.dw1000);

// 	exit:
// 		dw1000_unlock(location.dw1000);
// 		tstamp_hist_idx++;
// }


/* loc_dist_measured ****************************************************************************//**
 * @brief		Callback to report the distance measured between this node and the destination node.
 * 				This is used when the distance is explicitly measured instead of measured during a
 * 				location update. That is, this node initiates a distance measurement which causes
 * 				this node to send a frame to the destination node using a shared TSCH cell. This
 * 				function is called when the frame is ACK'd. */
void loc_dist_measured(const uint8_t* dest, uint32_t d0j)
{
	unsigned i;

	if(location.current_state != LOCATION_MEASURE_DIST_STATE)
	{
		return;
	}

	if((location.update.new_nbrhood & 1) == 0)
	{
		return;
	}

	if(memcmp(dest, &location.update.new_nbrs[0].address, 8) != 0)
	{
		return;
	}

	/* Store the distance measured between the prime beacon (0) and this node (6). */
	location.update.offset = 6;
	location.update.tstamps[compact_triu_index(0, 6)] = d0j;
	location.update.adj |= (1 << compact_triu_index(0, 6));

	LOG_INF("distance = %u. adj = %x", d0j, location.update.adj);

	/* Column 6 stores pseudoranges to this node. Convert pseudoranges to distances. The algorithm is
	 * as follows:
	 *
	 * 		Pseudorange = pij = tij - t0i       - d0i (d0j unknown)
	 * 		Distance    = dij = tij - t0i + d0j - d0i
	 *
	 * 	where
	 *
	 * 		i is an existing beacon.
	 * 		j is this node.
	 *
	 * Since this node was trying to perform TDOA, column 6 stores pseudoranges. To turn pseudoranges
	 * to distances, add d0j (distance from the prime beacon to this node) to every timestamp. */
	for(i = 1; i < 6; i++)
	{
		unsigned ij = compact_triu_index(i, 6);

		if(location.update.adj & (1 << ij))
		{
			location.update.tstamps[ij] += d0j;
		}
	}

	loc_handle(&location, LOCATION_DIST_MEASURED_EVENT, &location.update);
}


/* loc_slot *************************************************************************************//**
 * @brief		*/
void loc_slot(TsSlot* ts)
{
	dw1000_lock(location.dw1000);

	LocUpdate update;

	uint64_t asn  = ts_current_asn();
	update.dir    = asn_to_dir    (ts->slotframe, asn);
	update.slot   = asn_to_slot   (ts->slotframe, asn);
	update.offset = beacon_offset(&location.beacon, update.dir, update.slot);

	LOG_DBG("start. asn = %d. dir = %d, slot = %d, offset = %d",
		(uint32_t)asn, update.dir, update.slot, update.offset);

	if(location.current_state == LOCATION_JOINED_STATE && update.offset >= 6)
	{
		goto skip;
	}

	uint64_t t0 = dw1000_read_sys_tstamp(location.dw1000);
	uint64_t t1 = calc_addmod_u64(t0, dw1000_us_to_ticks(LOC_TX_START_TIME), DW1000_TSTAMP_PERIOD);
	uint64_t rxtstamp  = 0;
	unsigned j         = 0;
	bool     synced    = false;
	float    rx_clk_offset;

	/* Note: this node could still be searching but have set the beacon index which means
	 * update.offset could be >= 6 here. Todo: is this a state machine bug? */
	update.conflicts   = 0;
	update.shouldtx    = update.offset < 6 && beacon_try(&location.beacon);
	update.new_nbrhood = 0;
	update.adj         = 0;

	memset(update.new_nbrs, 0, sizeof(update.new_nbrs));
	memset(update.tstamps,  0, sizeof(update.tstamps));

	/* Initialize this node's beacon packet */
	create_tx_frame(&location, &update, &loc_tx_frame, loc_tx_frame_data, sizeof(loc_tx_frame_data));

	/* Initialize the rx timeout */
	dw1000_set_rx_timeout(location.dw1000, LOC_RX_TIMEOUT);

	/* Capture the local time of the first packet reception */
	nrf_ppi_group_enable(NRF_PPI, NRF_PPI_CHANNEL_GROUP2);

	/* Read status to eventually synchronize double buffers */
	uint32_t status = dw1000_read_status(location.dw1000);

	/* Start slot 0 */
	if(0 == update.offset && update.shouldtx)
	{
		t1 = loc_start_tx(&location, &update, 0, &loc_tx_frame, t1);
		synced = true;
	}
	else
	{
		loc_start_rx(&location, 0, t1);
	}

	/* Synchronize double rx buffers */
	dw1000_sync_drxb(location.dw1000, status);

	for(j = 1; j < 7; j++)
	{
		/* Wait for the previous slot to finish. Note: wait_for_trx() takes
		 * ~38 us when tx or rx completes without error.
		 * ~98 us when there is an rx timeout or error.  */
		status = wait_for_trx(&location);

		// if(!synced && (status & (
		// 	DW1000_SYS_STATUS_RXRFTO  |	/* Receive Frame Wait Timeout */
		// 	DW1000_SYS_STATUS_RXPTO   |	/* Preamble Detection Timeout */
		// 	DW1000_SYS_STATUS_RXPHE   |	/* Receiver PHY Header Error */
		// 	DW1000_SYS_STATUS_RXFCE   |	/* Receiver FCS Error */
		// 	DW1000_SYS_STATUS_RXRFSL  |	/* Receiver Reed Solomon Frame Sync Loss */
		// 	DW1000_SYS_STATUS_RXSFDTO |	/* Receive SFD Timeout */
		// 	DW1000_SYS_STATUS_AFFREJ  |	/* Automatic Frame Filtering Rejection */
		// 	DW1000_SYS_STATUS_LDEERR)))	/* Leading edge detection processing error */
		// {
		// 	goto exit;
		// }

		/* Read the packet received in the previous slot */
		rxtstamp = rx_start_read(&location, &update, j, status, &loc_tx_frame, t1);

		rx_clk_offset = dw1000_rx_clk_offset(location.dw1000);

		/* Sync to the previous slot if successfully received a packet and not already synced */
		if(!synced && rxtstamp != -1ull)
		{
			/* Sync to the last packet, which occurred at j-1 */
			t1 = calc_submod_u64(
				rxtstamp, dw1000_us_to_ticks((j-1) * LOC_GRID_LENGTH), DW1000_TSTAMP_PERIOD);
			synced = true;
		}

		/* Start the current slot */
		if(update.shouldtx && ((j < 6 && j == update.offset) || (j == 6 && update.offset == 0)))
		{
			loc_start_tx(&location, &update, j, &loc_tx_frame, t1);
		}
		else
		{
			loc_start_rx(&location, j, t1);
		}

		/* Finish reading the packet received in the previous slot */
		rx_finish_read(&location, &update, j, status, &loc_rx_frame, rx_clk_offset);
		// rx_finish_read(&location, &update, j, status, &loc_rx_frames[j-1], rx_clk_offset);

		dw1000_sync_drxb(location.dw1000, status);
	}

	/* Finish slot 6 */
	status = wait_for_trx(&location);
	rx_start_read(&location, &update, j, status, &loc_tx_frame, t1);
	rx_clk_offset = dw1000_rx_clk_offset(location.dw1000);
	rx_finish_read(&location, &update, j, status, &loc_rx_frame, rx_clk_offset);
	// rx_finish_read(&location, &update, j, status, &loc_rx_frames[j-1], rx_clk_offset);

	exit:
		if(!synced)
		{
			LOG_DBG("done (not synced)");
		}
		else
		{
			LOG_DBG("done");
		}

		dw1000_sync_drxb(location.dw1000, status);
		dw1000_unlock   (location.dw1000);
		beacon_set_tx_hist(&location.beacon, update.slot, update.dir, update.shouldtx);
		loc_handle(&location, LOCATION_CELL_DONE_EVENT, &update);
		return;

	skip:
		LOG_DBG("skip");
		dw1000_unlock(location.dw1000);
		beacon_set_tx_hist(&location.beacon, update.slot, update.dir, false);
		loc_handle(&location, LOCATION_CELL_SKIP_EVENT, &update);
		return;
}


/* create_tx_frame ******************************************************************************//**
 * @brief		Initializes a location update frame. The intent is that neighbor's addresses and
 * 				timestamps will be progressively appended to this frame during the location
 * 				update. */
static void create_tx_frame(
	Location*      loc,
	LocUpdate*     update,
	Ieee154_Frame* tx,
	uint8_t*       ptr,
	unsigned       len)
{
	unsigned i;

	uint8_t dir_slot_offset = 0;
	dir_slot_offset |= ((update->offset & 0x7) << 0);	/* bits [0-2]: offset */
	dir_slot_offset |= ((update->slot   & 0x3) << 3);	/* bits [3-4]: slot   */
	dir_slot_offset |= ((update->dir    & 0x7) << 5);	/* bits [5-7]: dir    */

	ieee154_data_frame_init(tx, ptr, len);
	ieee154_set_addr(tx, 0, 0, 0, 0, loc->address, 8);

	Vec3 current = loc_get(loc);

	buffer_push_u8 (&tx->buffer, 22);                            /* uint8_t  version         */
	buffer_push_u8 (&tx->buffer, 128);                           /* uint8_t  class           */
	buffer_push_u8 (&tx->buffer, dir_slot_offset);               /* uint8_t  dir_slot_offset */
	buffer_push_u8 (&tx->buffer, 0);                             /* uint8_t  _reserved       */
	buffer_push_mem(&tx->buffer, &current.x, sizeof(current.x)); /* float    x               */
	buffer_push_mem(&tx->buffer, &current.y, sizeof(current.y)); /* float    y               */
	buffer_push_mem(&tx->buffer, &current.z, sizeof(current.z)); /* float    z               */
	buffer_push_mem(&tx->buffer, &loc->r,    sizeof(loc->r));    /* float    r               */
	buffer_push_mem(&tx->buffer, &loc->t,    sizeof(loc->t));    /* float    t               */
	buffer_push_u32(&tx->buffer, le_u32(loc->all_nbrhood));      /* uint32_t nbrhood         */

	/* Append the expected beacons */
	for(i = 0; i < 6; i++)
	{
		unsigned idx = beacon_order[update->dir][update->slot][i];

		if(loc->all_nbrhood & (1 << idx))
		{
			buffer_push_mem(&tx->buffer, &loc->neighbors[idx].address, 8);
		}
		else
		{
			buffer_push_u64(&tx->buffer, le_u64(0));
		}

		if(loc->dropcount[idx] == 0)
		{
			buffer_push_u32(&tx->buffer, le_u32(0));
		}
		else
		{
			buffer_push_u32(&tx->buffer, le_u32(-1u));
		}
	}
}


/* loc_start_tx *********************************************************************************//**
 * @brief		Starts the UWB radio to transmit the next frame.
 * @param[in]	loc: location state.
 * @param[in]	update: location update state.
 * @param[in]	j: the current location grid offset.
 * @param[in]	tx: the frame to transmit.
 * @param[in]	t1: timestamp of when frame 0 was transmitted/received in this location update. */
static uint64_t loc_start_tx(
	Location*      loc,
	LocUpdate*     update,
	unsigned       j,
	Ieee154_Frame* tx,
	uint64_t       t1)
{
	int32_t tstamp;
	tstamp  = dw1000_us_to_ticks   (LOC_GRID_LENGTH * j);
	tstamp += dw1000_set_trx_tstamp(loc->dw1000, t1 + tstamp);
	tstamp += dw1000_ant_delay     (loc->dw1000);

	/* Update the tx beacon's offset to the current grid offset */
	frame_set_offset(tx, j);

	/* The tx beacon is almost ready. Update t[0,j] in the beacon. */
	if(0 < update->offset && update->offset < 6)
	{
		/* Update t[0,j] */
		update->tstamps[compact_triu_index(0, j)] = tstamp;
		update->tstamps[compact_triu_index(0, 6)] = tstamp;

		frame_update_tstamp(tx, tstamp, 0);
	}

	/* Do not send the expected neighbors with the first prime beacon. */
	unsigned len = ieee154_length(tx) - (5 * sizeof(LocTstamp)) * (j == 0);

	/* Send the packet to the dw1000 */
	dw1000_write_tx_fctrl  (loc->dw1000, 0, len + 2);
	dw1000_start_delayed_tx(loc->dw1000, false);
	dw1000_write_tx        (loc->dw1000, ieee154_ptr_start(tx), 0, len);

	/* Set new_nbrs[j] with this node's address */
	if(j < 6)
	{
		memmove(update->new_nbrs[j].address, loc->address, 8);
		update->new_nbrs[j].loc     = loc_get(loc);
		update->new_nbrs[j].nbrhood = loc->all_nbrhood;
	}

	update->new_nbrhood |= (1 << j);

	return calc_addmod_u64(t1, tstamp, DW1000_TSTAMP_PERIOD);
}


/* loc_start_rx *********************************************************************************//**
 * @brief		Starts the UWB radio to receive the next frame.
 * @param[in]	loc: location state.
 * @param[in]	j: the current location grid offset.
 * @param[in]	t1: timestamp of when frame 0 was transmitted/received in this location update. */
static void loc_start_rx(Location* loc, unsigned j, uint64_t t1)
{
	t1 += dw1000_us_to_ticks(LOC_GRID_LENGTH * j);
	t1 -= dw1000_us_to_ticks(LOC_RX_GUARD_TIME);

	dw1000_set_trx_tstamp  (loc->dw1000, t1);	/* 20 us */
	dw1000_start_delayed_rx(loc->dw1000);		/* 32 us (success) */
}


/* rx_start_read ********************************************************************************//**
 * @brief		Reads the rx timestamp and the addressing fields for the received packet. */
static uint64_t rx_start_read(
	Location*      loc,
	LocUpdate*     update,
	unsigned       j,
	uint32_t       status,
	Ieee154_Frame* tx,
	uint64_t       t1)
{
	uint64_t rxtstamp = -1ull;
	uint64_t tjk;

	/* Reading the previous slot's packet */
	j = j-1;

	/* No rx packet if this node transmitted last slot */
	if(update->shouldtx && ((j < 6 && j == update->offset) || (j == 6 && update->offset == 0)))
	{
		goto skip;
	}

	/* Check for reception / preamble detect timeouts */
	if(status & (DW1000_SYS_STATUS_RXRFTO | DW1000_SYS_STATUS_RXPTO))
	{
		goto error;
	}

	/* Check for RX errors */
	if(status & (DW1000_SYS_STATUS_RXPHE   | DW1000_SYS_STATUS_RXFCE  | DW1000_SYS_STATUS_RXRFSL |
 	             DW1000_SYS_STATUS_RXSFDTO | DW1000_SYS_STATUS_AFFREJ | DW1000_SYS_STATUS_LDEERR))
	{
		goto error;
	}

	/* Check that the frame was received */
	if(0 == (status & DW1000_SYS_STATUS_RXFCG))
	{
		goto error;
	}

	rxtstamp = dw1000_read_rx_tstamp(loc->dw1000);
	rxtstamp = calc_submod_u64(rxtstamp, dw1000_ant_delay(loc->dw1000), DW1000_TSTAMP_PERIOD);
	tjk      = calc_submod_u64(rxtstamp, t1, DW1000_TSTAMP_PERIOD);

	/* Store tjk where j is the beacon that was just received and k is this node */
	update->tstamps[compact_triu_index(j, 6)] = tjk;

	/* Only interested in the source address. Absolute maximum number of bytes that need to be read
	 * to guarantee that the source address has been read is:
	 *
	 * 		fctrl:       2 bytes
	 * 		seq num:     1 byte
	 * 		dest pan id: 2 bytes
	 * 		dest addr:   8 bytes
	 * 		src pan id:  2 bytes
	 * 		src addr:    8 bytes
	 * 		total:       23 bytes
	 */
	Ieee154_Frame rx;
	ieee154_frame_init(&rx, loc_rx_frame_data, 0, sizeof(loc_rx_frame_data));
	dw1000_read_rx(loc->dw1000, ieee154_set_length(&rx, 23), 0, 23);
	ieee154_parse(&rx);

	/* Progressively build the tx beacon */
	void* src = ieee154_src_addr(&rx);
	j -= (update->offset == 0 && update->shouldtx);
	frame_update_addr  (tx, src, j);
	frame_update_tstamp(tx, tjk, j);

	skip:
		return rxtstamp;

	/* Progressively build the tx beacon. There was an error and the frame was not received. add both
	 * a blank address and timestamp. Note: new_nbrs[j] should not have been set. */
	error:
		j -= (update->offset == 0 && update->shouldtx);
		frame_update_addr  (tx, update->new_nbrs[j].address, j);
		frame_update_tstamp(tx, -1, j);
		return -1ull;
}


/* rx_finish_read *******************************************************************************//**
 * @brief		Finishes reading the rx packet and adds the packet details to the location update. */
static bool rx_finish_read(
	Location*      loc,
	LocUpdate*     update,
	unsigned       j,
	uint32_t       status,
	Ieee154_Frame* rx,
	float          rx_clk_offset)
{
	unsigned i;

	/* Reading the previous slot's packet */
	j = j-1;

	/* No rx packet if this node transmitted last slot */
	if(update->shouldtx && ((j < 6 && j == update->offset) || (j == 6 && update->offset == 0)))
	{
		goto skip;
	}

	/* Check for reception / preamble detect timeouts */
	if(status & (DW1000_SYS_STATUS_RXRFTO | DW1000_SYS_STATUS_RXPTO))
	{
		goto error;
	}

	/* Check for RX errors */
	if(status & (DW1000_SYS_STATUS_RXPHE   | DW1000_SYS_STATUS_RXFCE  | DW1000_SYS_STATUS_RXRFSL |
 	             DW1000_SYS_STATUS_RXSFDTO | DW1000_SYS_STATUS_AFFREJ | DW1000_SYS_STATUS_LDEERR))
	{
		goto error;
	}

	/* Check that the frame was received */
	if(0 == (status & DW1000_SYS_STATUS_RXFCG))
	{
		goto error;
	}

	/* Finish reading the previous beacon */
	uint32_t flen = dw1000_read_rx_finfo(loc->dw1000) & DW1000_RX_FINFO_RXFLEN_MASK;

	/* Already read the first 23 bytes from rx_start_read() */
	if(flen > 23)
	{
		dw1000_read_rx(loc->dw1000, loc_rx_frame_data + 23, 23, flen - 23);
		// dw1000_read_rx(loc->dw1000, &loc_rx_frames_data[j][23], 23, flen - 23);
	}

	ieee154_frame_init(rx, loc_rx_frame_data, flen, sizeof(loc_rx_frame_data));
	ieee154_parse(rx);

	/* Find the frame's payload */
	Ieee154_IE ie = ieee154_ie_first(rx);
	while(!ieee154_ie_is_last(&ie)) {
		ieee154_ie_next(&ie);
	}

	/* Start reading at the frame's payload */
	/* Todo: rename to ieee154_reset_payload_buffer? */
	Buffer* rx_buffer = ieee154_reset_buffer(rx);

	/* Verify the packet version */
	uint8_t version = le_get_u8(buffer_pop_u8(rx_buffer));

	if(version != 22)
	{
		/* Received unrecognized version */
		goto error;
	}

	/* Verify dir, slot and offset */
	uint8_t class = le_get_u8(buffer_pop_u8(rx_buffer));
	uint8_t dso   = le_get_u8(buffer_pop_u8(rx_buffer));
	                buffer_pop_u8(rx_buffer);	/* _reserved */

	unsigned offset = (dso >> 0) & 0x7;		/* bits [0-2]: offset */
	unsigned slot   = (dso >> 3) & 0x3;		/* bits [3-4]: slot   */
	unsigned dir    = (dso >> 5) & 0x7;		/* bits [5-7]: dir    */

	if(dir != update->dir)
	{
		/* Received invalid dir */
		goto error;
	}

	if(slot != update->slot)
	{
		/* Received invalid slot */
		goto error;
	}

	if(offset != j)
	{
		/* Received invalid offset */
		goto error;
	}

	/* Mark the neighbor as valid */
	update->new_nbrhood |= (1 << offset);

	/* At this point, the packet has been verified. Start parsing. */
	void*    src        = ieee154_src_addr(rx);
	bool     same_prime = false;
	bool     final      = offset >= 6;
	unsigned end        = calc_min_uint(offset, 6);

	if(final)
	{
		offset     = 0;
		same_prime = memcmp(update->new_nbrs[0].address, src, 8) == 0;
	}

	/* Copy the beacon's address */
	memmove(update->new_nbrs[offset].address, src, 8);

	/* Copy the beacon's reported location */
	memmove(&update->new_nbrs[offset].loc.x, buffer_pop(rx_buffer, 4), 4);
	memmove(&update->new_nbrs[offset].loc.y, buffer_pop(rx_buffer, 4), 4);
	memmove(&update->new_nbrs[offset].loc.z, buffer_pop(rx_buffer, 4), 4);

	/* Copy the beacon's reported hyperspace coordinates */
	memmove(&update->new_nbrs[offset].r, buffer_pop(rx_buffer, 4), 4);
	memmove(&update->new_nbrs[offset].t, buffer_pop(rx_buffer, 4), 4);

	/* Copy the beacon's reported neighborhood */
	update->new_nbrs[offset].nbrhood = le_get_u32(buffer_pop_u32(rx_buffer));
	update->new_nbrs[offset].class   = class;

	/* Read the nonprime beacon's prime address and transmit duration */
	if(0 < offset && !final)
	{
		uint8_t* ptr = buffer_pop(rx_buffer, 8);
		int32_t  t0j = le_get_i32(buffer_pop_i32(rx_buffer)) * (1.0f - rx_clk_offset);

		/* This node and the nonprime beacon must share the same prime beacon in order for timestamps
		 * to make any sense */
		same_prime = memcmp(update->new_nbrs[0].address, ptr, 8) == 0;

		update->adj |= (same_prime << compact_triu_index(offset, 6));

		/* The algorithm for computing distance between beacons is djk = tjk - t0j + d0k - d0j where:
		 *
		 * 		j is the beacon that just transmitted.
		 * 		k is this node.
		 *
		 * At this point, tjk has been measured and stored in tstamps[j,6] by rx_start_read() and
		 * t0j has just been read from the beacon. Compute and store t[j,6] = tjk - t0j. */
		update->tstamps[compact_triu_index(0, offset)]  = t0j;
		update->tstamps[compact_triu_index(offset, 6)] -= t0j;
	}
	else if(final && !same_prime)
	{
		update->adj = 0;
	}

	/* Current beacons */
	for(i = 1; i < end; i++)
	{
		uint8_t* ptr = buffer_pop(rx_buffer, 8);
		int32_t  tij = le_get_i32(buffer_pop_i32(rx_buffer)) * (1.0f - rx_clk_offset);

		unsigned ij = compact_triu_index(i, offset);
		unsigned i0 = compact_triu_index(0, i);

		/* The algorithm for computing distance between beacons is dij = tij - t0i + d0j - d0i where:
		 *
		 * 		i is the ith neighbor of j.
		 * 		j is the beacon that just transmitted.
		 *
		 * At this point: tij has been read from the beacon and t0i has been read from a previous
		 * beacon. Compute and store tij - t0i. Can't compute dij yet because d0j and d0i are
		 * received in the final beacon. */
		update->tstamps[ij] = tij - update->tstamps[i0];

		/* Check that this node and the beacon share the same neighborhood */
		if(memcmp(update->new_nbrs[i].address, ptr, 8) == 0 && tij != -1u)
		{
			update->adj |= (((update->new_nbrhood & (1 << i)) != 0 && same_prime) << ij);
		}
		/* If this node transmitted a beacon and it's not reported by the other beacon, then there
		 * was a conflict. */
		else if(i == update->offset && update->shouldtx)
		{
			update->conflicts |= (1 << offset);
		}
	}

	/* Previous beacons */
	for(; offset != 0 && i < (6 - end); i++)
	{
		uint8_t* ptr = buffer_pop(rx_buffer, 8);
		int32_t  tij = le_get_i32(buffer_pop_i32(rx_buffer));

		if(i != update->offset || !beacon_get_tx_hist(&loc->beacon, slot, dir))
		{
			continue;
		}
		else if(memcmp(loc->address, ptr, 8) == 0 && tij != -1u)
		{
			continue;
		}

		update->conflicts |= (1 << offset);
	}


	// /* Previous beacons */
	// /* Note: without history */
	// for(; offset != 0 && i < (6 - end); i++)
	// {
	// 	uint8_t* ptr = buffer_pop(rx_buffer, 8);
	// 	int32_t  tij = buffer_pop_le_i32(rx_buffer);

	// 	if(i != update->offset || !update->shouldtx)
	// 	{
	// 		continue;
	// 	}
	// 	else if(memcmp(update->new_nbrs[i].address, loc->addres, 8) == 0 && tij != -1u)
	// 	{
	// 		continue;
	// 	}

	// 	update->conflicts |= (1 << offset);
	// }

	skip:
		return true;

	error:
		update->new_nbrhood &= ~(1 << j);
		return false;
}


/* wait_for_trx *********************************************************************************//**
 * @brief		Waits for the previous DW1000 transaction (either transmit or receive) to complete.
 * 				Returns the final DW1000 system status register. */
static uint32_t wait_for_trx(Location* loc)
{
	uint32_t status = 0;

	/* Note: see RXDFR */

	while(0 == (status & (
		DW1000_SYS_STATUS_TXFRS   |	/* Tx Complete */
		DW1000_SYS_STATUS_RXFCG   |	/* Rx Complete */
		DW1000_SYS_STATUS_RXRFTO  |	/* Receive Frame Wait Timeout */
		DW1000_SYS_STATUS_RXPTO   |	/* Preamble Detection Timeout */
		DW1000_SYS_STATUS_RXPHE   |	/* Receiver PHY Header Error */
		DW1000_SYS_STATUS_RXFCE   |	/* Receiver FCS Error */
		DW1000_SYS_STATUS_RXRFSL  |	/* Receiver Reed Solomon Frame Sync Loss */
		DW1000_SYS_STATUS_RXSFDTO |	/* Receive SFD Timeout */
		DW1000_SYS_STATUS_AFFREJ  |	/* Automatic Frame Filtering Rejection */
		DW1000_SYS_STATUS_LDEERR)))	/* Leading edge detection processing error */
	{
		dw1000_wait_for_irq(loc->dw1000, -1);

		status = dw1000_handle_irq(loc->dw1000);
	}

	return status;
}


/* prepare_tstamps ******************************************************************************//**
 * @brief		Processes timestamps prior to computing location. */
static void prepare_tstamps(LocUpdate* update)
{
	unsigned i, j;

	/* Timestamps are invalid if the final beacon was not received */
	if((update->new_nbrhood & (0x1 << 6)) == 0 && (update->new_nbrhood & (0x1 << 0)) != 0)
	{
		update->adj = 0;
		return;
	}

	/* Move received timestamps to the row corresponding to this beacons's offset */
	if(update->shouldtx)
	{
		unsigned end = (update->offset == 0) ? 6 : update->offset;

		for(i = 1; i < end; i++)
		{
			unsigned ij = compact_triu_index(i, update->offset);
			unsigned ik = compact_triu_index(i, 6);

			update->tstamps[ij] = update->tstamps[ik];
			update->adj = (update->adj & ~(1 << ij)) | (((update->adj & (1 << ik)) != 0) << ij);
		}
	}

	/* Compute distances from the prime node to nonprime nodes */
	for(i = 1; i < 6; i++)
	{
		update->tstamps[compact_triu_index(0, i)] /= 2;
	}

	/* Compute interbeacon distances */
	for(i = 1; i < 6; i++)
	{
		for(j = i+1; j < 6; j++)
		{
			unsigned idx_ij = compact_triu_index(i,j);
			unsigned idx_0i = compact_triu_index(0,i);
			unsigned idx_0j = compact_triu_index(0,j);
			unsigned mask   = (1 << idx_ij) | (1 << idx_0i) | (1 << idx_0j);

			if((update->adj & mask) == mask)
			{
				update->tstamps[idx_ij] += update->tstamps[idx_0j] - update->tstamps[idx_0i];
			}
			else
			{
				update->adj &= ~(1 << idx_ij);
			}
		}
	}

	/* Prime beacon doesn't compute pseudoranges */
	if(update->shouldtx && update->offset == 0)
	{
		update->adj &= ~(0x3F << compact_triu_index(0, 6));
	}
	/* Compute pseudoranges */
	else
	{
		/* pik = tik - t0i - d0i. Note: tstamps[ik] already stores tik - t0i. */
		for(i = 1; i < 6; i++)
		{
			unsigned idx_ik = compact_triu_index(i, 6);
			unsigned idx_0i = compact_triu_index(0, i);
			unsigned mask   = (1 << idx_ik) | (1 << idx_0i);

			if((update->adj & mask) == mask)
			{
				update->tstamps[idx_ik] -= update->tstamps[idx_0i];
			}
			else
			{
				update->adj &= ~(1 << idx_ik);
			}
		}
	}
}


/* update_neighbors *****************************************************************************//**
 * @brief		Updates this node's neighborhood with the beacons from the current location
 * 				update. */
static void update_neighbors(Location* loc, LocUpdate* update)
{
	unsigned i, j;

	uint32_t outliers = local_outliers(loc, update);

	/* Update this node's neighborhood with the beacons from the current update */
	for(i = 0; i < 6; i++)
	{
		const unsigned idx = beacon_order[update->dir][update->slot][i];

		/* Neighbor is valid */
		if(update->new_nbrhood & (1 << i))
		{
			loc->all_nbrhood   |= (1 << idx);
			loc->neighbors[idx] = update->new_nbrs[i];
			loc->dropcount[idx] = 0;

			if(outliers & (1 << i))
			{
				loc->local_nbrhood &= ~(1 << idx);
			}
			else
			{
				loc->local_nbrhood |= (1 << idx);
			}

			/* Ensure that the beacon is unique in the neighbor table. A beacon could change indices
			 * which would leave an entry in the old index. */
			for(j = 0; j < 20; j++)
			{
				if(j != idx && (loc->all_nbrhood & (1 << j)) &&
				   memcmp(loc->neighbors[j].address, update->new_nbrs[i].address, 8) == 0)
				{
					loc->all_nbrhood   &= ~(1 << j);
					loc->local_nbrhood &= ~(1 << j);
				}
			}
		}
		/* Neighbor was not received */
		else if(loc->all_nbrhood & (1 << idx))
		{
			/* Neighbor is in the process of being dropped */
			if(loc->dropcount[idx] < NBR_DROP_MAX)
			{
				loc->dropcount[idx]++;
			}
			/* Neighbor hasn't been heard for NBR_DROP_MAX slots and is dropped */
			else
			{
				LOG_INF("dropping %d", idx);
				// memset(&loc->neighbors[idx], 0, sizeof(loc->neighbors[idx]));
				loc->all_nbrhood   &= ~(1 << idx);
				loc->local_nbrhood &= ~(1 << idx);
			}
		}
	}

	/* Detect if this beacon has an inconsistent location */
	if(calc_popcount_u32(loc->local_nbrhood) < calc_popcount_u32(loc->all_nbrhood) / 2)
	{
		/* Clear everything to eventually trigger LOCATION_LOST_EVENT in update_location */
		LOG_INF("inconsistent");
		beacon_stop(&loc->beacon);
		update->adj        = 0;
		loc->all_nbrhood   = 0;
		loc->local_nbrhood = 0;
		loc_clear(loc);
	}

	/* Zero the adj entries corresponding to nodes that have inconsistent locations */
	for(i = 0; i < 6; i++)
	{
		unsigned idx = beacon_order[update->dir][update->slot][i];

		if((update->new_nbrhood & (1 << i)) && !(loc->local_nbrhood & (1 << idx)))
		{
			for(j = 0; j < 7; j++)
			{
				if(i != j)
				{
					update->adj &= ~(1 << compact_triu_index(i, j));
				}
			}
		}
	}
}


/* update_location ******************************************************************************//**
 * @brief		Update's this node's location using the data from the specified location update
 * 				cell. Expects update's timestamps to be formatted with prepare_tstamps() before
 * 				calling update_location. */
static LocStatus update_location(Location* loc, LocUpdate* update)
{
	// LOG_DBG("start");

	/* Exclude beacons that have moved */
	verify_distances(update);

	unsigned mutual          = update_mutual_nbrhood(update);
	unsigned num_new_beacons = calc_popcount_u32    (mutual);
	bool     is_coplanar     = update_is_coplanar   (update, mutual);

	if(calc_popcount_u32(loc->all_nbrhood) >= 4)
	{
		LOG_DBG("num_new_beacons = %d", num_new_beacons);

		/* Use springs to update this node's location if this node is a beacon and this node has a
		 * location estimate. Using springs mitigates the effects of noise in distance
		 * measurements. */
		if(update->shouldtx && loc_is_finite(loc))
		{
			return compute_springs_location(loc, update);
		}
		/* For 3D TOA, this node needs to be a beacon with 4 other beacons. This node needs to be a
	 	 * beacon to receive distance measurements. Also, the value of num_new_beacons will include
		 * this node if this node is a beacon. Therefore, for 3D TOA, num_new_beacons must be at
		 * least 5. */
		else if(num_new_beacons >= 5 && !is_coplanar && update->shouldtx)
		{
			return compute_toa_location(loc, update);
		}
		/* For 3D TDOA, need 5 non-coplanar beacons: 1 prime beacon providing a time reference, and 4
		* nonprime beacons providing pseudoranges. */
		else if(num_new_beacons >= 5 && !is_coplanar && !update->shouldtx)
		{
			return compute_tdoa_location(loc, update);
		}
		/* 3D location may still be computed if the beacons are coplanar and this node is a beacon */
		else if(num_new_beacons >= 4 && update->shouldtx)
		{
			return compute_3sphere_location(loc, update);
		}
	}
	/* Bootstrapping logic. Note: don't check update->shouldtx here as that would cause bootstrapping
	 * nodes to trigger the next else if block and raise the LOCATION_LOST_EVENT. */
	else if(nbrs_with_root(loc) && calc_popcount_u32(loc->neighbors[0].nbrhood) <= 4)
	{
		if(update->shouldtx && beacon_index(&loc->beacon) == 0)
		{
			/* This beacon is the root beacon and is located at (0,0,0). */
			loc_set(loc, 0, 0, 0);
			return LOCATION_UPDATED;
		}
		else if(update->shouldtx && beacon_index(&loc->beacon) == 4)
		{
			/* This beacon is located at (d,0,0) where d is the distance between this node and the
			 * node at (0,0,0). */
			return compute_1line_location(loc, update);
		}
		else if(update->shouldtx && beacon_index(&loc->beacon) == 9)
		{
			/* This beacon is located at (e,f,0). Update location using intersection of 2 circles. */
			return compute_2circle_location(loc, update);
		}
		else if(update->shouldtx && beacon_index(&loc->beacon) == 13)
		{
			/* This beacon is located at (e,f,0). Update location using intersection of 2 circles. */
			return compute_2circle_location(loc, update);
		}
	}

	return LOCATION_NOT_UPDATED;
}


/* update_beacon ********************************************************************************//**
 * @brief		Updates this node's beacon state using the data from the specified location update
 * 				cell. */
static void update_beacon(Location* loc, LocUpdate* update)
{
	/* Detect if this node has been established as a beacon */
	if(update->shouldtx)
	{
		unsigned i;
		unsigned count = 0;

		for(i = 0; i < 6; i++)
		{
			unsigned idx = beacon_order[update->dir][update->slot][i];

			if(loc->all_nbrhood & (1 << idx))
			{
				count++;
			}
		}

		if(calc_popcount_u32(update->conflicts) <= (count - 1) / 2)
		{
			beacon_success(&loc->beacon);
		}
		else
		{
			beacon_backoff(&loc->beacon);
		}
	}

	/* If this node is trying to become a beacon but doesn't have a location yet and another node
	 * takes this node's beacon index, then just stop beacons on this node. */
	if(beacon_enabled(&loc->beacon) && update->offset < 6 && !loc_is_finite(loc) &&
	  (update->new_nbrhood & (1 << update->offset)) &&
	  (memcmp(update->new_nbrs[update->offset].address, loc->address, 8) != 0))
	{
		LOG_DBG("update = %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
			update->new_nbrs[update->offset].address[0],
			update->new_nbrs[update->offset].address[1],
			update->new_nbrs[update->offset].address[2],
			update->new_nbrs[update->offset].address[3],
			update->new_nbrs[update->offset].address[4],
			update->new_nbrs[update->offset].address[5],
			update->new_nbrs[update->offset].address[6],
			update->new_nbrs[update->offset].address[7]);

		LOG_DBG("loc    = %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
			loc->address[0], loc->address[1], loc->address[2], loc->address[3],
			loc->address[4], loc->address[5], loc->address[6], loc->address[7]);

		beacon_stop(&loc->beacon);
	}

	/* If location is valid, optimize beacons */
	if(loc_is_finite(loc))
	{
		optimize_beacons(loc);
	}
	/* If total number of beacon neighbors is greater than 3, then join as a beacon */
	else if(calc_popcount_u32(loc->all_nbrhood) >= 3u + beacon_enabled(&loc->beacon))
	{
		join_beacons(loc);
	}
	/* Bootstrap */
	else if(nbrs_with_root(loc))
	{
		if(!beacon_enabled(&loc->beacon) && is_root(loc))
		{
			beacon_start(&loc->beacon, 0, 0);
		}
		else if(!beacon_enabled(&loc->beacon) && loc->neighbors[0].nbrhood == 0x1)
		{
			beacon_start(&loc->beacon, 4, 0);
		}
		else if(!beacon_enabled(&loc->beacon) && loc->neighbors[0].nbrhood == 0x11)
		{
			beacon_start(&loc->beacon, 9, 0);
		}
	}
	else if(!is_root(loc))
	{
		beacon_stop(&loc->beacon);
	}
}



/* verify_distances *****************************************************************************//**
 * @brief		Removes beacons with inconsistencies between reported locations and measured
 *				distances. */
static void verify_distances(LocUpdate* update)
{
	unsigned i, j;
	uint8_t  uncertain[6] = { 0 };
	uint8_t  final        = 0x3F;

	for(i = 0; i < 6; i++)
	{
		/* Copy uncertain locations from previous beacons to avoid having to recompute floating
		 * point distances. */
		for(j = 0; j < i+1; j++)
		{
			uncertain[i] |= (uncertain[j] & (1 << i)) >> (i-j);
		}

		for(j = i+1; j < 6; j++)
		{
			unsigned ij = compact_triu_index(i, j);

			if((update->adj & (1 << ij)) == 0)
			{
				continue;
			}

			/* It is expected that the measured distance between two beacons matches the distance
			 * between the two beacon's reported location within a certain threshold. If the measured
			 * distance is greater than this threshold, then the beacon's actual location may be
			 * different than the reported location. */
			if(fabsf(vec3_dist(update->new_nbrs[i].loc, update->new_nbrs[j].loc) -
				update->tstamps[ij] * DW1000_TIME_RES * SPEED_OF_LIGHT) > LOC_FIXED_THRESHOLD)
			{
				uncertain[i] |= (1 << i) | (1 << j);
			}
		}

		/* Accumulate uncertain beacons */
		final &= uncertain[i];
	}

	/* Todo: stop beaconing if too many beacons have uncertain location? */

	/* Remove beacons with uncertain locations from the update. Either the measurement is too noisy,
	 * in which case the measurement might be better in subsequent updates, or the beacon has moved
	 * in which case the beacon will update it's own location. */
	for(i = 0; i < 6; i++)
	{
		/* Don't mark distance measurements to this node as invalid if this node is a beacon and has
		 * an inconsistent location. Leaving distances as valid allows this node to update its
		 * location. */
		if(final & (1 << i) && !(update->offset == i && update->shouldtx))
		{
			for(j = 0; j < 7; j++)
			{
				if(i != j)
				{
					update->adj &= ~(1 << compact_triu_index(i, j));
				}
			}
		}
	}
}


/* local_outliers *******************************************************************************//**
 * @brief		Returns a bitmask of bits [0-5] representing which new_nbrs are nonlocal to this
 * 				node. */
static uint32_t local_outliers(Location* loc, LocUpdate* update)
{
	/* If this node does not have a location set, then compute outliers just within this location
	 * update. */
	if(!loc_is_finite(loc))
	{
		return update_outliers(loc, update);
	}

	unsigned i;
	uint32_t local = 0;

	Vec3 ideal = quantize_to_grid(loc_get(loc));

	for(i = 0; i < 6; i++)
	{
		if(update->new_nbrhood & (1 << i))
		{
			Vec3 q = quantize_to_grid(update->new_nbrs[i].loc);

			if(vec3_dist(q, ideal) <= sqrtf(3.0f) * LATTICE_R)
			{
				local |= (1 << i);
			}
		}
	}

	return (local ^ 0x3F) & update->new_nbrhood;
}


/* update_outliers ******************************************************************************//**
 * @brief		Returns a bitmask of bits [0-5] representing which new_nbrs are nonlocal to this
 * 				location update. */
static uint32_t update_outliers(Location* loc, LocUpdate* update)
{
	unsigned i, j;
	bool     loc_is_set = loc_is_finite(loc);
	unsigned count      = calc_popcount_u32(update->new_nbrhood);

	for(i = 0; i < 6; i++)
	{
		if((update->new_nbrhood & (1 << i)) == 0)
		{
			continue;
		}

		uint32_t local = (1 << i);

		for(j = i+1; j < 6; j++)
		{
			if(update->new_nbrhood & (1 << j))
			{
				Vec3 qi = quantize_to_grid(update->new_nbrs[i].loc);
				Vec3 qj = quantize_to_grid(update->new_nbrs[j].loc);

				if(vec3_dist(qi, qj) <= sqrtf(3.0f) * LATTICE_R)
				{
					local |= (1 << j);
				}
			}
		}

		/* If this node transmitted a beacon but doesn't have a location, then consider this node
		 * part of the local neighborhood. */
		local |= ((update->shouldtx && !loc_is_set) << update->offset);

		if(calc_popcount_u32(local) > count / 2)
		{
			/* Flip local to outliers. Only consider actual neighbors. */
			return (local ^ 0x3F) & update->new_nbrhood;
		}
	}

	return (0x3F & update->new_nbrhood);
}


/* update_mutual_nbrhood ************************************************************************//**
 * @brief		Returns a bitmask of bits [0-5] of the new_nbrs that can hear each other. */
static uint32_t update_mutual_nbrhood(LocUpdate* update)
{
	/* Note: new_nbrhood bitmask is bits [0-6] where bit 6 is the final beacon from the prime
	 * node. */
	unsigned i, j;
	uint32_t mutual = (update->new_nbrhood & 0x3F);
	for(i = 0; i < 6; i++)
	{
		if(update->new_nbrhood & (1 << i))
		{
			for(j = i+1; j < 6; j++)
			{
				mutual &= ~((i != j && (update->adj & (1 << compact_triu_index(i, j))) == 0) << j);
			}
		}
	}

	return mutual;
}


/* update_is_coplanar ***************************************************************************//**
 * @brief		Returns true if the beacons in a location update are coplanar.
 * 				Note: mutual_nbrhood represents offsets, not indices. */
static bool update_is_coplanar(LocUpdate* update, uint32_t mutual_nbrhood)
{
	unsigned i;
	unsigned a_sheet = 0;	/* The sheet on the same level as the prime node */
	unsigned b_sheet = 0;	/* The sheet above the prime node */
	unsigned c_sheet = 0;	/* The sheet below the prime node */

	for(i = 0; i < 6; i++)
	{
		if(mutual_nbrhood & (1 << i))
		{
			unsigned index = beacon_order[update->dir][update->slot][i];
			unsigned pos   = relpos[update->slot][index];
			if(pos <= 8)       { a_sheet++; }
			else if(pos <= 12) { b_sheet++; }
			else if(pos <= 16) { c_sheet++; }
		}
	}

	return (a_sheet + b_sheet + c_sheet <= 3) || (a_sheet == 0) || (b_sheet + c_sheet == 0);
}


/* is_root **************************************************************************************//**
 * @brief		Returns true if this node is the root beacon. */
static bool is_root(Location* loc)
{
	return beacon_enabled(&loc->beacon) && vec3_norm(loc_get(loc)) <= LATTICE_R / 2;
}


/* nbrs_with_root *******************************************************************************//**
 * @brief		Returns true if this node is neighbors with the root (0,0,0) node of the network. */
static bool nbrs_with_root(Location* loc)
{
	return (loc->all_nbrhood & 1) && vec3_norm(loc->neighbors[0].loc) <= sqrtf(2) * LATTICE_R;
}


/* optimize_beacons *****************************************************************************//**
 * @brief		Tries to become a beacon if this node is located closer to an ideal point than an
 * 				existing beacon. */
static void optimize_beacons(Location* loc)
{
	Vec3     current   = loc_get(loc);
	Vec3     ideal     = quantize_to_grid(current);
	unsigned candidate = index_from_point(ideal);
	unsigned i, j;

	uint32_t grid_nbrhood = 0;

	/* Compute the local neighborhood by location, not by beacon index. For example, a node might be
	 * located at a nonprime location but chose to beacon as a prime if there is no suitable prime
	 * in its local neighborhood. Note: this step is not required if this node is naturally a
	 * candidate for a prime beacon. */
	for(i = 0; candidate > 4 && i < 20; i++)
	{
		if(loc->all_nbrhood & (1 << i))
		{
			Vec3 q = quantize_to_grid(loc->neighbors[i].loc);

			if(vec3_dist(q, ideal) <= sqrtf(3.0f) * LATTICE_R)
			{
				grid_nbrhood |= (1 << index_from_point(q));
			}
		}
	}

	/* Try and become a prime beacon if there is no prime beacon in this node's grid neighborhood.
	 * Loop through all possible primes (0,1,2,3) looking for a candidate. Note: this step is not
	 * required if this node is naturally a candidate for a prime beacon. */
	for(i = 0; candidate > 4 && (grid_nbrhood & 0xF) == 0 && i < 4; i++)
	{
		/* Local neighborhood of the prime beacon candidate */
		uint32_t prime_nbrhood = 0;

		/* Ideal location of the prime beacon candidate. */
		Vec3 prime = vec3_add(ideal, vec3_scale(vectors[relpos[candidate][i]], LATTICE_R));

		/* Compute what the neighborhood of the prime beacon would be if it was added to the local
		 * neighborhood */
		for(j = 0; j < 20; j++)
		{
			if(j != candidate && loc->all_nbrhood & (1 << j))
			{
				Vec3 q = quantize_to_grid(loc->neighbors[j].loc);

				if(vec3_dist(q, prime) <= sqrtf(3.0f) * LATTICE_R)
				{
					prime_nbrhood |= (1 << index_from_point(q));
				}
			}
		}

		/* A suitable prime beacon has at least 3 neighbors which can provide location updates */
		if(calc_popcount_u32(prime_nbrhood) < 3)
		{
			continue;
		}

		/* Do nothing if this node is already a suitable prime beacon */
		if(loc->all_nbrhood & (1 << i) && memcmp(loc->address, loc->neighbors[i].address, 8) == 0)
		{
			LOG_DBG("skip: already suitable prime beacon %d", i);
			return;
		}

		/* Start beaconing if this node is closer to the prime's ideal loction than the existing
		 * prime. */
		if(beacon_start(&loc->beacon, i, compare_distances(loc, i, prime)))
		{
			return;
		}
	}

	/* Do nothing if this node is already the ideal beacon */
	if(memcmp(loc->address, loc->neighbors[candidate].address, 8) == 0)
	{
		LOG_DBG("skip: already ideal beacon %d", candidate);
		return;
	}

	/* Try and start beaconing if this node is closer to the ideal location than the existing beacon.
	 * Stop beaconing if the other node is closer. */
	if(!beacon_start(&loc->beacon, candidate, compare_distances(loc, candidate, ideal)))
	{
		beacon_stop(&loc->beacon);
		beacon_set_index(&loc->beacon, candidate);
	}
}


/* compare_distances ****************************************************************************//**
 * @brief		Compares the distance between this node and the ideal point and a beacon specified by
 * 				index to the ideal point. Returns a positive value if this node is 25 % closer than
 * 				the existing beacon. Returns a negative value otherwise. */
static float compare_distances(Location* loc, unsigned i, Vec3 ideal)
{
	float dv = vec3_dist(loc_get(loc), ideal);

	if(loc->all_nbrhood & (1 << i))
	{
		float du = vec3_dist(loc->neighbors[i].loc, ideal);

		if(dv > du || (dv-du) / du >= -0.25f)
		{
			return -dv;
		}
	}

	return dv;
}


/* join_beacons *********************************************************************************//**
 * @brief		Tries to become a beacon if location data can't be provided by the local
 * 				neigbhborhood. */
static void join_beacons(Location* loc)
{
	// LOG_DBG("start");

	unsigned i, j, k;
	uint32_t candidates    = 0;
	uint32_t nbrhood_1_hop = loc->local_nbrhood;
	uint32_t nbrhood_2_hop = nbrhood_1_hop;
	uint32_t clearmask     = 0;
	float    dotmax        = 0;

	/* Compute this node's 2-hop neighborhood */
	for(i = 0; i < 20; i++)
	{
		if(nbrhood_1_hop & (1 << i))
		{
			clearmask     |= (!vec3_is_finite(&loc->neighbors[i].loc)) << i;
			nbrhood_2_hop |= loc->neighbors[i].nbrhood;
		}
	}

	/* Clear this node's bit in nbrhood if this node is a beacon */
	nbrhood_1_hop &= ~(beacon_enabled(&loc->beacon) << beacon_index(&loc->beacon));
	nbrhood_2_hop &= ~(beacon_enabled(&loc->beacon) << beacon_index(&loc->beacon));

	/* Clear bit corresponding to neighbor's with still unknown locations */
	nbrhood_1_hop &= ~(clearmask);
	nbrhood_2_hop &= ~(clearmask);

	LOG_DBG("beacon = %d, enabled = %d", beacon_index(&loc->beacon), beacon_enabled(&loc->beacon));

	/* Iterate cluster matrix */
	for(i = 0; i < 20; i++)
	{
		/* Find the first neighbor that is expected to be in the neighborhood of the candidate */
		for(j = 0; j < 20; j++)
		{
			if(nbrhood_1_hop & (1 << j) && relpos[i][j] < 17)
			{
				break;
			}
		}

		unsigned first = index_from_point(quantize_to_grid(loc->neighbors[j].loc));
		float    dot   = 0;

		/* Check that the relative positions of the neighbors match the expected positions */
		for(j += 1; j < 20; j++)
		{
			if(nbrhood_1_hop & (1 << j) && relpos[i][j] < 17)
			{
				/* Get the actual index of the node */
				unsigned k = index_from_point(quantize_to_grid(loc->neighbors[j].loc));

				/* Move the neighbor's actual position relative to the first neighbor */
				Vec3 shifted = vec3_sub(
					quantize_to_grid(loc->neighbors[j].loc),
					quantize_to_grid(loc->neighbors[first].loc));

				/* Move the neighbor's ideal vector relative to the first neighbor */
				Vec3 ideal = vec3_sub(vectors[relpos[i][k]], vectors[relpos[i][first]]);

				/* Accumulate the correlation between the ideal vector and the actual vector */
				dot += vec3_dot(shifted, ideal);
			}
		}

		// /* Keep track of candidates that have neighborhoods that most closely match the expected
		//  * positions. */
		// switch(compare_normalize(calc_compare_f(dot, dotmax)))
		// {
		// 	case  0: candidates |= (1 << i); break;
		// 	case +1: candidates  = (1 << i); dotmax = dot; break;
		// }

		/* Keep track of candidates that have neighborhoods that most closely match the expected
		 * positions. */
		if(calc_compare_f(dot, dotmax) == 0)
		{
			candidates |= (1 << i);
		}
		else if(calc_compare_f(dot, dotmax) > 0)
		{
			candidates = (1 << i);
			dotmax     = dot;
		}
	}

	/* Eliminate candidates that would cause conflicts for other neighbors */
	candidates &= ~(nbrhood_2_hop);

	for(i = 0; i < 20; i++)
	{
		/* For each candidate... */
		if((candidates & (1 << i)) == 0)
		{
			continue;
		}

		uint32_t expected_1_hop = nbrhood_1_hop;

		/* ... compute the candidate's expected 1-hop neighborhood as if the candidate was added to
		 * the neighborhood. For example: suppose this node is node 15 and it's 1-hop neighborhood is
		 * 0,9,19 and each neighbor's 1-hop neighborhood is:
		 *
		 *		0:  0,4,9,19
		 *		9:  0,4,9,19
		 *		19: 0,4,9,19
		 *
		 * Therefore the 2-hop neighborhood is 0,4,9,19. Given the 1-hop neighborhood, both 13 and 15
		 * are valid candidates. However, if 13 is added to the neighborhood, than it would expect to
		 * also hear node 4. Therefore node 13 can't be a candidate. If 15 is added to the
		 * neighborhood, it is not expected to hear node 4, therefore, 15 is a candidate. */
		for(j = 0; j < 20; j++)
		{
			/* For each 1-hop neighbor of candidate... */
			if((nbrhood_1_hop & (1 << j)) == 0)
			{
				continue;
			}

			for(k = 0; k < 20; k++)
			{
				/* ... check to see if the neighbors of neighbors that don't also exist in this
				 * node's 1-hop neighborhood are expected to be in the candidate's 1-hop
				 * neighborhood.
				 *
				 * 		i is the current candidate.
				 * 		j is the current 1-hop neighbor of this node.
				 * 		k is the current neighbor of j that doesn't exist in our 1-hop neighborhood.
				 *
				 * i and k are expected to be neighbors if the distance, as computed with positions
				 * in j's neighborhood, is less than the maximum distance between any 2 nodes, which
				 * is the diagonal of a unit square, or sqrt(2). */
				if(k == j)
				{
					continue;
				}
				else if((loc->neighbors[j].nbrhood & ~(nbrhood_1_hop | clearmask) & (1 << k)) == 0)
				{
					continue;
				}
				else if(relpos[j][i] == 17 || relpos[j][k] == 17)
				{
					continue;
				}
				else if(vec3_norm(vec3_sub(vectors[relpos[j][i]], vectors[relpos[j][k]])) > sqrtf(2))
				{
					continue;
				}

				expected_1_hop |= (1 << k);
			}
		}

		if(expected_1_hop != nbrhood_1_hop)
		{
			candidates &= ~(1 << i);
		}
	}

	for(i = 0; i < 20; i++)
	{
		if(candidates & (1 << i))
		{
			LOG_DBG("candidate = %d", i);
			beacon_start(&loc->beacon, i, 0);
			return;
		}
	}
}


/* quantize_to_grid *****************************************************************************//**
 * @brief		Returns the closest grid point to a given point. */
static Vec3 quantize_to_grid(Vec3 p)
{
	Vec3 q,r;

	/* Change of coordinates:
	 *
	 *		       | 1 0 -1/2 |
	 *		M^-1 = | 0 1 -1/2 | / LATTICE_R
	 *		       | 0 0  1   |
	 *
	 * Quantize z first to avoid over-estimating the ideal point. Example with LATTICE_R = 2.5:
	 *
	 * 		p = { 0.907493, 0.143357, 3.036491 };
	 *
	 * Not doing z first (incorrect):
	 *
	 * 		q = { 1.250000, -1.250000, 2.500000z };
	 *
	 * Doing z first (correct):
	 *
	 * 		q = { 1.250000, 1.250000, 2.500000z };
	 *
	 * Notice 0.143357 is closer to 1.25 than -1.25. */
	q.z = roundf((p.z)              / LATTICE_R) * LATTICE_R;
	q.x = roundf((p.x - q.z / 2.0f) / LATTICE_R) * LATTICE_R;
	q.y = roundf((p.y - q.z / 2.0f) / LATTICE_R) * LATTICE_R;

	/* Change of basis:
	 *
	 *		    | 1 0 1/2 |
	 *		M = | 0 1 1/2 | * LATTICE_R
	 *		    | 0 0 1   |
	 */
	r.x = q.x + q.z / 2.0f;
	r.y = q.y + q.z / 2.0f;
	r.z = q.z;

	return r;
}


/* index_from_point *****************************************************************************//**
 * @brief		Returns the beacon index that corresponds to the point. */
static unsigned index_from_point(Vec3 q)
{
	static const uint8_t beacons[2][10] = {
		{ 0,  4, 8,  12, 16, 1,  5,  9,  13, 17 },	/* A sheet beacons */
		{ 2,  6, 10, 14, 18, 3,  7,  11, 15, 19 },	/* B sheet beacons */
	};

	/* Pattern per sheet:
	 *
	 * 		16 1  5  9  13 17 0  4  8      -  -  -  -  -  -  0  -  -
	 * 		9  13 17 0  4  8  12 16 1      -  -  -  0  -  -  -  -  -
	 * 		0  4  8  12 16 1  5  9  13     0  -  -  -  -  -  -  -  -
	 * 		12 16 1  5  9  13 17 0  4      -  -  -  -  -  -  -  0  -
	 * 		5  9  13 17 0  4  8  12 16     -  -  -  -  0  -  -  -  -
	 * 		17 0  4  8  12 16 1  5  9      -  0  -  -  -  -  -  -  -
	 * 		8  12 16 1  5  9  13 17 0      -  -  -  -  -  -  -  -  0
	 * 		1  5  9  13 17 0  4  8  12     -  -  -  -  -  0  -  -  -
	 * 		13 17 0  4  8  12 16 1  5      -  -  0  -  -  -  -  -  -
	 *
	 * 		y = 1/3 * x => 3y = x => x - 3y
	 *
	 * Since q is quantized (e.g. [5, 0, 0] with LATTICE_R = 5), divide by LATTICE_R to turn the
	 * coordinates into indices. */
	int x = roundf((q.x / LATTICE_R) - (q.y * (3.0f / LATTICE_R)));
	int z = calc_mod_int(roundf(q.z / LATTICE_R), 4);

	/* Every two levels alternates between normal and swapped sheets. For example:
	 *
	 *		z-level 0: 0  4  8  12 16 1  5  9  13 17
	 *		z-level 2: 1  5  9  13 17 0  4  8  12 6
	 *
	 * The index is computed by offsetting the index by 5 every 2 sheets and wrapping the index.
	 * Since z is in the domain [0,4), z/2 will have the range [0,1]. Therefore, adding z/2*5 and
	 * wrapping the index will compute the correct index for normal and swapped sheets. */
	int i = calc_mod_int(x + z/2*5, 10);

	/* Alternate between A and B sheets */
	int j = calc_mod_int(z, 2);

	return beacons[j][i];
}


/* asn_to_slot **********************************************************************************//**
 * @brief		Converts asn to location beacon slot. For example:
 *
 * 					                 <---------Slotframe---------> <---------Slotframe--------->
 * 					ASN:             0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19
 * 					Loc Beacon Slot:       0  1  2  3                    0  1  2  3
 * 					Loc Beacon SF Length: 10
 */
static unsigned asn_to_slot(TsSlotframe* sf, uint64_t asn)
{
	return (asn % sf->numslots) / (sf->numslots / 4);
}


/* asn_to_dir ***********************************************************************************//**
 * @brief		Converts asn to location beacon direction. Each group of 4 location cells iterates
 * 				between NE, N, NW, W, SW, S, SE, E directions. For example, in slotframe 0, the 4
 * 				prime beacons will transmit to neighbors in the NE.
 * @desc		Location is encoded in numbers [0-7]:
 *
 * 				0 = NE
 * 				1 = N
 * 				2 = NW
 * 				3 =  W
 * 				4 = SW
 * 				5 = S
 * 				6 = SE
 * 				7 =  E
 */
static unsigned asn_to_dir(TsSlotframe* sf, uint64_t asn)
{
	return (asn / sf->numslots) % 8;
}


/* compact_triu_index ***************************************************************************//**
 * @brief		Computes the index into a compact column-wise upper triangular matrix shifted 1 to
 * 				the right of the diagonal. Example:
 *
 * 					  | 0 1 2 3 4 5 6              0 1-- 2---- 3------ 4-------- 5----------
 * 					--+--------------              0 0 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 1 1 2
 * 					0 | - a b d g k p              0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
 * 					1 | - - c e h l q  Stored as: [a b c d e f g h i j k l m n o p q r s t u]
 * 					2 | - - - f i m r
 * 					3 | - - - - j n s
 * 					4 | - - - - - o t
 * 					5 | - - - - - - u
 */
static unsigned compact_triu_index(unsigned i, unsigned j)
{
	unsigned min = calc_min_uint(i, j);
	unsigned max = calc_max_uint(i, j);
	return min + max * (max-1) / 2;
}


/* compute_springs_location *********************************************************************//**
 * @brief		Computes the location of this node as if springs where attached to it and the
 * 				neighboring nodes. */
static LocStatus compute_springs_location(Location* loc, LocUpdate* update)
{
	LOG_DBG("start");

	float r[5];
	Vec3  p[5];
	unsigned i, j;

	for(i = 0, j = 0; i < 6; i++)
	{
		if(i != update->offset && (update->adj & (1 << compact_triu_index(i, update->offset))))
		{
			/* Compute the distance between the beacon and this node */
			r[j] = update->tstamps[compact_triu_index(i, update->offset)] *
				DW1000_TIME_RES * SPEED_OF_LIGHT;

			/* Get the beacon's reported location */
			p[j] = update->new_nbrs[i].loc;

			j++;
		}
	}

	if(j < 3)
	{
		LOG_DBG("skip: insufficient number of beacons");
		return LOCATION_SKIP_NUM_BEACONS;
	}

	float ax = 0, ay = 0, az = 0;

	/* Compute spring forces between neighbors and this beacon. The spring's natural length is the
	 * distance measured during the location update. */
	for(i = 0; i < j; i++)
	{
		Vec3  v   = vec3_sub (loc_get(loc), p[i]);
		float mag = vec3_norm(v);

		if(isfinite(mag) && mag != 0)
		{
			Vec3 u = vec3_scale(v, 1.0f / mag);
			ax += LOC_KS * (r[i] * u.x - v.x) / LOC_M;
			ay += LOC_KS * (r[i] * u.y - v.y) / LOC_M;
			az += LOC_KS * (r[i] * u.z - v.z) / LOC_M;
		}
	}

	Vec3 x0 = loc_get(loc);					/* This node's current location */
	Vec3  g = quantize_to_grid(x0);			/* The closest lattice point to this node */
	Vec3 ug = vec3_unit(vec3_sub(g, x0));	/* Unit vector from current location to lattice point */

	/* Apply a constant attraction to the closest ideal point (ug: unit vector pointed from this
	 * node's position x0 to the ideal point g). This constant attraction is to keep the whole
	 * network from spinning and translating in space due to errors in measurements.
	 *
	 * Also apply damping which opposes this beacon's velocity (vel). */
	ax += (LOC_KG * ug.x / LOC_M) - (LOC_B * loc->vel.x / LOC_M);
	ay += (LOC_KG * ug.y / LOC_M) - (LOC_B * loc->vel.y / LOC_M);
	az += (LOC_KG * ug.z / LOC_M) - (LOC_B * loc->vel.z / LOC_M);

	/* Apply time step to acceleration to update this node's velocity */
	loc->vel.x += ax * LOC_DT;
	loc->vel.y += ay * LOC_DT;
	loc->vel.z += az * LOC_DT;

	/* Apply time step to velocity to update this node's position */
	x0.x += loc->vel.x * LOC_DT;
	x0.y += loc->vel.y * LOC_DT;
	x0.z += loc->vel.z * LOC_DT;

	loc_set(loc, x0.x, x0.y, x0.z);

	// LOG_DBG("done");
	LOG_INF("done");
	return LOCATION_UPDATED;
}


/* compute_1line_location ***********************************************************************//**
 * @brief		Computes the location of this node given one distance measurement. This function is
 * 				expected to be called to help bootstrap location services. The root beacon is
 * 				located at (0, 0, 0). This beacon will be placed at (d, 0, 0) where d is distance
 * 				from this node to the root beacon. */
static LocStatus compute_1line_location(Location* loc, LocUpdate* update)
{
	LOG_DBG("start");

	/* Computing distance along a line requires the prime beacon providing distance to this beacon */
	if((update->adj & (1 << compact_triu_index(0, update->offset))) == 0)
	{
		LOG_DBG("skip: insufficient number of beacons");
		return LOCATION_SKIP_NUM_BEACONS;
	}

	float d = update->tstamps[compact_triu_index(0, update->offset)] *
		DW1000_TIME_RES * SPEED_OF_LIGHT;

	loc_filter(loc, d, 0, 0);

	LOG_DBG("done");
	return LOCATION_UPDATED;
}


/* compute_2circle_location *********************************************************************//**
 * @brief		Computes the location of this node given two distance measurements. This function is
 * 				expected to be called to help bootstrap location services. The root beacon is
 * 				located at (0, 0, 0) and the first beacon is located at (d, 0, 0). This beacon will
 * 				be located at (+e, +f, 0). */
static LocStatus compute_2circle_location(Location* loc, LocUpdate* update)
{
	LOG_DBG("start");

	/* Computing 2 circle location requires the prime beacon and the beacon along the x-axis which
	 * only occurs during dir == 0 and slot == 0. */
	if(update->dir != 0 || update->slot != 0)
	{
		LOG_DBG("skip: invalid dir and slot");
		return LOCATION_SKIP_INVALID_DIR_SLOT;
	}

	float r[2];
	Vec3  p[2];
	unsigned i, j;

	/* Computing 2 circle location requires the prime beacon and the beacon along the x-axis, which
	 * are beacons 0 and 1. */
	for(i = 0, j = 0; i < 2 && j < 2; i++)
	{
		if(i != update->offset && update->adj & (1 << compact_triu_index(i, update->offset)))
		{
			/* Compute the distance between the beacon and this node */
			r[j] = update->tstamps[compact_triu_index(i, update->offset)] *
				DW1000_TIME_RES * SPEED_OF_LIGHT;

			/* Get the beacon's reported location */
			p[j] = update->new_nbrs[i].loc;

			j++;
		}
	}

	/* Computing intersection of 2 circles requires 2 beacons providing distances to this beacon. */
	if(j < 2)
	{
		LOG_DBG("skip: insufficient number of beacons");
		return LOCATION_SKIP_NUM_BEACONS;
	}

	Vec3 v1 = vec3_sub(p[1], p[0]);

	float d = vec3_norm(v1);
	float l = (calc_dop_f(r[0], r[0], r[1], r[1]) + d*d) / (2.0f * d);
	float h = sqrtf(calc_dop_f(r[0], r[0], l, l));

	/* There are 2 solutions (if they exist)
	 *
	 * 		x = l/d*(x2-x1) +/- h/d(y2-y1) + x1
	 * 		y = l/d*(y2-y1) -/+ h/d(x2-x1) + y1
	 *
	 * Which are calculated:
	 *
	 * 		Vec3 solp = make_vec3(l/d*(x1-x0) + h/d*(y1-y0) + x0, l/d*(y1-y0) - h/d*(x1-x0) + y0, 0);
	 * 		Vec3 solm = make_vec3(l/d*(x1-x0) - h/d*(y1-y0) + x0, l/d*(y1-y0) + h/d*(x1-x0) + y0, 0);
	 *
	 * Just take +y which is solm as compute_2circle_location is used only for bootstrapping which
	 * places p0 at 0,0,0 and p1 along the x axis. */
	loc_filter(loc, l/d * v1.x - h/d * v1.y + p[0].x, l/d * v1.y + h/d * v1.x + p[0].y, 0);

	LOG_DBG("done");
	return LOCATION_UPDATED;
}


/* compute_3sphere_location *********************************************************************//**
 * @brief		Computes the location of this node given three distances measurements and an
 * 				expected beacon index. The intersection of 3 spheres most often results in 2
 * 				solutions. The solutions are disambiguated by using this node's expected beacon
 * 				index, the local neighborhood, and the neighborhood's ideal vectors. The solution in
 * 				line with the expected vector is chosen.
 * @warning		Expects that this node's beacon index is set before calling
 * 				compute_3sphere_location(). */
static LocStatus compute_3sphere_location(Location* loc, LocUpdate* update)
{
	LOG_DBG("start");

	unsigned index[3];	/* Index of the other beacons */
	float    r[3];		/* Measured distance between this node and the other beacons */
	Vec3     p[3];		/* The reported position of the other beacons */
	unsigned i,j;

	/* The intersection of 3 spheres results in 2 points. Therefore, the beacon index needs to be set
	 * to disambiguate this node's position. */
	if(beacon_index(&loc->beacon) > 19)
	{
		LOG_DBG("skip: index %d", beacon_index(&loc->beacon));
		return LOCATION_SKIP_BINDEX_NOT_SET;
	}

	/* Find the three beacons providing distances to this beacon */
	for(i = 0, j = 0; i < 6 && j < 3; i++)
	{
		if(i != update->offset && update->adj & (1 << compact_triu_index(i, update->offset)))
		{
			/* Get the beacon's index */
			index[j] = beacon_order[update->dir][update->slot][i];

			/* Compute the distance between the beacon and this node */
			r[j] = update->tstamps[compact_triu_index(i, update->offset)] *
				DW1000_TIME_RES * SPEED_OF_LIGHT;

			/* Get the beacon's reported location */
			p[j] = update->new_nbrs[i].loc;

			j++;
		}
	}

	/* Computing intersection of 3 spheres requires 3 beacons providing distances to this beacon. */
	if(j < 3)
	{
		LOG_DBG("skip: insufficient number of beacons");
		return LOCATION_SKIP_NUM_BEACONS;
	}

	Vec3  v1  = vec3_sub(p[1], p[0]);				/* Vector from first beacon to second beacon */
	Vec3  v2  = vec3_sub(p[2], p[0]);				/* Vector from first beacon to third beacon  */
	float d   = vec3_norm(v1);						/* Local x coordinate of v1 */

	Vec3  u1  = vec3_scale(v1, 1.0f / d);			/* First unit vector is along v1 */
	float e   = vec3_dot(v2, u1);					/* Local x coordinate of v2 */

	Vec3  rej = vec3_sub(v2, vec3_scale(u1, e));	/* rej(v2,v1) = v2 - proj(v2,v1) = v2 - (e*u1) */
	float f   = vec3_norm(rej);						/* Local y coordinate of v2 */
	Vec3  u2  = vec3_scale(rej, 1.0f / f);			/* Second unit vector along rej */
	Vec3  u3  = vec3_cross(u1, u2);					/* Third unit vector */

	/* Solution for the particular case when p0 = (0,0,0), p1 = (d,0,0), p2 = (e,f,0) */
	float l = (r[0]*r[0] - r[1]*r[1] + d*d) / (2.0f * d);
	float w = (r[0]*r[0] - r[2]*r[2] - 2.0f*e*l + e*e + f*f) / (2.0f*f);
	float h = sqrtf(r[0]*r[0] - l*l - w*w);

	/* Solution for the general case: l*u1 + w*u2 +/- h*u3 + [x0,y0,z0]
	 *
	 * Use ideal vectors to determine which solution to pick. The triple product indicates if the
	 * solution is along (+) or opposite (-) the u3 basis. The triple product is:
	 *
	 * 		v0,3 . (v0,1 x v0,2)
	 *
	 * Note: order of indices is important. The particular order does not matter, just that the order
	 * is consistent with the order used to compute the basis vectors u1 and u2. */
	Vec3 v01 = vectors[relpos[index[0]][index[1]]];
	Vec3 v02 = vectors[relpos[index[0]][index[2]]];
	Vec3 v03 = vectors[relpos[index[0]][beacon_index(&loc->beacon)]];
	float triple = vec3_dot(v03, vec3_cross(v01, v02));

	Vec3 sol = p[0];
	sol = vec3_add(sol, vec3_scale(u1, l));
	sol = vec3_add(sol, vec3_scale(u2, w));
	sol = vec3_add(sol, vec3_scale(u3, copysignf(h, triple)));

	loc_filter(loc, sol.x, sol.y, sol.z);

	LOG_INF("done");
	return LOCATION_UPDATED;
}


/* compute_toa_location *************************************************************************//**
 * @brief		Computes this beacons's location using measured distances to other beacons.
 * @warning		Requires 4 non-coplanar beacons to determine this beacon's position. If the other
 * 				beacons are coplanar and this node knows its beacon index, then
 * 				compute_3sphere_location() can be used instead. */
static LocStatus compute_toa_location(Location* loc, LocUpdate* update)
{
	LOG_DBG("start");

	/* 	Equations of circles where d may be imprecise.
	 *
	 * 		(x0 - x)^2 + (y0 - y)^2 + (z0 - z)^2 = d0^2
	 * 		(x1 - x)^2 + (y1 - y)^2 + (z1 - z)^2 = d1^2
	 * 		(x2 - x)^2 + (y2 - y)^2 + (z2 - z)^2 = d2^2
	 * 		(x3 - x)^2 + (y3 - y)^2 + (z3 - z)^2 = d3^2
	 *
	 * Expanded:
	 *
	 * 		x0^2 - 2x*x0 - x^2 + y0^2 - 2y*y0 + y^2 + z0^2 - 2z*y0 + z^2 = d0^2
	 * 		x1^2 - 2x*x1 - x^2 + y1^2 - 2y*y1 + y^2 + z1^2 - 2z*y1 + z^2 = d1^2
	 * 		x2^2 - 2x*x2 - x^2 + y2^2 - 2y*y2 + y^2 + z2^2 - 2z*y2 + z^2 = d2^2
	 * 		x3^2 - 2x*x3 - x^2 + y3^2 - 2y*y3 + y^2 + z3^2 - 2z*y3 + z^2 = d3^2
	 *
	 * Subtract equations 2,3,4 from equation 1:
	 *
	 * 		(x0-x1)*x + (y0-y1)*y + (z0-z1)*z = 1/2 * (x0^2-x1^2 + y0^2-y1^2 + z0^2-z1^2 + d1^2-d0^2)
	 * 		(x0-x2)*x + (y0-y2)*y + (z0-z2)*z = 1/2 * (x0^2-x2^2 + y0^2-y2^2 + z0^2-z2^2 + d2^2-d0^2)
	 * 		(x0-x3)*x + (y0-y3)*y + (z0-z3)*z = 1/2 * (x0^2-x3^2 + y0^2-y3^2 + z0^2-z3^2 + d3^2-d0^2)
	 *
	 * The system of equations is overdetermined. Represent the system of equations in matrix form:
	 *
	 * 		Ax = b
	 *
	 * 		A = {
	 * 			{ x0-x1, y0-y1, z0-z1 },
	 * 			{ x0-x2, y0-y2, z0-z2 },
	 * 			{ x0-x3, y0-y3, z0-z3 },
	 * 		}
	 *
	 * 		b = {
	 * 			{ 1/2 * (x0^2-x1^2 + y0^2-y1^2 + z0^2-z1^2 + d1^2-d0^2) },
	 * 			{ 1/2 * (x0^2-x2^2 + y0^2-y2^2 + z0^2-z2^2 + d2^2-d0^2) },
	 * 			{ 1/2 * (x0^2-x3^2 + y0^2-y3^2 + z0^2-z3^2 + d3^2-d0^2) },
	 * 		}
	 *
	 * 		x = [ x y z ]'
	 *
	 * The solution is:
	 *
	 * 		x = (A' * A)^-1 * A' * b
	 *
	 * Which can be solved with QR decomposition:
	 *
	 * 		A = Q1 * R1
	 *
	 * 		R1 * x = Q1' * b
	 */
	Matrix A, B;
	float A_data[5][3];
	float B_data[5][1];
	float tau[3];

	Vec3  p0;
	float d0;
	unsigned i,j;

	/* Find the first beacon */
	for(i = 0; i < 6; i++)
	{
		if(i != update->offset && update->adj & (1 << compact_triu_index(i, update->offset)))
		{
			break;
		}
	}

	p0 = update->new_nbrs[i].loc;
	d0 = update->tstamps[compact_triu_index(i, update->offset)] * DW1000_TIME_RES * SPEED_OF_LIGHT;

	/* Find subsequent beacons and fill in the A and b matrices */
	for(i += 1, j = 0; i < 6; i++)
	{
		if(i != update->offset && update->adj & (1 << compact_triu_index(i, update->offset)))
		{
			Vec3  pi = update->new_nbrs[i].loc;
			float di = update->tstamps[compact_triu_index(i, update->offset)] *
				DW1000_TIME_RES * SPEED_OF_LIGHT;

			A_data[j][0]  = p0.x - pi.x;
			A_data[j][1]  = p0.y - pi.y;
			A_data[j][2]  = p0.z - pi.z;

			B_data[j][0]  = calc_dop_f(p0.x, p0.x, pi.x, pi.x);	/* (p0.x)^2 - (pi.x)^2 */
			B_data[j][0] += calc_dop_f(p0.y, p0.y, pi.y, pi.y);	/* (p0.y)^2 - (pi.y)^2 */
			B_data[j][0] += calc_dop_f(p0.z, p0.z, pi.z, pi.z);	/* (p0.z)^2 - (pi.z)^2 */
			B_data[j][0] += calc_dop_f(di, di, d0, d0);			/* (di)^2   - (d0)^2   */
			B_data[j][0] *= 1.0f / 2.0f;
			j++;
		}
	}

	/* Computing time of arrival requires at least 4 non-coplanar beacons providing distance
	 * measurements to this node. */
	if(j < 4)
	{
		LOG_DBG("skip: insufficient number of beacons. adj = %x", update->adj);
		return LOCATION_SKIP_NUM_BEACONS;
	}

	mat_init(&A, j, 3, A_data);
	mat_init(&B, j, 1, B_data);

	mat_qr        (&A, tau);
	mat_mult_qt   (&A, &B, tau);
	mat_qr_backsub(&A, &B);

	if(loc_filter(loc, B_data[0][0], B_data[1][0], B_data[2][0]))
	{
		LOG_INF("updated");
		return LOCATION_UPDATED;
	}
	else
	{
		LOG_INF("nonfinite");
		return LOCATION_TOA_NONFINITE;
	}
}


/* compute_tdoa_locaion *************************************************************************//**
 * @brief		Computes this node's location using difference of distances to beacons.
 * @warning		Requires 4 non-coplanar beacons to determine location. */
static LocStatus compute_tdoa_location(Location* loc, LocUpdate* update)
{
	LOG_DBG("start");

	/* 	Difference of distances:
	 *
	 * 		d1 = delta_10 + d0
	 * 		d2 = delta_20 + d0
	 * 		d3 = delta_30 + d0
	 *
	 * Equivalent equations as TOA but with d1, d2, d3 substituted.
	 *
	 * 		(x0-x1)*x + (y0-y1)*y + (z0-z1)*z =
	 * 			1/2 * (x0^2-x1^2 + y0^2-y1^2 + z0^2-z1^2 + delta_10^2 + 2*delta_10*d0)
	 * 		(x0-x2)*x + (y0-y2)*y + (z0-z2)*z =
	 * 			1/2 * (x0^2-x2^2 + y0^2-y2^2 + z0^2-z2^2 + delta_20^2 + 2*delta_20*d0)
	 * 		(x0-x3)*x + (y0-y3)*y + (z0-z3)*z =
	 * 			1/2 * (x0^2-x3^2 + y0^2-y3^2 + z0^2-z3^2 + delta_30^2 + 2*delta_30*d0)
	 *
	 * Represent the system of equations in matrix form:
	 *
	 * 		Ax = b1 + (b2 * d0)
	 *
	 * 		A = {
	 * 			{ x0-x1, y0-y1, z0-z1 },
	 * 			{ x0-x2, y0-y2, z0-z2 },
	 * 			{ x0-x3, y0-y3, z0-z3 },
	 * 		}
	 *
	 * 		b1 = {
	 * 			{ 1/2 * (x0^2-x1^2 + y0^2-y1^2 + z0^2-z1^2 + delta_10^2) },
	 * 			{ 1/2 * (x0^2-x2^2 + y0^2-y2^2 + z0^2-z2^2 + delta_20^2) },
	 * 			{ 1/2 * (x0^2-x3^2 + y0^2-y3^2 + z0^2-z3^2 + delta_30^2) },
	 * 		}
	 *
	 * 		b2 = {
	 * 			{ delta_10 },
	 * 			{ delta_20 },
	 * 			{ delta_30 },
	 * 		}
	 *
	 * 		x = (A' * A)^-1 * A' * (b1 + b2 * d0)
	 *
	 * Which can be solved with QR decomposition:
	 *
	 * 		A = Q1 * R1
	 *
	 * 		R1 * x = Q1' * [b1 b2]
	 *
	 * The solution will have the form:
	 *
	 * 		xs = a + b*d0    x[0][0] + x[0][1] * d0
	 * 		ys = c + d*d0 -> x[1][0] + x[1][1] * d0
	 * 		zs - e + f*d0    x[2][0] + x[2][1] * d0
	 *
	 * Find d0 by solving:
	 *
	 * 		(x - x0)^2 + (y - y0)^2 + (z - z0)^2 = d0^2
	 *
	 * Plug in the formulas for x, y, z:
	 *
	 * 		=> (a + b*d0 - x0)^2 + (c + d*d0 - y0)^2 + (e + f*d0 - z0)^2 = d0^2
	 * 		=> (m + b*d0)^2      + (n + d*d0)^2      + (o + f*d0)^2      = d0^2, where:
	 *
	 * 			m = a - x0
	 * 			n = c - y0
	 * 			o = e - z0
	 *
	 * 		=> m^2 + 2m*b*d0 + b^2*d0^2 + n^2 + 2n*d*d0 + d^2*d0^2 + o^2 + 2o*f*d0 + f^2*d0^2 = d0^2
	 * 		=> b^2*d0^2 + d^2*d0^2 + f^2*d0^2 + 2m*b*d0 + 2n*d*d0 + 2o*f*d0 + m^2 + n^2 + o^2 = d0^2
	 * 		=> (b^2 + d^2 + f^2 - 1)*d0^2 + (2m*b + 2n*d + 2o*f)d0 + (m^2 + n^2 + o^2) = 0
	 * 		    ^^^^^^^^^^^^^^^^^^^          ^^^^^^^^^^^^^^^^^^       ^^^^^^^^^^^^^^^
	 * 		    A                            B                        C
	 *
	 * 			A = b*b + d*d + f*f - 1  = x[0][1]*x[0][1] + x[1][1]*x[1][1] + x[2][1]*x[2][1] - 1
	 * 			B = 2(m*b + n*d + o*f)   = 2(m*x[0][1] + n*x[1][1] + o*x[2][1])
	 * 			C = m*m + n*n + o*o      = m*m + n*n + o*o
	 *
	 * 		=> d0 = (-B +/- sqrt(B^2 - 4 * A * C)) / (2*A)
	 *
	 * Pick d0 to be positive and plug d0 back in to find x, y, and z.
	 *
	 * 		xs = a + b*d0
	 * 		ys = c + d*d0
	 * 		zs - e + f*d0
	 */
	Matrix A, B;
	float A_data[5][3];
	float B_data[5][2];
	float tau[3];

	unsigned i, j;

	/* Find the first neighbor */
	for(i = 1; i < 6; i++)
	{
		if(update->adj & (1 << compact_triu_index(i, 6)))
		{
			break;
		}
	}

	if(i >= 6)
	{
		return LOCATION_SKIP_INVALID_DIR_SLOT;
	}

	/* Location of the first neighbor */
	Vec3 p0 = update->new_nbrs[i].loc;

	/* Pseudorange stored in column 6: p1k = t1k - t01 - d01 */
	float p0k = update->tstamps[compact_triu_index(i, 6)] * DW1000_TIME_RES * SPEED_OF_LIGHT;

	for(i += 1, j = 0; i < 6; i++)
	{
		if(update->adj & (1 << compact_triu_index(i, 6)))
		{
			/* Location of the i'th neighbor */
			Vec3 pi = update->new_nbrs[i].loc;

			/* Pseudorange of the i'th neighbor
			 *
			 * 		pik = tik - t0i - d0i
			 *
			 * Distance to the i'th neighobr
			 *
			 * 		dik = pik + d0k
			 *
			 * However d0k is not known. To find a solution, pseudoranges can be subtracted from each
			 * other.
			 *
			 * 		d1k = p1k + d0k
			 * 		d2k = p2k + d0k
			 * 	=>	d2k - d1k = (p2k + d0k) - (p1k + d0k)
			 * 	=>	          =  p2k + d0k  -  p1k - d0k
			 * 	=>	          =  p2k - p1k
			 *
			 * In other words, the difference of two pseudoranges is the same as the difference of
			 * two distances. Which means that 4 pseudoranges are required for 3 hyperbolas. Which
			 * also means that 5 beacons are required for a 3D TDOA location update: 1 prime beacon
			 * which provides a time reference, and 4 nonprime beacons providing 4 pseudoranges. */
			float pik = update->tstamps[compact_triu_index(i, 6)] * DW1000_TIME_RES * SPEED_OF_LIGHT;

			A_data[j][0]  = p0.x - pi.x;
			A_data[j][1]  = p0.y - pi.y;
			A_data[j][2]  = p0.z - pi.z;

			B_data[j][0]  = calc_dop_f(p0.x, p0.x, pi.x, pi.x);	/* x0^2 - xi^2 */
			B_data[j][0] += calc_dop_f(p0.y, p0.y, pi.y, pi.y);	/* y0^2 - yi^2 */
			B_data[j][0] += calc_dop_f(p0.z, p0.z, pi.z, pi.z);	/* z0^2 - zi^2 */
			B_data[j][0] += (pik - p0k) * (pik - p0k);
			B_data[j][0] *= 1.0f / 2.0f;

			B_data[j][1] = pik - p0k;
			j++;
		}
	}

	/* Computing TDOA location requires at least 3 differences of pseudoranges */
	if(j < 3)
	{
		LOG_DBG("skip: insufficient number of beacons");
		return LOCATION_SKIP_NUM_BEACONS;
	}

	mat_init(&A, j, 3, A_data);
	mat_init(&B, j, 2, B_data);

	mat_qr        (&A, tau);
	mat_mult_qt   (&A, &B, tau);
	mat_qr_backsub(&A, &B);

	float m  = B_data[0][0] - p0.x;
	float n  = B_data[1][0] - p0.y;
	float o  = B_data[2][0] - p0.z;

	/* Solve the quadratic equation */
	float qa = (B_data[0][1] * B_data[0][1]) +
	           (B_data[1][1] * B_data[1][1]) +
	           (B_data[2][1] * B_data[2][1]) - 1.0f;
	float qb = 2.0f * (m * B_data[0][1] + n * B_data[1][1] + o * B_data[2][1]);
	float qc = m*m + n*n + o*o;
	float qm;

	calc_ax2_bx_c_f(qa, qb, qc, 0, &qm);

	/* Todo: reject solutions far away from the neighbors */
	/* Todo: what happens if this node moves to a new location? */
	Vec3 sol = make_vec3(
		B_data[0][0] + B_data[0][1] * qm,
		B_data[1][0] + B_data[1][1] * qm,
		B_data[2][0] + B_data[2][1] * qm);

	for(i = 0; i < j; i++)
	{
		if(update->adj & (1 << compact_triu_index(i, 6)) &&
		   vec3_dist(sol, update->new_nbrs[i].loc) > sqrtf(3.0f) * LATTICE_R)
		{
			LOG_INF("ignore inaccurate location");
			return LOCATION_SKIP_INACCURATE;
		}
	}

	if(loc_filter(loc, sol.x, sol.y, sol.z))
	{
		LOG_INF("updated");
		return LOCATION_UPDATED;
	}
	else
	{
		LOG_INF("nonfinite");
		return LOCATION_TDOA_NONFINITE;
	}
}





// ----------------------------------------------------------------------------------------------- //
// Location Update Frame                                                                           //
// ----------------------------------------------------------------------------------------------- //
// /* frame_get_version ****************************************************************************//**
//  * @brief		Returns the location update frame's version. */
// static uint8_t frame_get_version(const Ieee154_Frame* frame)
// {
// 	return le_get_u8(buffer_peek_at(&frame->buffer, ieee154_payload_ptr(frame) + 0));
// }


// /* frame_get_class ******************************************************************************//**
//  * @brief		Returns the location update frame's class. */
// static uint8_t frame_get_class(const Ieee154_Frame* frame)
// {
// 	return le_get_u8(buffer_peek_at(&frame->buffer, ieee154_payload_ptr(frame) + 1));
// }


// /* frame_get_dir ********************************************************************************//**
//  * @brief		Returns the location update frame's direction. */
// static uint8_t frame_get_dir(const Ieee154_Frame* frame)
// {
// 	return (le_get_u8(buffer_peek_at(&frame->buffer, ieee154_payload_ptr(frame) + 2)) >> 5) & 0x7;
// }


// /* frame_get_slot *******************************************************************************//**
//  * @brief		Returns the location update frame's slot. */
// static uint8_t frame_get_slot(const Ieee154_Frame* frame)
// {
// 	return (le_get_u8(buffer_peek_at(&frame->buffer, ieee154_payload_ptr(frame) + 2)) >> 3) & 0x3;
// }


/* frame_set_offset *****************************************************************************//**
 * @brief		Sets the beacon's offset */
static void frame_set_offset(Ieee154_Frame* frame, uint8_t offset)
{
	uint8_t dso = le_get_u8(buffer_peek_at(&frame->buffer, ieee154_payload_ptr(frame) + 2, 0));

	dso = (dso & ~(0x7)) | (offset & 0x7);

	le_set_u8(buffer_peek_at(&frame->buffer, ieee154_payload_ptr(frame) + 2, 0), dso);
}


// /* frame_get_offset *****************************************************************************//**
//  * @brief		Returns the beacon's offset. */
// static uint8_t frame_get_offset(const Ieee154_Frame* frame)
// {
// 	return (le_get_u8(buffer_peek_at(&frame->buffer, ieee154_payload_ptr(frame) + 2)) >> 0) & 0x7;
// }


// /* frame_get_location ***************************************************************************//**
//  * @brief		Returns the location update frame's location. */
// static Vec3 frame_get_location(const Ieee154_Frame* frame)
// {
// 	Vec3 ret;
// 	memmove(&ret.x, buffer_peek_at(&frame->buffer, ieee154_payload_ptr(frame) + 4),  4);
// 	memmove(&ret.y, buffer_peek_at(&frame->buffer, ieee154_payload_ptr(frame) + 8),  4);
// 	memmove(&ret.z, buffer_peek_at(&frame->buffer, ieee154_payload_ptr(frame) + 12), 4);
// 	return ret;
// }


// /* frame_get_nbrhood ****************************************************************************//**
//  * @brief		Returns the location update frame's direction. */
// static uint32_t frame_get_nbrhood(const Ieee154_Frame* frame)
// {
// 	return le_get_u32(buffer_peek_at(&frame->buffer, ieee154_payload_ptr(frame) + 16));
// }


// /* frame_push_addr ******************************************************************************//**
//  * @brief		Appends a neighbor's address to the location update frame. */
// static void frame_push_addr(Ieee154_Frame* frame, const uint8_t* addr)
// {
// 	buffer_push_mem(&frame->buffer, addr, 8);
// }


// /* frame_push_tstamp ****************************************************************************//**
//  * @brief		Appends a neighbor's timestamp to the location update frame. */
// static void frame_push_tstamp(Ieee154_Frame* frame, int32_t tstamp)
// {
// 	buffer_push_le_u32(&frame->buffer, tstamp);
// }


/* frame_update_addr ****************************************************************************//**
 * @brief		Updates a neighbor's address in the location update frame. */
static void frame_update_addr(Ieee154_Frame* frame, const void* addr, unsigned offset)
{
	offset = ieee154_payload_start(frame) +
		offsetof(LocBeacon, nbrs)      +
		offsetof(LocTstamp, addr)      +
		(offset * sizeof(LocTstamp));

	buffer_replace_offset(&frame->buffer, addr, offset, 8);
}


/* frame_update_tstamp **************************************************************************//**
 * @brief		Updates a neighbor's timestamp in the location update frame. */
static void frame_update_tstamp(Ieee154_Frame* frame, int32_t tstamp, unsigned offset)
{
	offset = ieee154_payload_start(frame) +
		offsetof(LocBeacon, nbrs)      +
		offsetof(LocTstamp, tstamp)    +
		(offset * sizeof(LocTstamp));

	le_set_i32(buffer_peek_offset(&frame->buffer, offset, 0), tstamp);
}





// ----------------------------------------------------------------------------------------------- //
// Location Beacon                                                                                 //
// ----------------------------------------------------------------------------------------------- //
/* beacon_init **********************************************************************************//**
 * @brief		Initializes beacons in the silent state. */
static void beacon_init(Beacon* b)
{
	if(b->current_state != BEACON_FORCED_STATE)
	{
		b->current_state   = BEACON_SILENT_STATE;
		b->next_state      = BEACON_SILENT_STATE;
		b->index           = 20;
		b->tx_hist         = 0;	/* Todo */
	}

	backoff_init(&b->backoff, 1, 32);
}


/* beacon_index *********************************************************************************//**
 * @brief		Returns this node's beacon index. Beacon index may be set even if beacons are
 * 				disabled. Since beacon index is determined by this node's location in the network,
 * 				setting beacon index independently of the beacon enabled state means that costly
 * 				floating point operations can be avoided to look up this node's index. */
static unsigned beacon_index(Beacon* b)
{
	return b->index;
}


/* beacon_offset ********************************************************************************//**
 * @brief		Returns this node's beacon offset in a particular location update slot. */
static unsigned beacon_offset(Beacon* b, unsigned dir, unsigned slot)
{
	unsigned i;

	for(i = 0; i < sizeof(beacon_order[0][0]) / sizeof(beacon_order[0][0][0]); i++)
	{
		if(beacon_order[dir][slot][i] == beacon_index(b))
		{
			return i;
		}
	}

	return -1u;
}


/* beacon_enabled *******************************************************************************//**
 * @brief		Returns true if transmitting location beacons is enabled. */
static bool beacon_enabled(Beacon* b)
{
	if(b->current_state == BEACON_JOINED_STATE || b->current_state == BEACON_FORCED_STATE)
	{
		return true;
	}
	else if(b->current_state == BEACON_JOINING_STATE)
	{
		if(b->start_timer == 0)
		{
			return true;
		}
		else
		{
			b->start_timer--;
			return false;
		}
	}

	return false;
}


/* beacon_get_tx_hist ***************************************************************************//**
 * @brief		Returns true if this node transmitted the last time the location update specified by
 * 				slot and dir was active. */
static bool beacon_get_tx_hist(const Beacon* b, uint8_t slot, uint8_t dir)
{
	/*      0000 0000 0011 1111 1111 2222 2222 2233
	 *      0123 4567 8901 2345 6789 0123 4567 8901
	 *
	 * dir  0000 1111 2222 3333 4444 5555 6666 7777
	 * slot 0123 0123 0123 0123 0123 0123 0123 0123 */
	return b->tx_hist & (1 << (slot + (dir * 4)));
}


/* beacon_set_tx_hist ***************************************************************************//**
 * @brief		Stores the beacon transmit history for the location update specified by slot and
 *				dir. */
static void beacon_set_tx_hist(Beacon* b, uint8_t slot, uint8_t dir, bool did_tx)
{
	/*      0000 0000 0011 1111 1111 2222 2222 2233
	 *      0123 4567 8901 2345 6789 0123 4567 8901
	 *
	 * dir  0000 1111 2222 3333 4444 5555 6666 7777
	 * slot 0123 0123 0123 0123 0123 0123 0123 0123 */
	uint32_t idx = slot + (dir * 4);

	b->tx_hist = (b->tx_hist & ~(1 << idx)) | ((did_tx != 0) << idx);
}


/* beacon_try ***********************************************************************************//**
 * @brief		Returns true if this node should transmit a beacon. Returns false if this node has
 * 				disabled transmitting beacons or if beacons are backing off because of a conflict. */
static bool beacon_try(Beacon* b)
{
	return beacon_enabled(b) && backoff_try(&b->backoff);
}


/* beacon_start *********************************************************************************//**
 * @brief		Starts a timer to begin transmitting location beacons. The timer duration is scaled
 * 				by the distance from this node's location to the beacon's ideal location. */
static bool beacon_start(Beacon* b, unsigned index, float distance)
{
	if(distance >= 0)
	{
		beacon_handle(b, BEACON_START_EVENT, index, distance);
		return true;
	}
	else
	{
		return false;
	}
}


/* beacon_stop **********************************************************************************//**
 * @brief		Stops transmitting location beacons. */
static void beacon_stop(Beacon* b)
{
	beacon_handle(b, BEACON_STOP_EVENT, 0, 0);
}


/* beacon_set_inddex ****************************************************************************//**
 * @brief		Updates the beacon's index. */
static void beacon_set_index(Beacon* b, unsigned index)
{
	beacon_handle(b, BEACON_SET_INDEX_EVENT, index, 0);
}


/* beacon_success *******************************************************************************//**
 * @brief		Beacon was transmitted successfully (success for a beacon means that a neighbor
 * 				received our beacon and transmitted some form of acknowledgement). */
static void beacon_success(Beacon* b)
{
	LOG_DBG("success");

	backoff_success(&b->backoff);

	beacon_handle(b, BEACON_JOINED_EVENT, 0, 0);
}


/* beacon_backoff *******************************************************************************//**
 * @brief		Backs off on transmitting a beacon. This is required if there was a conflict. */
static void beacon_backoff(Beacon* b)
{
	LOG_DBG("backoff");

	backoff_fail(&b->backoff);
}


/* beacon_handle ********************************************************************************//**
 * @brief		Handles a beacon event. */
static void beacon_handle(Beacon* b, BeaconEvent e, unsigned index, float distance)
{
	if(e == BEACON_FORCE_INDEX_EVENT && b->current_state != BEACON_FORCED_STATE)
	{
		LOG_INF("BEACON_FORCE_EVENT -> BEACON_FORCED_STATE index = %d", index);

		if(b->index != index)
		{
			b->tx_hist = 0;
			b->index   = index;
			backoff_reset(&b->backoff);
		}

		b->next_state = BEACON_FORCED_STATE;
	}

	if(e == BEACON_ALLOW_EVENT)
	{
		b->allow_beaconing = true;
	}
	else if(e == BEACON_DISALLOW_EVENT)
	{
		b->allow_beaconing = false;
		b->start_timer     = 0;
		b->current_state   = BEACON_SILENT_STATE;
		b->next_state      = BEACON_SILENT_STATE;
	}
	else if(e == BEACON_SET_INDEX_EVENT && b->index != index)
	{
		LOG_INF("BEACON_SET_INDEX -> BEACON_SILENT_STATE index = %d", index);

		b->tx_hist     = 0;
		b->index       = index;
		b->next_state  = BEACON_SILENT_STATE;
		b->start_timer = 0;
	}

	switch(b->current_state)
	{
		case BEACON_SILENT_STATE:
			if(e == BEACON_START_EVENT && b->allow_beaconing)
			{
				LOG_INF("BEACON_SILENT_STATE. Got BEACON_START_EVENT -> BEACON_JOINING_STATE index = %d", index);

				b->tx_hist     = 0;
				b->index       = index;
				b->start_timer = beacon_rand(distance);
				b->next_state  = BEACON_JOINING_STATE;
				backoff_reset(&b->backoff);
			}
			break;

		case BEACON_JOINING_STATE:
			if(e == BEACON_STOP_EVENT)
			{
				LOG_INF("BEACON_JOINING_STATE. Got BEACON_STOP_EVENT -> BEACON_SILENT_STATE");

				b->next_state  = BEACON_SILENT_STATE;
				b->start_timer = 0;
			}
			else if(e == BEACON_START_EVENT && b->allow_beaconing && index != b->index)
			{
				LOG_INF("BEACON_JOINING_STATE. Got BEACON_START_EVENT -> BEACON_JOINING_STATE index = %d", index);

				b->tx_hist     = 0;
				b->index       = index;
				b->start_timer = beacon_rand(distance);
				b->next_state  = BEACON_JOINING_STATE;
				backoff_reset(&b->backoff);
			}
			else if(e == BEACON_JOINED_EVENT)
			{
				LOG_INF("BEACON_JOINING_STATE. Got BEACON_JOINED_EVENT -> BEACON_JOINED_STATE");

				b->next_state = BEACON_JOINED_STATE;
			}
			break;

		case BEACON_JOINED_STATE:
			if(e == BEACON_STOP_EVENT)
			{
				LOG_INF("BEACON_JOINED_STATE. Got BEACON_STOP_EVENT -> BEACON_SILENT_STATE");

				b->start_timer = 0;
				b->next_state  = BEACON_SILENT_STATE;
			}
			else if(e == BEACON_START_EVENT && b->allow_beaconing && index != b->index)
			{
				LOG_INF("BEACON_JOINED_STATE. Got BEACON_START_EVENT -> BEACON_JOINING_STATE index = %d", index);

				b->tx_hist     = 0;
				b->index       = index;
				b->start_timer = beacon_rand(distance);
				b->next_state  = BEACON_JOINING_STATE;
				backoff_reset(&b->backoff);
			}
			break;

		case BEACON_FORCED_STATE:
			break;

		default: break;
	}

	b->current_state = b->next_state;
}


/* beacon_rand **********************************************************************************//**
 * @brief		Returns a random number scaled by distance. Shorter distances result in smaller
 * 				return values while larger distances result in larger return values. The return value
 * 				is expected to be used to set a timer for this node to start transmitting beacons.
 * 				If a node is closer to the ideal point, it will start transmitting sooner, taking
 * 				priority over other nodes that are also trying to transmit but may be further
 * 				away. */
static unsigned beacon_rand(float distance)
{
	float x = distance / (LATTICE_R / 2);
	float r = (float)(rand()) / (RAND_MAX + 1.0f);

	return (12.0f * x) + (4.0f * x * r);



	// /* Circles centered at lattice points have radii of LATTICE_R / 2. If nodes are uniformly
	//  * distributed, then the number of nodes centered at lattice points grows with r^2. The area of
	//  * a circle at the lattice point is:
	//  *
	//  * 		A_L = pi * (LATTICE_R / 2)^2
	//  * 		=>  = pi * LATTICE_R^2 / 4
	//  *
	//  * The area of a circle with radius = distance is:
	//  *
	//  * 		A_D = pi * distance^2
	//  *
	//  * Compute the ratio x of the two circles:
	//  *
	//  * 		 x = A_D / A_L
	//  * 		=> = (pi * distance^2) / (pi * LATTICE_R^2 / 4)
	//  * 		=> = (4 * distance^2) / (LATTICE_R^2)
	//  */
	// float x = (4 * distance * distance) / (LATTICE_R * LATTICE_R);
	// float r = (float)(rand()) / (RAND_MAX + 1.0f);

	// return (12.0f * x) + (4.0f * x * r);



	// /* Spheres centered at lattice points have radii of LATTICE_R / 2. If nodes are uniformly
	//  * distributed in space, then the number of nodes centered at a lattice points grows with r^3.
	//  * The volume of the sphere at the lattice point is:
	//  *
	//  * 		V_L = 4/3 * pi * (LATTICE_R / 2)^3
	//  * 		=>  = 1/6 * pi * LATTICE_R^3
	//  *
	//  * The volume of the sphere at distance from the lattice point is:
	//  *
	//  * 		V_D = 4/3 * pi * distance^3
	//  *
	//  * Compute the ratio x of the distance sphere and the lattice sphere:
	//  *
	//  * 		 x = V_D / V_L
	//  * 		=> = (4/3 * pi * distance^3) / (1/6 * pi * LATTICE_R^3)
	//  * 		=> = (8 * distance^3) / (LATTICE_R^3)
	//  */
	// float x = (8 * distance * distance * distance) / (LATTICE_R * LATTICE_R * LATTICE_R);
	// float r = (float)(rand()) / (RAND_MAX + 1.0f);

	// return (12.0f * x) + (4 * x * r);
}


/******************************************* END OF FILE *******************************************/
