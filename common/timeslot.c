/************************************************************************************************//**
 * @file		timeslot.c
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
#include <nrf52.h>
#include <nrf52_bitfields.h>
#include <nrfx/hal/nrf_ppi.h>
#include <nrfx/hal/nrf_rtc.h>
#include <nrfx/hal/nrf_timer.h>

#include "calc.h"
#include "compare.h"
#include "pool.h"
#include "timeslot.h"
#include "utils.h"

/* Zephyr */
#include "logging/log.h"
// LOG_MODULE_REGISTER(ts, LOG_LEVEL_DBG);
// LOG_MODULE_REGISTER(ts, LOG_LEVEL_INF);
LOG_MODULE_REGISTER(ts, LOG_LEVEL_NONE);

// /* Non-Zephyr */
// #define LOG_DBG(...)
// #define LOG_INF(...)
// #define LOG_WRN(...)
// #define LOG_ERR(...)


/* Private Macros -------------------------------------------------------------------------------- */
// #define TS_THREAD_STACK_SIZE

/* The timeslot grid uses a 64-bit unsigned integer to keep track of microseconds. The period of the
 * RTC is 512s. Therefore, extend the period of the RTC to evenly fit into a 64-bit timestamp.
 * 2^64 / 512E6 = 36028797018.963968. Therefore, 512000000 * 36028797018 is the extended period. */
// #define TS_PERIOD			(512000000ull * 36028797018ull)
// #define TS_PERIOD			(18438809997803520000ull)
#define TS_CELL_LENGTH_US	(2500)
#define TS_NUM_SLOTS		(8)
#define TS_NUM_SLOTFRAMES	(8)


/* Private Types --------------------------------------------------------------------------------- */
/* Inline Function Instances --------------------------------------------------------------------- */
/* Private Functions ----------------------------------------------------------------------------- */
static void     ts_slot_handler    (void);
static void     ts_slotframe_update(TsSlotframe*, uint64_t);
static uint64_t ts_next_asn        (uint64_t, uint32_t, uint32_t);
static uint64_t ts_next_timeout    (uint64_t);
static void     ts_slot_handler    (void);
static uint64_t ts_time_to_asn     (uint64_t);
static uint64_t ts_update_next     (uint64_t);
static void     ts_slotframe_update(TsSlotframe*, uint64_t);
static uint64_t ts_next_asn        (uint64_t, uint32_t, uint32_t);
static uint64_t ts_next_timeout    (uint64_t);
static void     ts_set_timeout     (uint64_t);
static void     ts_set_power_up    (uint64_t);
static void     ts_try_power_down  (uint64_t);


/* Private Variables ----------------------------------------------------------------------------- */
TsGrid      tgrid;
TsSlotframe tgrid_slotframes[TS_NUM_SLOTFRAMES];
TsSlot      tgrid_slots[TS_NUM_SLOTS];
Pool        tgrid_slotframes_pool;
Pool        tgrid_slots_pool;


// void RTC0_IRQHandler(void)			/* Non-Zephyr */
ISR_DIRECT_DECLARE(ts_rtc_isr)	/* Zephyr */
{
	LOG_DBG("isr");

	if(nrf_rtc_event_check(NRF_RTC0, NRF_RTC_EVENT_OVERFLOW))
	{
		nrf_rtc_event_clear(NRF_RTC0, NRF_RTC_EVENT_OVERFLOW);

		tgrid.time = calc_addmod_u64(tgrid.time, 512000000ull, TS_PERIOD);
	}

	if(nrf_rtc_event_check(NRF_RTC0, NRF_RTC_EVENT_COMPARE_1))
	{
		nrf_rtc_event_clear(NRF_RTC0, NRF_RTC_EVENT_COMPARE_1);

		if(ts_next() && tgrid.pwrup_fn)
		{
			tgrid.pwrup_fn();
		}
	}

	return 0;	/* Zephyr */
}


// void TIMER1_IRQHandler(void)		/* Non-Zephyr */
ISR_DIRECT_DECLARE(ts_timer_isr)	/* Zephyr */
{
	LOG_DBG("isr");

	if(nrf_timer_event_check(NRF_TIMER1, NRF_TIMER_EVENT_COMPARE0))
	{
		nrf_timer_event_clear(NRF_TIMER1, NRF_TIMER_EVENT_COMPARE0);

		/* Clear radio timer timestamps at the start of the timeslot */
		NRF_TIMER0->CC[1] = 0;
		NRF_TIMER0->CC[2] = 0;

		ts_slot_handler();
	}

	ISR_DIRECT_PM();	/* Zephyr */
	return 1;			/* Zephyr */
}


