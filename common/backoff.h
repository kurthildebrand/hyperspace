/************************************************************************************************//**
 * @file		backoff.h
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
#ifndef BACKOFF_H
#define BACKOFF_H

#if __STDC_VERSION__ < 199901L
#error Compile with C99 or higher!
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* Includes -------------------------------------------------------------------------------------- */
#include <stdbool.h>
#include <stdint.h>


/* Public Types ---------------------------------------------------------------------------------- */
typedef struct {
	uint8_t t;
	uint8_t limit;
	uint8_t min;
	uint8_t max;
} Backoff;


/* Public Functions ------------------------------------------------------------------------------ */
inline void backoff_init   (Backoff*, uint8_t, uint8_t);
inline void backoff_reset  (Backoff*);
inline bool backoff_try    (Backoff*);
inline void backoff_success(Backoff*);
inline void backoff_hole   (Backoff*);
       void backoff_fail   (Backoff*);


/* backoff_init *********************************************************************************//**
 * @brief		Initializes a backoff timer and sets its max limit. */
inline void backoff_init(Backoff* b, uint8_t min, uint8_t max)
{
	/* Todo: validate min max */
	b->t     = 0;
	b->min   = min;
	b->max   = max;
	b->limit = min;
}


/* backoff_reset ********************************************************************************//**
 * @brief		Resets the timeout and limit of a backoff timer. */
inline void backoff_reset(Backoff* b)
{
	b->t     = 0;
	b->limit = b->min;
}


/* backoff_try **********************************************************************************//**
 * @brief		Decrements the backoff timer. Returns true if the backoff has timed out. Returns
 * 				false if the backoff timer is still counting down. */
inline bool backoff_try(Backoff* b)
{
	if(b->t)
	{
		b->t--;
		return false;
	}
	else
	{
		return true;
	}
}


/* backoff_hole *********************************************************************************//**
 * @brief		Decrements the backoff timer. */
inline void backoff_hole(Backoff* b)
{
	backoff_try(b);
}


/* backoff_success ******************************************************************************//**
 * @brief		Handles a successful backoff with no conflict. */
inline void backoff_success(Backoff* b)
{
	b->t     = 0;
	b->limit = b->min;
}


#ifdef __cplusplus
}
#endif

#endif // BACKOFF_H
/******************************************* END OF FILE *******************************************/
