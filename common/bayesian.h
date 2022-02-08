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
#ifndef BAYESIAN_H
#define BAYESIAN_H

#if __STDC_VERSION__ < 199901L
#error Compile with C99 or higher!
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* Includes -------------------------------------------------------------------------------------- */
#include <stdbool.h>
#include <stdint.h>

#include "calc.h"


/* Public Types ---------------------------------------------------------------------------------- */
typedef struct {
	float v;
	float limit;
} Bayesian;


/* Public Functions ------------------------------------------------------------------------------ */
inline void  bayes_init   (Bayesian*, float);
// inline void  bayes_update (Bayesian* b) { b->v = calc_max_f(b->v + 1.0f / M_E, 1.0f);         }
inline void  bayes_update (Bayesian* b) { b->v = calc_clamp_f(b->v + 1.0f / M_E, 1.0f, b->limit); }
       bool  bayes_try    (Bayesian*);
inline void  bayes_success(Bayesian* b) { b->v -= 1.0f;                                       }
inline void  bayes_hole   (Bayesian* b) { b->v -= 1.0f;                                       }
inline void  bayes_fail   (Bayesian* b) { if(b->v < b->limit) { b->v += 1.0f / (M_E - 2.0f); }}
       float bayes_rand   (void);


/* bayes_init ***********************************************************************************//**
 * @brief		Initializes bayesian backoff. */
inline void bayes_init(Bayesian* b, float limit)
{
	b->v     = 1.0f;
	b->limit = limit;
}


// /* bayes_try ************************************************************************************//**
//  * @brief		Returns true if a packet should be transmitted using bayesian backoff. */
// inline bool bayes_try(Bayesian* b)
// {
// 	return bayes_rand() < (1.0f / b->v);
// }


#ifdef __cplusplus
}
#endif

#endif // BAYESIAN_H
/******************************************* END OF FILE *******************************************/