/* usleep ***************************************************************************************//**
 * @brief		*/
void usleep(uint32_t delay)
{
	uint64_t start = ts_time_now();
	uint64_t end   = calc_addmod_u64(start, calc_round_u32(delay + 30, 31), TS_PERIOD);

	while(calc_wrapdiff_u64(end, ts_time_now(), TS_PERIOD) > 0) { }
}


/* ts_init **************************************************************************************//**
 * @brief		Initializes the timeslot grid. */
void ts_init(void)
{
	pool_init(&tgrid_slotframes_pool, tgrid_slotframes, TS_NUM_SLOTFRAMES, sizeof(TsSlotframe));
	pool_init(&tgrid_slots_pool, tgrid_slots, TS_NUM_SLOTS, sizeof(TsSlot));

	tgrid.time       = 0;
	tgrid.tasn0      = 0;
	tgrid.last_time  = 0;
	tgrid.last_asn   = 0;
	tgrid.next_time  = 0;
	tgrid.nextsf     = 0;
	tgrid.pwrdown_fn = 0;
	tgrid.pwrup_fn   = 0;
	tgrid.pwrdown_us = 0;
	tgrid.pwrup_us   = 0;
	linked_init(&tgrid.slotframes);

	NRF_RTC0->TASKS_STOP  = 1;
	NRF_RTC0->TASKS_CLEAR = 1;
	NRF_RTC0->EVTENSET    =
		(RTC_EVTENSET_COMPARE0_Enabled << RTC_EVTENSET_COMPARE0_Pos) |
		(RTC_EVTENSET_COMPARE1_Enabled << RTC_EVTENSET_COMPARE1_Pos) |
		(RTC_EVTENSET_OVRFLW_Enabled << RTC_EVTENSET_OVRFLW_Pos);
	NRF_RTC0->INTENSET    =
		(RTC_INTENSET_OVRFLW_Enabled << RTC_INTENSET_OVRFLW_Pos) |
		(RTC_INTENSET_COMPARE1_Enabled << RTC_INTENSET_COMPARE1_Pos);

	/* TIMER1 is used to increase the time resolution of the RTC */
	NRF_TIMER1->TASKS_STOP  = 1;
	NRF_TIMER1->TASKS_CLEAR = 1;
	NRF_TIMER1->INTENSET    = (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos);
	NRF_TIMER1->SHORTS      =
		(TIMER_SHORTS_COMPARE0_STOP_Enabled << TIMER_SHORTS_COMPARE0_STOP_Pos) |
		(TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos);
	NRF_TIMER1->MODE        = TIMER_MODE_MODE_Timer;
	NRF_TIMER1->BITMODE     = (TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos);
	NRF_TIMER1->PRESCALER   = 0;	/* Timer frequency = 16 MHz / (2^prescaler) = 16 MHz */

	NRF_TIMER0->TASKS_STOP  = 1;
	NRF_TIMER0->TASKS_CLEAR = 1;
	NRF_TIMER0->MODE        = TIMER_MODE_MODE_Timer;
	NRF_TIMER0->BITMODE     = (TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos);
	NRF_TIMER0->PRESCALER   = 4;	/* Timer frequency = 16 MHz / (2^prescaler) = 1 MHz */

	/* Todo: TIMER1 could be reused for the same function as TIMER0. */
	/* Ch 14: RTC0->EVENTS_COMPARE[0]   -+--> TIMER1->TASKS_CLEAR
	 *                                    \-> TIMER1->TASKS_START
	 * Ch 15: TIMER1->EVENTS_COMPARE[0] -+--> TIMER0->TASKS_CLEAR
	 *                                    \-> TIMER0->TASKS_START */
	nrf_ppi_channel_and_fork_endpoint_setup(
		NRF_PPI,
		NRF_PPI_CHANNEL14,
		nrf_rtc_event_address_get(NRF_RTC0, NRF_RTC_EVENT_COMPARE_0),
		nrf_timer_task_address_get(NRF_TIMER1, NRF_TIMER_TASK_CLEAR),
		nrf_timer_task_address_get(NRF_TIMER1, NRF_TIMER_TASK_START));

	nrf_ppi_channel_and_fork_endpoint_setup(
		NRF_PPI,
		NRF_PPI_CHANNEL15,
		nrf_timer_event_address_get(NRF_TIMER1, NRF_TIMER_EVENT_COMPARE0),
		nrf_timer_task_address_get(NRF_TIMER0, NRF_TIMER_TASK_CLEAR),
		nrf_timer_task_address_get(NRF_TIMER0, NRF_TIMER_TASK_START));

	nrf_ppi_channel_enable(NRF_PPI, NRF_PPI_CHANNEL14);
	nrf_ppi_channel_enable(NRF_PPI, NRF_PPI_CHANNEL15);

	// /* Non-Zephyr */
	// NVIC_SetPriority(RTC0_IRQn, 2);
	// NVIC_SetPriority(TIMER1_IRQn, 3);
	// NVIC_EnableIRQ(RTC0_IRQn);
	// NVIC_EnableIRQ(TIMER1_IRQn);

	/* Zephyr */
	IRQ_DIRECT_CONNECT(RTC0_IRQn,   2, ts_rtc_isr,   0);
	IRQ_DIRECT_CONNECT(TIMER1_IRQn, 3, ts_timer_isr, 0);
	irq_enable(RTC0_IRQn);
	irq_enable(TIMER1_IRQn);

	NRF_RTC0->TASKS_START = 1;
}


