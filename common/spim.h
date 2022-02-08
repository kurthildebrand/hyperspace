/************************************************************************************************//**
 * @file		nrf52832_spim.h
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
#ifndef SPIM_H
#define SPIM_H

#if __STDC_VERSION__ < 199901L
#error Compile with C99 or higher!
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* Includes -------------------------------------------------------------------------------------- */
/* Public Macros --------------------------------------------------------------------------------- */
/* Public Types ---------------------------------------------------------------------------------- */
/* Spi Configuration
 *
 *           ---+                                                                   +---
 * SS           |                                                                   |
 *              +-------------------------------------------------------------------+
 *              .   +---+   +---+   +---+   +---+   +---+   +---+   +---+   +---+   .
 * CPOL = 0     .   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   .
 *           ---.---+   +---+   +---+   +---+   +---+   +---+   +---+   +---+   +---.---
 *           ---.---+   +---+   +---+   +---+   +---+   +---+   +---+   +---+   +---.---
 * CPOL = 1     .   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   .
 *              .   +---+   +---+   +---+   +---+   +---+   +---+   +---+   +---+   .
 *              .   .   .                                                           .
 *           --\./--.--\./-----\ /-----\ /-----\ /-----\ /-----\ /-----\ /-----\ /--.---
 * CPHA = 0     X   1   X   2   X   3   X   4   X   5   X   6   X   7   X   8   X   .
 * Leading   --/.\--.--/.\-----/ \-----/ \-----/ \-----/ \-----/ \-----/ \-----/ \--.---
 *              .   .   .                                                           .
 *           ---.--\./--.--\ /-----\ /-----\ /-----\ /-----\ /-----\ /-----\ /-----\./--
 * CPHA = 1     .   X   1   X   2   X   3   X   4   X   5   X   6   X   7   X   8   X
 * Trailing  ---.--/.\--.--/ \-----/ \-----/ \-----/ \-----/ \-----/ \-----/ \-----/.\--
 *
 * Mode CPOL CPHA
 * 0    0    0
 * 1    0    1
 * 2    1    0
 * 3    1    1
 */
#define SPI_CPOL_MASK   (0x1)
#define SPI_CPHA_MASK   (0x2)
#define SPI_ORDER_MASK  (0x4)
#define SPI_MASTER_MASK (0x8)

#define SPI_CPOL_0      (0x0)	/* Bit 0: Clock idles low, active high              */
#define SPI_CPOL_1      (0x2)	/* Bit 0: Clock idles high, active low              */
#define SPI_CPHA_0      (0x0)	/* Bit 1: Sample on leading edge, shift on trailing */
#define SPI_CPHA_1      (0x1)	/* Bit 1: Shift on leading edge, sample on trailing */
#define SPI_ORDER_MSB   (0x0)	/* Bit 2: Data shifted MSb first                    */
#define SPI_ORDER_LSB   (0x4)	/* Bit 2: Data shifted LSb first                    */
#define SPI_SLAVE       (0x0)
#define SPI_MASTER      (0x8)


typedef struct {
	uint8_t  mode;	/* Master/Slave, Bit Order, CPHA, CPOL */
	uint16_t miso;
	uint16_t mosi;
	uint16_t sck;
	uint16_t cs;
	uint32_t freq;
} Spi_Config;


typedef struct {
	void*    ptr;
	unsigned len;
} Spibuf;


typedef struct {
	void*         instance;
	Spi_Config    cfg;
	const Spibuf* txbufs;
	const Spibuf* rxbufs;
	unsigned      txcount;
	unsigned      rxcount;
} Spi;


/* Public Functions ------------------------------------------------------------------------------ */
void spim_init      (void);
void spim_power_up  (void);
void spim_power_down(void);
void spim_lock      (void);
void spim_unlock    (void);
void spim_set_freq  (uint32_t);
void spim_trx       (const Spibuf*, unsigned, const Spibuf*, unsigned);
void spim_cs        (bool);


#ifdef __cplusplus
}
#endif

#endif // SPIM_H
/******************************************* END OF FILE *******************************************/
