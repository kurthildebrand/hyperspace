/************************************************************************************************//**
 * @file		iir.h
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
 * @brief		Simple single-pole infinite impulse response filter. The filter algorithm is
 *
 * 					y(n) = (alpha * y(n-1)) + (1 - alpha) * (new_value)
 *
 ***************************************************************************************************/
#ifndef IIR_H
#define IIR_H

#if __STDC_VERSION__ < 199901L
#error Compile with C99 or higher!
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* Public Types ---------------------------------------------------------------------------------- */
typedef struct {
	float alpha;
	float value;
} iir;


/* Public Functions ------------------------------------------------------------------------------ */
inline void  iir_init     (iir* f, float alpha, float value) { f->alpha = alpha; f->value = value; }
inline void  iir_set_alpha(iir* f, float alpha)              { f->alpha = alpha;                   }
inline void  iir_set_value(iir* f, float value)              { f->value = value;                   }
inline float iir_filter   (iir*, float);
inline float iir_value    (iir* f)                           { return f->value;                    }


/* iir_filter ***********************************************************************************//**
 * @brief		Filters a new value through the IIR filter and returns the filtered value. */
inline float iir_filter(iir* f, float new_value)
{
	return f->value = (f->alpha * f->value) + ((1.0f - f->alpha) * new_value);
}


#ifdef __cplusplus
}
#endif

#endif // IIR_H
/******************************************* END OF FILE *******************************************/