/* ts_clear *************************************************************************************//**
 * @brief		Removes all slotframes and slots from the timeslot grid. */
void ts_clear(void)
{
	ts_lock();

	TsSlotframe* sf;
	TsSlotframe* sf_next;

	LINKED_FOREACH_CONTAINER(tgrid.slotframes, sf, sf_next, node)
	{
		TsSlot* slot;
		TsSlot* s_next;

		LINKED_FOREACH_CONTAINER(sf->slots, slot, s_next, node)
		{
			linked_remove(&sf->slots, &slot->node);

			pool_release(&tgrid_slots_pool, slot);
		}

		pool_release(&tgrid_slotframes_pool, sf);
	}

	ts_unlock();
}


/* ts_config_power_down *************************************************************************//**
 * @brief		Configures the power down callback and power down duration in us. */
void ts_config_power_down(TsPowerdown fn, uint32_t us)
{
	tgrid.pwrdown_fn = fn;
	tgrid.pwrdown_us = us;
}


/* ts_config_power_up ***************************************************************************//**
 * @brief		Configures the power up callback and power up duration in us. */
void ts_config_power_up(TsPowerup fn, uint32_t us)
{
	tgrid.pwrup_fn = fn;
	tgrid.pwrup_us = us;
}


/* ts_slot_handler ******************************************************************************//**
 * @brief		*/
static void ts_slot_handler(void)
{
	uint64_t now = ts_time_now();
	TsSlot* slot = ts_next();

	if(slot)
	{
		LOG_DBG("start. time = %u", (uint32_t)ts_time_now());

		tgrid.last_time = tgrid.next_time;
		tgrid.last_asn  = ts_asn_now();
		tgrid.last_asn += (int64_t)(slot->index - tgrid.last_asn % slot->slotframe->numslots);

		NRF_P0->OUTSET = GPIO_OUTSET_PIN12_Set << GPIO_OUTSET_PIN12_Pos;
		slot->handler(slot);
		NRF_P0->OUTCLR = GPIO_OUTCLR_PIN12_Clear << GPIO_OUTCLR_PIN12_Pos;

		LOG_DBG("done. dur = %u", (uint32_t)(ts_time_now() - now));
	}

	uint64_t next = ts_update_next(ts_asn_now() + 1);
	ts_set_timeout(next);
	ts_set_power_up(next);
	ts_try_power_down(next);
}


/* ts_next **************************************************************************************//**
 * @brief		Returns the next active slot. */
TsSlot* ts_next(void)
{
	if(tgrid.nextsf && tgrid.nextsf->next)
	{
		return CONTAINER_OF(tgrid.nextsf->next, TsSlot, node);
	}
	else
	{
		return 0;
	}
}


/* ts_lock **************************************************************************************//**
 * @brief		*/
void ts_lock(void)
{
	LOG_DBG("lock");

	NRF_TIMER1->INTENCLR = TIMER_INTENCLR_COMPARE0_Enabled << TIMER_INTENCLR_COMPARE0_Pos;
}


/* ts_unlock ************************************************************************************//**
 * @brief		*/
