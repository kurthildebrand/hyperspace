/************************************************************************************************//**
 * @file		location.h
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
#ifndef LOCATION_H
#define LOCATION_H

#if __STDC_VERSION__ < 199901L
#error Compile with C99 or higher!
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* Includes -------------------------------------------------------------------------------------- */
#include <stdbool.h>
#include <stdint.h>

#include "dw1000.h"
#include "matrix.h"
#include "timeslot.h"


/* Public Macros --------------------------------------------------------------------------------- */
// // // #define LOC_GRID_LENGTH         (1000)
// // #define LOC_GRID_LENGTH         (700)
// // #define LOC_TX_START_TIME       (400)
// // #define LOC_RX_START_TIME       (200)
// // #define LOC_RX_GUARD_TIME       (150)
// // #define LOC_RX_START_TIMEOUT    (400)
// // #define LOC_RX_TIMEOUT          (300)

// // #define LOC_RX_GUARD_TIME         (50)
// // #define LOC_RX_TIMEOUT         (100)


// // #define LOC_GRID_LENGTH         (700)
// #define LOC_GRID_LENGTH         (1000)
// // #define LOC_GRID_LENGTH         (2000)
// #define LOC_TX_START_TIME       (400)
// #define LOC_RX_START_TIME       (200)
// #define LOC_RX_START_TIMEOUT    (400)
// #define LOC_RX_GUARD_TIME       (200)
// // #define LOC_RX_GUARD_TIME       (150)
// // #define LOC_RX_GUARD_TIME       (125)
// // #define LOC_RX_GUARD_TIME       (110)
// // #define LOC_RX_GUARD_TIME       (100)
// // #define LOC_RX_GUARD_TIME       (75)
// // #define LOC_RX_GUARD_TIME       (50)
// #define LOC_RX_TIMEOUT          (400)
// // #define LOC_RX_TIMEOUT          (300)


// #define LOC_GRID_LENGTH         (800)
// // #define LOC_TX_START_TIME       (400)
// #define LOC_TX_START_TIME       (800)
// #define LOC_RX_GUARD_TIME       (200)
// #define LOC_RX_TIMEOUT          (400)


#define LOC_GRID_LENGTH         (800)
// #define LOC_GRID_LENGTH         (1000)
// #define LOC_TX_START_TIME       (400)
#define LOC_TX_START_TIME       (800)
#define LOC_RX_GUARD_TIME       (300)
#define LOC_RX_TIMEOUT          (600)


// #define LOC_GRID_LENGTH         (600)
// #define LOC_TX_START_TIME       (400)
// #define LOC_RX_GUARD_TIME       (200)
// #define LOC_RX_TIMEOUT          (400)

// // #define LOC_GRID_LENGTH         (300)
// #define LOC_GRID_LENGTH         (400)
// // #define LOC_TX_START_TIME       (200)
// #define LOC_TX_START_TIME       (300)
// #define LOC_RX_GUARD_TIME       (100)
// #define LOC_RX_TIMEOUT          (200)

// #define LATTICE_R		(5.0f)
#define LATTICE_R		(2.5f)
// #define LATTICE_R		(3.0f)


/* Public Types ---------------------------------------------------------------------------------- */
typedef struct {
	uint8_t    address[8];  /* Address of this neighbor */
	Vec3       loc;         /* The most recent estimate of this neighbor's 3D location */
	float      r;           /* Hyperspace coordinate radius */
	float      t;           /* Hyperspace coordinate theta */
	uint32_t   nbrhood;     /* The beacon slots that this neighbor is listening to */
	uint8_t    class;
} Neighbor;


/* Public Functions ------------------------------------------------------------------------------ */
void loc_init           (DW1000*, const uint8_t*);
void loc_start          (void);
void loc_start_root     (void);
void loc_stop           (void);
void loc_allow_beaconing(bool);
void loc_force_index    (int);
Vec3 loc_current        (void);
void loc_set_hypercoord (float, float);
void loc_dist_measured  (const uint8_t*, uint32_t);
void loc_slot           (TsSlot*);

/* Todo: Rename loc nbrs to loc beacons */
unsigned  loc_nbrs_size (void);
Neighbor* loc_nbrs      (unsigned);

bool     loc_is_beacon   (void);
unsigned loc_beacon_index(void);

// /* Loc Testing */
// void loc_start_tx(void);
// void loc_start_rx(void);
// void loc_tx_slot(TsSlot* ts);
// void loc_rx_slot(TsSlot* ts);


#ifdef __cplusplus
}
#endif

#endif // LOCATION_H
/******************************************* END OF FILE *******************************************/
