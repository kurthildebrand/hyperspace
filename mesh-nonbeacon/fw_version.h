/************************************************************************************************//**
 * @file		fw_version.h
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
#ifndef FW_VERSION_H
#define FW_VERSION_H

#if __STDC_VERSION__ < 199901L
#error Compile with C99 or higher!
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* Public Macros --------------------------------------------------------------------------------- */
#define FW_MANUF		"kh"
#define FW_BOARD		"mesh-nonbeacon"
#define FW_VERSION		"1.0"


#ifdef __cplusplus
}
#endif

#endif // FW_VERSION_H
/******************************************* END OF FILE *******************************************/