void ts_unlock(void)
{
	LOG_DBG("unlock");

	NRF_TIMER1->INTENSET = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;
}


/* ts_current_tstart ****************************************************************************//**
 * @brief		Returns the start time of the current slot. */
uint64_t ts_current_tstart(void)
{
	return tgrid.last_time;
}


/* ts_current_toffset ***************************************************************************//**
 * @brief		Returns the start time of the current slot with an offset applied. */
uint64_t ts_current_toffset(int32_t offset)
{
	return calc_addmod_u64(tgrid.last_time, offset, TS_PERIOD);
}


/* ts_current_asn *******************************************************************************//**
 * @brief		Returns the current slot's ASN. */
uint64_t ts_current_asn(void)
{
	return tgrid.last_asn;
}


/* ts_time_now **********************************************************************************//**
 * @brief		Returns the grid's current local timestamp. */
uint64_t ts_time_now(void)
{
	uint64_t time;
	uint64_t counter;

	/* Read both time extension and RTC counter. Double check time extension to ensure that it is
	 * consistent with RTC counter. */
	do {
		time    = tgrid.time;
		counter = NRF_RTC0->COUNTER;
	} while(time != tgrid.time);

	/* Note: ts_grid_time_now only reads RTC counter. Therefore, returned time could be behind actual
	 * time by (1/(32768 Hz)) = 30.517 us = 31 us. */
	return time + (counter * 1000000ull / 32768ull);
}


/* ts_asn_now ***********************************************************************************//**
 * @brief		Returns the grid's current ASN. */
uint64_t ts_asn_now(void)
{
	return ts_time_to_asn(ts_time_now());
}


/* ts_time_to_asn *******************************************************************************//**
 * @brief		Converts timestamp to ASN. */
static uint64_t ts_time_to_asn(uint64_t time)
{
	/* Round up 1 RTC clock tick to ensure that the correct ASN is computed in the timeslot grid
	 * interrupt.
	 *
	 * Getting the current time from ts_grid_time_now only reads the RTC COUNTER register which is
	 * 1 RTC clock tick (1/(32768 Hz) = 30.517 us) behind the actual time. If the timeslot timer
	 * interrupt requests ts_grid_asn_now, the returned ASN could be miscalculated as the previous
	 * ASN. Therefore, rounding up 1 RTC clock tick produces the expected ASN without having to store
	 * additional state in ts grid. */
	return calc_submod_u64(time + 31, tgrid.tasn0, TS_PERIOD) / TS_CELL_LENGTH_US;
}


/* ts_offset ************************************************************************************//**
 * @brief		Applies asn offset in us to the timeslot grid. */
void ts_offset(int32_t offset)
{
	LOG_INF("offset %d. before: tasn0 = %d. last_time = %d",
		offset, (uint32_t)tgrid.tasn0, (uint32_t)tgrid.last_time);

	if(offset < 0)
	{
		tgrid.tasn0     = calc_submod_u64(tgrid.tasn0,     -offset, TS_PERIOD);
		tgrid.last_time = calc_submod_u64(tgrid.last_time, -offset, TS_PERIOD);
	}
	else
	{
		tgrid.tasn0     = calc_addmod_u64(tgrid.tasn0,      offset, TS_PERIOD);
		tgrid.last_time = calc_addmod_u64(tgrid.last_time,  offset, TS_PERIOD);
	}

	LOG_INF("after: tasn0 = %d. last_time = %d", (uint32_t)tgrid.tasn0, (uint32_t)tgrid.last_time);

	uint64_t next = ts_next_timeout(ts_asn_now() + 1);
	ts_set_timeout(next);
	ts_set_power_up(next);
}


/* ts_sync **************************************************************************************//**
 * @brief		Synchronizes the timeslot grid to the ASN which occurred at the given timestamp. */
void ts_sync(uint64_t asn, uint64_t tstamp)
{
	tgrid.tasn0     = calc_submod_u64(tstamp, asn * TS_CELL_LENGTH_US, TS_PERIOD);
	tgrid.last_asn  = asn;
	tgrid.last_time = tstamp;

	uint64_t next = ts_next_timeout(ts_asn_now() + 1);
	ts_set_timeout(next);
	ts_set_power_up(next);
}





// ----------------------------------------------------------------------------------------------- //
// Timeslot Slotframes                                                                             //
// ----------------------------------------------------------------------------------------------- //
/* ts_slotframes ********************************************************************************//**
 * @brief		Returns a list of slotframes in the timeslot grid. */
