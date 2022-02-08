/************************************************************************************************//**
 * @file		backoff.c
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
#include <stdlib.h>

#include "backoff.h"


/* Inline Function Instances --------------------------------------------------------------------- */
extern void backoff_init   (Backoff*, uint8_t, uint8_t);
extern void backoff_reset  (Backoff*);
extern bool backoff_try    (Backoff*);
extern void backoff_success(Backoff*);
extern void backoff_hole   (Backoff*);


/* backoff_fail *********************************************************************************//**
 * @brief		Handles a conflict by increasing the backoff time. */
void backoff_fail(Backoff* b)
{
	if(b->limit < b->max)
	{
		b->limit *= 2;
	}

	if(b->limit > 1)
	{
		/* Divide RAND_MAX into n equally sized slots */
		int r;
		int n = b->limit;
		int blocksize = (RAND_MAX) / n;

		/* Keep rolling r until it is within range */
		do {
			r = rand();
		} while(r >= blocksize * n);

		b->t = r / blocksize;
	}
}


/******************************************* END OF FILE *******************************************/
