/************************************************************************************************//**
 * @file		iir.c
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
 * @brief		Simple single-pole infinite impulse response filter.
 *
 ***************************************************************************************************/
#include "iir.h"


/* Inline Function Instances --------------------------------------------------------------------- */
extern void  iir_init     (iir*, float, float);
extern void  iir_set_alpha(iir*, float);
extern void  iir_set_value(iir*, float);
extern float iir_filter   (iir*, float);
extern float iir_value    (iir*);


/******************************************* END OF FILE *******************************************/