Link* ts_slotframes(void)
{
	return tgrid.slotframes;
}


/* ts_slotframe_add *****************************************************************************//**
 * @brief		Adds a slotframe to the timeslot grid.
 * @param[in]	id: the id of the slotframe.
 * @param[in]	numslots: total number of slots in the slotframe. */
TsSlotframe* ts_slotframe_add(uint16_t id, uint16_t numslots)
{
	/* Search for the spot to insert the new slotframe. Maintain a sorted list of slotframes, sorted
	 * by id. */
	TsSlotframe* pos;
	TsSlotframe* next;
	LINKED_FOREACH_CONTAINER(tgrid.slotframes, pos, next, node)
	{
		if(pos->id >= id) {
			break;
		}
	}

	/* Guarantee that slotframe id is unique */
	if(pos && pos->id == id) {
		return pos;
	}

	/* Try and allocate a new slotframe */
	TsSlotframe* ptr = pool_reserve(&tgrid_slotframes_pool);

	if(!ptr) {
		return 0;
	}

	ptr->id       = id;
	ptr->numslots = numslots;
	ptr->next     = 0;
	linked_init(&ptr->slots);
	linked_insert_after(&tgrid.slotframes, (pos ? &pos->node : 0), &ptr->node);

	// if(!pos)
	// {
	// 	linked_append(&tgrid.slotframes, &ptr->node);
	// }
	// else
	// {
	// 	linked_insert(&tgrid.slotframes, &pos->node, &ptr->node);
	// }

	return ptr;
}


/* ts_slotframe_find ****************************************************************************//**
 * @brief		Searches the timeslot grid for a slotframe with the specified id. Returns null if the
 * 				id was not found. */
TsSlotframe* ts_slotframe_find(uint16_t id)
{
	TsSlotframe* ptr;
	TsSlotframe* next;

	LINKED_FOREACH_CONTAINER(tgrid.slotframes, ptr, next, node) {
		if(ptr->id == id) {
			return ptr;
		}
	}

	return 0;
}


/* ts_slotframe_remove **************************************************************************//**
 * @brief		Removes the slotframe from the timeslot grid. */
void ts_slotframe_remove(TsSlotframe* sf)
{
	if(!sf)
	{
		return;
	}

	ts_lock();

	TsSlot* ptr;
	TsSlot* next;
	LINKED_FOREACH_CONTAINER(sf->slots, ptr, next, node) {
		linked_remove(&sf->slots, &ptr->node);
		pool_release(&tgrid_slots_pool, ptr);
	}

	linked_remove(&tgrid.slotframes, &sf->node);
	pool_release(&tgrid_slotframes_pool, sf);

	uint64_t tstamp = ts_update_next(ts_asn_now() + 1);
	ts_set_timeout(tstamp);
	ts_set_power_up(tstamp);

	ts_unlock();
}


/* ts_slotframe_next_free ***********************************************************************//**
 * @brief		Searches the slotframe for the next available slot after and including the specified
 * 				slot. */
uint16_t ts_slotframe_next_free(TsSlotframe* sf, uint16_t index)
{
	if(!sf || index >= sf->numslots) {
		return -1;
	}

	/* Find first slot greater than or equal to the desired slot */
	TsSlot* first;
	TsSlot* next;
	LINKED_FOREACH_CONTAINER(sf->slots, first, next, node) {
		if(first->index == index) {
			break;
		}
	}

	/* Slot > desired slot, or no slot >= desired slot, means desired slot is free */
	if(!first || first->index > index) {
		return index;
	}

	/* Find the next open slot by circularly iterating forwards through slotframe's slots while
	 * simultaneously incrementing desired slot. */
	TsSlot* ptr = first;
	do {
		index = calc_addmod_uint(index, 1, sf->numslots);
		ptr   = LINKED_NEXT_CONTAINER(&first->node, ptr, node);

		if(ptr->index != index) {
			return index;
		}
	} while(ptr != first);

	/* No free slot */
	return -1;
}


/* ts_slotframe_prev_free ***********************************************************************//**
 * @brief		Searches the slotframe for the previous available slot before and including the
 * 				specified slot. */
