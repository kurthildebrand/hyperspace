/************************************************************************************************//**
 * @file		bayesian.c
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
#include <random/rand32.h>
#include <stdlib.h>
// #include <zephyr.h>

#include "bayesian.h"


/* Inline Function Instances --------------------------------------------------------------------- */
extern void  bayes_init   (Bayesian*, float);
extern void  bayes_update (Bayesian*);
extern void  bayes_success(Bayesian*);
extern void  bayes_hole   (Bayesian*);
extern void  bayes_fail   (Bayesian*);


/* bayes_try ************************************************************************************//**
 * @brief		Returns true if a packet should be transmitted using bayesian backoff. */
bool bayes_try(Bayesian* b)
{
	return bayes_rand() < (1.0f / b->v);
}


/* bayes_rand ***********************************************************************************//**
 * @brief		*/
float bayes_rand(void)
{
	return (float)(rand()) / (float)(RAND_MAX);
	// return (float)(sys_rand32_get()) / (float)(UINT32_MAX);
	// return (float)(sys_rand32_get()) / (UINT32_MAX + 1.0f);
}


/******************************************* END OF FILE *******************************************/
