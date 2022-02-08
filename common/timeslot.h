/************************************************************************************************//**
 * @file		timeslot.h
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
#ifndef TIMESLOT_H
#define TIMESLOT_H

#if __STDC_VERSION__ < 199901L
#error Compile with C99 or higher!
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* Includes -------------------------------------------------------------------------------------- */
#include <stdbool.h>
#include <stdint.h>
#include <zephyr.h>	/* Zephyr */

#include "linked.h"


/* Public Macros --------------------------------------------------------------------------------- */
/* TSCH specifies that ASN is stored in a 40 bit number. For various slot lengths:
 *
 * 		2^40 *  2500 us =  2748779069440000 us
 * 		2^40 * 10000 us = 10995116277760000 us
 *
 * The NRF52832 RTC is a 24 bit counter running at 32768 Hz which means that the RTC count wraps
 * every 512 s. Therefore, to wrap perfectly, the timeslot period should be multiples of:
 *
 * 		LCM (2^40 * 10000us, 512000000us) = 274877906944000000 us
 *
 * The calculation was done using 10ms time slots as 2.5ms time slots evenly divide into 10ms.
 */
#define TS_PERIOD	(274877906944000000ull)
// #define TS_PERIOD			(512000000ull * 36028797018ull)


/* Public Types ---------------------------------------------------------------------------------- */
typedef void (*TsPowerdown)(void);
typedef void (*TsPowerup)(void);

typedef struct TsSlotframe TsSlotframe;
typedef struct TsSlot TsSlot;

struct TsSlot {
	Link         node;
	// void*        neighbor;	/* Pointer to this slot's neighbor */
	TsSlotframe* slotframe;	/* Pointer to this slot's slotframe */
	uint16_t     index;		/* Slot index in the slotframe */
	// uint16_t     channel;	/* Slot channel offset */
	uint8_t      flags;		/* Tx/Rx/Shared/EB/Timekeeping flags */
	uint8_t      dropcount;	/* Count of times no communications were heard on this slot */
	uint8_t      count;
	// uint8_t      txslot;
	void (*handler)(struct TsSlot*);
	struct k_queue tx_queue;	/* Zephyr */
};


struct TsSlotframe {
	Link     node;			/* List sorted by slotframe id */
	uint16_t id;			/* Slotframe id */
	uint16_t numslots;		/* Number of slots in this slotframe */
	Link*    slots;			/* List of slots in this slotframe */
	TsSlot*  next;			/* Pointer to the next active slot in this slotframe */
};


typedef struct {
	volatile uint64_t time;		/* Variable to extend the bits of the RTC counter */
	uint64_t     tasn0;			/* Timestamp in us of ASN 0 */
	uint64_t     last_time;		/* Last active slot's timestamp */
	uint64_t     last_asn;		/* Last active slot's ASN */
	uint64_t     next_time;		/* Next active slot's timestamp */
	TsSlotframe* nextsf;		/* Pointer to the next slotframe (which points to the next slot) */
	Link*        slotframes;
	TsPowerdown  pwrdown_fn;
	TsPowerup    pwrup_fn;
	uint32_t     pwrdown_us;
	uint32_t     pwrup_us;
	// struct k_mutex  lock;
	// struct k_sem    sync;
	// struct k_thread thread;
} TsGrid;


/* Public Functions ------------------------------------------------------------------------------ */
void         usleep(uint32_t);

void         ts_init               (void);
void         ts_clear              (void);
void         ts_config_power_down  (TsPowerdown, uint32_t);
void         ts_config_power_up    (TsPowerup, uint32_t);
TsSlot*      ts_next               (void);
void         ts_lock               (void);
void         ts_unlock             (void);
uint64_t     ts_current_tstart     (void);
uint64_t     ts_current_toffset    (int32_t);
uint64_t     ts_current_asn        (void);
uint64_t     ts_time_now           (void);
uint64_t     ts_asn_now            (void);
void         ts_offset             (int32_t);
void         ts_sync               (uint64_t, uint64_t);

Link*        ts_slotframes         (void);
TsSlotframe* ts_slotframe_add      (uint16_t, uint16_t);
TsSlotframe* ts_slotframe_find     (uint16_t);
void         ts_slotframe_remove   (TsSlotframe*);
uint16_t     ts_slotframe_next_free(TsSlotframe*, uint16_t);
uint16_t     ts_slotframe_prev_free(TsSlotframe*, uint16_t);

Link*        ts_slots      (TsSlotframe*);
bool         ts_slot_add   (TsSlotframe*, uint8_t, uint16_t, void (*)(TsSlot*));
TsSlot*      ts_slot_find  (TsSlotframe*, uint16_t);
void         ts_slot_remove(TsSlot*);
// void         ts_slot_tx_append(TsSlot*, struct net_buf*);


#ifdef __cplusplus
}
#endif

#endif // TIMESLOT_H
/******************************************* END OF FILE *******************************************/