uint16_t ts_slotframe_prev_free(TsSlotframe* sf, uint16_t index)
{
	if(!sf || index >= sf->numslots) {
		return -1;
	}

	/* Find first slot less than or equal to the desired slot */
	TsSlot* first;
	TsSlot* next;
	LINKED_FOREACH_CONTAINER(sf->slots, first, next, node) {
		if(first->index <= index) {
			break;
		}
	}

	/* Slot < desired slot or no slot <= desired slot, means desired slot is free */
	if(!first || first->index < index) {
		return index;
	}

	/* Find the next open slot by circularly iterating backwards through slotframe's slots while
	 * simultaneously decrementing desired slot. */
	TsSlot* ptr = first;
	do {
		index = calc_submod_uint(index, 1, sf->numslots);
		ptr   = LINKED_PREV_CONTAINER(&first->node, ptr, node);

		if(ptr->index != index) {
			return index;
		}
	} while(ptr != first);

	/* No free slot */
	return -1;
}





// ----------------------------------------------------------------------------------------------- //
// Timeslot Slotframes                                                                             //
// ----------------------------------------------------------------------------------------------- //
/* ts_slots *************************************************************************************//**
 * @brief		Returns the slots in the specified slotframe. */
Link* ts_slots(TsSlotframe* sf)
{
	return sf->slots;
}


/* ts_slot_add **********************************************************************************//**
 * @brief		Adds a slot to the slotframe if the slot is available.
 * @param[in]	sf: the slotframe to add the slot to.
 * @param[in]	flags: the slot's flags.
 * @param[in]	slot: the new slot's index.
 * @param[in]	handler: handler function called when the slot becomes active. */
bool ts_slot_add(TsSlotframe* sf, uint8_t flags, uint16_t index, void (*handler)(TsSlot*))
{
	LOG_DBG("add %d to sf %d", index, sf->id);
	if(!sf)
	{
		LOG_ERR("no sf");
		return 0;
	}

	ts_lock();

	/* Find the position to insert the slot which maintains a sorted slotframe. Slots are sorted by
	 * index. Returns null if the slot with a specified index already exists in the grid. */
	TsSlot* pos;
	TsSlot* next;
	LINKED_FOREACH_CONTAINER(sf->slots, pos, next, node) {
		if(pos->index == index) {
			goto error;
		} else if(pos->index == index) {
			break;
		}
	}

	TsSlot* slot = pool_reserve(&tgrid_slots_pool);
	if(!slot)
	{
		goto error;
	}

	/* Initialize the slot */
	slot->slotframe = sf;
	slot->index     = index;
	slot->flags     = flags;
	slot->dropcount = 0;
	slot->count     = 0;
	slot->handler   = handler;
	k_queue_init(&slot->tx_queue);	/* Zephyr */
	linked_insert_after(&sf->slots, pos ? &pos->node : 0, &slot->node);

	uint64_t asn    = ts_asn_now() + 1;
	uint64_t tstamp = ts_update_next(asn);
	ts_set_timeout(tstamp);
	ts_set_power_up(tstamp);
	ts_unlock();
	return slot;

	error:
		LOG_ERR("error");
		ts_unlock();
		return 0;
}


/* ts_slot_find *********************************************************************************//**
 * @brief		Attempts to find a slot with the specified index in the slotframe. Returns null if
 * 				the slot was not found. */
TsSlot* ts_slot_find(TsSlotframe* sf, uint16_t index)
{
	if(sf)
	{
		TsSlot* ptr;
		TsSlot* next;
		LINKED_FOREACH_CONTAINER(sf->slots, ptr, next, node) {
			if(ptr->index == index) {
				return ptr;
			}
		}
	}

	return 0;
}


/* ts_slot_remove *******************************************************************************//**
 * @brief		Removes the slot from its slotframe. */
void ts_slot_remove(TsSlot* slot)
{
	LOG_DBG("remove %p", slot);

	if(!slot)
	{
		return;
	}

	ts_lock();

	linked_remove(&slot->slotframe->slots, &slot->node);
	pool_release(&tgrid_slots_pool, slot);

	if(tgrid.nextsf->next == slot)
	{
		tgrid.nextsf->next = 0;
		tgrid.nextsf = 0;

		uint64_t asn  = ts_asn_now() + 1;
		uint64_t next = ts_update_next(asn);
		ts_set_timeout(next);
		ts_set_power_up(next);
	}

	ts_unlock();
}





// ----------------------------------------------------------------------------------------------- //
// Timeslot Private Functions                                                                      //
// ----------------------------------------------------------------------------------------------- //
/* ts_update_next *******************************************************************************//**
 * @brief		Updates the timeslot grid for the next slot. */
static uint64_t ts_update_next(uint64_t asn)
{
	LOG_DBG("asn = %u", (uint32_t)asn);

	TsSlotframe* sf;
	TsSlotframe* sf_next;
	LINKED_FOREACH_CONTAINER(tgrid.slotframes, sf, sf_next, node) {
		ts_slotframe_update(sf, asn);
	}

	/* Slotframes may be empty. Iterate to find the first non-empty slotframe. */
	TsSlotframe* min;
	LINKED_FOREACH_CONTAINER(tgrid.slotframes, min, sf_next, node) {
		if(min->next) {
			break;
		}
	}

	if(min)
	{
		TsSlotframe* start = min;
		TsSlotframe* pos   = LINKED_NEXT_CONTAINER(&min->node, min, node);

		while(pos)
		{
			if(pos->next)
			{
				/* Compute the number of slots that need to elapse before a particular slot is active
				 * again. For example, if
				 *
				 * 		min = 2
				 * 		asn % min->numslots = 5
				 * 		min->numslots = 40,
				 *
				 * then min is active in 2-5 % 40 = -3 % 40 = 37 slots. */
				int32_t m = calc_submod_u32(min->next->index, asn % min->numslots, min->numslots);
				int32_t p = calc_submod_u32(pos->next->index, asn % pos->numslots, pos->numslots);

				LOG_DBG("m: min->next->index = %d, asn %% min->numslots = %d, min->numslots = %d",
					min->next->index, (uint32_t)asn % min->numslots, min->numslots);

				LOG_DBG("p: pos->next->index = %d, asn %% pos->numslots = %d, pos->numslots = %d",
					pos->next->index, (uint32_t)asn % pos->numslots, pos->numslots);

				/* Find the next slot. Sort by time till the slot is active. */
				if(m > p)
				{
					LOG_DBG("sf %d next %d >  sf %d next %d", min->id, m, pos->id, p);
					min = pos;
				}
				/* Then sort by slotframe id. Smaller slotframe ids take precedence. */
				else if(m == p && pos->id < min->id)
				{
					LOG_DBG("sf %d next %d == sf %d next %d", min->id, m, pos->id, p);
					min = pos;
				}
				else
				{
					LOG_DBG("sf %d next %d <  sf %d next %d", min->id, m, pos->id, p);
				}
			}

			pos = LINKED_NEXT_CONTAINER(&start->node, pos, node);
		}
	}

	tgrid.nextsf = min;

	if(tgrid.nextsf)
	{
		LOG_DBG("next = %d (%p)", tgrid.nextsf->next->index, tgrid.nextsf->next->handler);
	}
	else
	{
		LOG_DBG("next = null");
	}

	return ts_next_timeout(asn);
}


/* ts_slotframe_update **************************************************************************//**
 * @brief		Updates the slotframe for the next slot. */
static void ts_slotframe_update(TsSlotframe* sf, uint64_t asn)
{
	if(linked_empty(sf->slots))
	{
		sf->next = 0;
		return;
	}

	uint16_t sfasn = asn % sf->numslots;

	/* Slotframe is not empty but pointer to next slot hasn't been set. This happens when adding
	 * slots to an empty slotframe. Slotframe next pointer needs to be set to provide a terminator
	 * for the min finding loop. */
	if(!sf->next)
	{
		sf->next = LINKED_FIRST_CONTAINER(sf->slots, sf->next, node);
	}

	TsSlot* min = sf->next;
	TsSlot* pos = LINKED_NEXT_CONTAINER(&min->node, min, node);

	while(pos && pos != min)
	{
		if(calc_submod_uint(pos->index, sfasn, sf->numslots) <
		   calc_submod_uint(min->index, sfasn, sf->numslots))
		{
			min = pos;
		}

		pos = LINKED_NEXT_CONTAINER(&min->node, pos, node);
	}

	sf->next = min;

	LOG_DBG("sf %d next %d", sf->id, sf->next->index);
}


/* ts_next_asn **********************************************************************************//**
 * @brief		Computes the next asn where the given slot is active.
 * @param[in]	asn: the current asn.
 * @param[in]	slot: the slot offset.
 * @param[in]	numslots: the number of slots in the slotframe. */
static uint64_t ts_next_asn(uint64_t asn, uint32_t slot, uint32_t numslots)
{
	return asn + calc_submod_u64(slot, asn % numslots, numslots);
}


/* ts_next_timeout ******************************************************************************//**
 * @brief		Returns the timestamp in us of the start of the next timeslot. */
static uint64_t ts_next_timeout(uint64_t asn)
{
	if(tgrid.nextsf)
	{
		uint64_t next_asn = ts_next_asn(asn, tgrid.nextsf->next->index, tgrid.nextsf->numslots);
		uint64_t timeout  = calc_addmod_u64(tgrid.tasn0, next_asn * TS_CELL_LENGTH_US, TS_PERIOD);

		LOG_DBG("\r\n\tcurrent asn = %u\r\n\tnext asn = %u\r\n\tnext timeout = %u",
			(uint32_t)asn, (uint32_t)next_asn, (uint32_t)timeout);

		return timeout;
	}
	else
	{
		return -1ull;
	}
}


/* ts_set_timeout *******************************************************************************//**
 * @brief		Sets an interrupt for the timestamp when the next slot is active. Also trys to set an
 * 				interrupt to powerup for the slot. Interrupts at the next possible opportunity if
 * 				the timestamp is in the past. For NRF52832, the next possible opportunity is 2 RTC
 * 				tick from now */
static void ts_set_timeout(uint64_t tstamp)
{
	/* Exit quickly if the timeout has already been set */
	if(tstamp == tgrid.next_time)
	{
		return;
	}

	uint64_t now = ts_time_now() + 62;

	/* Immediately interrupt if new timeout is in the past and let tgrid handler deal with it */
	if(tstamp < now)
	{
		/* Timeout set late */
		LOG_DBG("too late");
		NRF_RTC0->CC[0]   = NRF_RTC0->COUNTER + 2;
		NRF_TIMER1->CC[0] = 1;
		tgrid.next_time   = now;
	}
	/* Otherwise, set the interrupt normally */
	else
	{
		tgrid.next_time = tstamp;

		/* Compute RTC counter value corresponding to the desired timestamp. RTC is a 24 bit timer
		 * at 32768 Hz, meaning the RTC wraps around every  2^24 / 32768 = 512 s. */
		uint64_t rtc  = (tstamp % 512000000) * 32768ull / 1000000ull;

		/* RTC has a resolution of 1/(32768 Hz) = 30.517 us. Compute the remaining time. */
		uint64_t rem  = (tstamp % 512000000) - (rtc * 1000000ull / 32768ull);

		/* Compute high resolution timer's counter value */
		uint64_t frac = rem * 16000000ull / 1000000ull;

		if(NRF_RTC0->CC[0] != rtc)
		{
			NRF_RTC0->CC[0] = rtc;
			NRF_TIMER1->CC[0] = frac == 0 ? 1 : frac;

			LOG_DBG("\r\n\trtc counter = %u\r\n\ttstamp = %u\r\n\tfrac = %u",
				NRF_RTC0->COUNTER, (uint32_t)rtc, (uint32_t)frac);
		}
		else
		{
			LOG_DBG("cc0 = rtc %u", (uint32_t)rtc);
		}
	}
}


/* ts_set_power_up ******************************************************************************//**
 * @brief		Set an interrupt to power up before the timeslot starting at the specified
 * 				timestamp. */
static void ts_set_power_up(uint64_t tstamp)
{
	uint64_t pwrup = calc_submod_u64(tstamp, tgrid.pwrup_us, TS_PERIOD);

	/* Power up uses RTC0 CC1 and is a coarse timestamp */
	pwrup = (pwrup % 512000000) * 32768ull / 1000000ull;

	/* Note: Power up will be skipped if the power up timestamp has passed */
	if(NRF_RTC0->CC[1] != pwrup)
	{
		NRF_RTC0->CC[1] = pwrup;
	}
}


/* ts_try_power_down ****************************************************************************//**
 * @brief		Trys to power down for the tstamp when the next slot is active. Does not power down
 * 				if there is not enough time to power down and power back up before the next
 * 				timeslot. */
static void ts_try_power_down(uint64_t tstamp)
{
	uint64_t now     = ts_time_now();
	uint64_t pwrdown = calc_submod_u64(tstamp, tgrid.pwrup_us + tgrid.pwrdown_us, TS_PERIOD);

	if(calc_wrapdiff_u64(pwrdown, now, TS_PERIOD) > 0 && tgrid.pwrdown_fn)
	{
		tgrid.pwrdown_fn();
	}
}


/******************************************* END OF FILE *******************************************/
