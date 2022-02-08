/************************************************************************************************//**
 * @file		dw1000.c
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
 * @ref			https://github.com/Decawave/dwm1001-examples/blob/68f86cf4a55dcda925df1d8e22ae0d8ebd41e4d1/deca_driver/deca_device.c
 * @brief
 * @desc		Default configuration on powerup:
 *
 * 					Channel       = 5
 * 					Preamble Code = 4
 * 					Data Rate     = 6.8 Mbps
 * 					PRF           = 16 MHz
 * 					Preamble      = 128 symbols		(IEEE std: 16, 64, 1024, 4096)
 * 					LDERUNE       = Enabled			(Note: LDELOAD on DW1000 initialization)
 *
 * 					DW1000 GPIO5  = Open Circuit (CPOL 0)
 * 					DW1000 GPIO6  = Open Circuit (CPHA 0)
 *
 * 					GPIO0/RXOKLEN         = GPIO0
 * 					GPIO1/SFDLED          = GPIO1
 * 					GPIO2/RXLED           = GPIO2
 * 					GPIO3/TXLED           = GPIO3
 * 					GPIO4/EXTPA           = GPIO4
 * 					GPIO5/EXTXE/SPIPHA    = GPIO5
 * 					GPIO6/EXTRXE/SPIPOL   = GPIO6
 * 					SYNC/GPIO7            = SYNC
 * 					IRQ/GIPO8             = IRQ
 *
 * 					Smart TX power        = On
 * 					Sniff Mode            = Off
 * 					Frame Wait Timeout    = Off (RXWTOE)
 * 					SFD Detection Timeout = Off (DRX_SFDTOC)
 *
 * 					RXAUTR                   = Off
 * 					FFEN                     = Off
 * 					DIS_DRXB                 = Off
 * 					AUTOACK                  = Off
 * 					Automatic CRC Generation = On
 * 					CRC LFSR                 = 0x0000 (FCS_INIT2F)
 *
 * 					External Synchronization = Off
 * 					External PA              = Off
 *
 *				Default Configurations that should be modified:
 *
 * 					AGC_TUNE1  = 0x8870
 * 					AGC_TUNE2  = 0x2502A907
 * 					DRX_TUNE2  = 0x311A002D
 * 					NTM        = 0xD
 * 					LDF_CFG2   = 0x1607
 * 					TX_POWER   = 0x0E082848
 * 					RF_TXCTRL  = 0x001E3FE3
 * 						RF_TXCTRL for different channels
 * 						TX Channel	RF_TXCTRL
 * 						1			0x00005C40
 * 						2			0x00045CA0
 * 						3			0x00086CC0
 * 						4			0x00045C80
 * 						5			0x001E3FE3
 * 						7			0x001E7DE0
 * 					TC_PGDELAY = 0xC0
 * 					FS_PLLTUNE = 0xBE
 * 					LDELOAD    = set
 * 						Register accesses required to load LDE microcode
 * 						Step	Instruction			Register				Length	Data (Write/Read)
 * 													Address					(Bytes)
 * 						L-1		Write Sub-Register	0x36:00 (PMSC_CTRL0)	2		0x0301
 * 						L-2		Write Sub-Register	0x2D:06 (OTP_CTRL)		2		0x8000
 * 								Wait 150 μs
 * 						L-3		Write Sub-Register	0x36:00 (PMSC_CTRL0)	2		0x0200
 * 					LDOTUNE    = load LDOTUNE_CAL from OTP if it has been set
 *
 * 				Message Transmission
 * 				--------------------
 * 				+------> IDLE State
 * 				|        |   Write TX Data to Data buffer
 * 				|        v   Configure TX parameters
 * 				|        TX START?
 * 				|        |
 * 				|        v
 * 				|+-----> TRANSMIT State
 * 				||       |   Transmit Message
 * 				||       v
 * 				|+-no--+ TX COMPLETE?
 * 				|        |
 * 				|        v
 * 				+--no--+ AUTOSLEEP?
 * 				         |
 * 				         v
 * 				         SLEEP State
 *
 * 				Message Reception
 * 				-----------------
 * 				+------> IDLE State
 * 				|        |   Configure RX parameters
 * 				|        v
 * 				|        RX START?
 * 				|        |
 * 				|        v
 * 				|        RECEIVE State
 * 				|        |
 * 				|        +---+-------------> Search for Preamble
 * 				|            |               |
 * 				|            no              |
 * 				|            |               v
 * 				+--yes-- Preamble <-----no-- Preamble Detected?
 * 				|        Detection           |
 * 				|        Timeout?            v
 * 				|                            Acquire Preamble
 * 				|                            |
 * 				|                            v
 * 				|            +-------------> Acquire SFD
 * 				|            |               |
 * 				|            no              |
 * 				|            |               v
 * 				+--yes-- SFD Timeout? <-no-- Preamble Acquisition Complete?
 * 				|                            |
 * 				|                            v
 * 				|            +-------------> Acquire Data
 * 				|            |               |
 * 				|            no              |
 * 				|            |               v
 * 				+--yes-- Frame Wait <---no-- Frame Received?
 * 				|        Timeout?            |
 * 				|            ^               |
 * 				|            |               v
 * 				|            +----------no-- RX Complete?
 * 				|                            |
 * 				|                            v
 * 				+-----------------------no-- AUTOSLEEP?
 * 				                             |
 * 				                             v
 * 				                             SLEEP State
 *
 * 				SPI Transfers
 *
 * 					    7       6       5       4       3       2       1       0
 * 					+-------+-------+-------+-------+-------+-------+-------+-------+
 * 					| R/W   | Sub   | 6-bit access address                          |
 * 					+-------+-------+-------+-------+-------+-------+-------+-------+
 * 					| Ext   | 7 bits sub address                                    |
 * 					+-------+-------+-------+-------+-------+-------+-------+-------+
 * 					| 8 Bits Sub Address (MSB bits [14:7])                          |
 * 					+-------+-------+-------+-------+-------+-------+-------+-------+
 * 					| Data                                                          |
 * 					+-------+-------+-------+-------+-------+-------+-------+-------+
 *
 * 					R/W:
 * 						0 = Read
 * 						1 = Write
 *
 * 					Sub:
 * 						0 = No sub address
 * 						1 = Sub address present
 *
 * 					Ext:
 * 						0 = 1 byte sub address
 * 						1 = 2 byte sub address
 *
 ***************************************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include <nrfx/hal/nrf_gpio.h>

#include "byteorder.h"
#include "dw1000.h"
#include "spim.h"
#include "timeslot.h"


/* Inline Function Instances --------------------------------------------------------------------- */
extern unsigned          dw1000_channel        (const DW1000*);
extern DW1000_Data_Rate  dw1000_data_rate      (const DW1000*);
extern DW1000_Prf        dw1000_prf            (const DW1000*);
extern DW1000_Pac        dw1000_pac            (const DW1000*);
extern DW1000_Pre_Length dw1000_preamble_length(const DW1000*);
extern unsigned          dw1000_tx_code        (const DW1000*);
extern unsigned          dw1000_rx_code        (const DW1000*);
extern uint16_t          dw1000_sfd_timeout    (const DW1000*);
extern uint64_t          dw1000_ns_to_ticks    (uint64_t);
extern uint64_t          dw1000_us_to_ticks    (uint64_t);
extern uint64_t          dw1000_ms_to_ticks    (uint64_t);


/* Private Functions ----------------------------------------------------------------------------- */
static void     dw1000_ldeload            (DW1000*);
static void     dw1000_set_xtal_trim      (DW1000*, uint8_t);
static void     dw1000_set_channel        (DW1000*, unsigned);
static void     dw1000_set_pac_prf        (DW1000*, DW1000_Pac, DW1000_Prf);
static void     dw1000_set_data_rate      (DW1000*, DW1000_Data_Rate);
static void     dw1000_set_tx_code        (DW1000*, unsigned);
static void     dw1000_set_rx_code        (DW1000*, unsigned);
static void     dw1000_set_preamble_length(DW1000*, DW1000_Pre_Length);
static void     dw1000_set_sfd_timeout    (DW1000*, uint16_t);
static void     dw1000_enable_clock       (DW1000*, uint16_t, uint16_t);
static uint32_t dw1000_otp_read           (DW1000*, uint32_t);

static void     dw1000_spi_write8 (DW1000*, uint8_t, uint16_t, uint8_t);
static void     dw1000_spi_write16(DW1000*, uint8_t, uint16_t, uint16_t);
static void     dw1000_spi_write32(DW1000*, uint8_t, uint16_t, uint32_t);
static uint8_t  dw1000_spi_read8  (DW1000*, uint8_t, uint16_t);
static uint16_t dw1000_spi_read16 (DW1000*, uint8_t, uint16_t);
static uint32_t dw1000_spi_read32 (DW1000*, uint8_t, uint16_t);
static void     dw1000_spi_write  (DW1000*, uint8_t, uint16_t, const void*, unsigned);
static void     dw1000_spi_read   (DW1000*, uint8_t, uint16_t, void*, unsigned);

static unsigned dw1000_set_write_header(void*, uint8_t, uint16_t);
static unsigned dw1000_set_read_header (void*, uint8_t, uint16_t);
static unsigned dw1000_set_header      (void*, uint8_t, uint16_t, bool);
// static unsigned dw1000_header_length   (const void*);


/* Private Variables ----------------------------------------------------------------------------- */
// /* 0x1A – ACK_RESP_T. Indexed by dw1000_ack_tim[DW1000_Data_Rate] */
// static const uint8_t dw1000_ack_tim[] = {
// 	0,	/* 110 kbps */
// 	2,	/* 850 kbps */
// 	3,	/* 6.8 Mbps */
// };

// /* 0x1E – TX_POWER. Indexed by dw1000_tx_power[DIS_STXP = 0/1][Channel - 1][DW1000_Prf] */
// static const uint32_t dw1000_tx_power[2][7][2] = {
// 	/* Transmit Power Control for DIS_STXP = 0
// 	 * 16 MHz,      64 MHz */
// 	{ { 0x15355575, 0x07274767 },	/* Ch. 1 */
// 	  { 0x15355575, 0x07274767 },	/* Ch. 2 */
// 	  { 0x0F2F4F6F, 0x2B4B6B8B },	/* Ch. 3 */
// 	  { 0x1F1F3F5F, 0x3A5A7A9A },	/* Ch. 4 */
// 	  { 0x0E082848, 0x25456585 },	/* Ch. 5 */
// 	  { 0x00000000, 0x00000000 },	/* Ch. 6 */
// 	  { 0x32527292, 0x5171B1D1 } },	/* Ch. 7 */

// 	/* Transmit Power Control for DIS_STXP = 1
// 	 * 16 MHz,      64 MHz */
// 	{ { 0x75757575, 0x67676767 },	/* Ch. 1 */
// 	  { 0x75757575, 0x67676767 },	/* Ch. 2 */
// 	  { 0x6F6F6F6F, 0x8B8B8B8B },	/* Ch. 3 */
// 	  { 0x5F5F5F5F, 0x9A9A9A9A },	/* Ch. 4 */
// 	  { 0x48484848, 0x85858585 },	/* Ch. 5 */
// 	  { 0x00000000, 0x00000000 },	/* Ch. 6 */
// 	  { 0x92929292, 0xD1D1D1D1 } },	/* Ch. 7 */
// };

/* 0x23:04 – AGC_TUNE1. Indexed by dw1000_agc_tune1[DW1000_Prf] */
static const uint16_t dw1000_agc_tune1[] = {
	0x8870,	/* PRF 16 MHz */
	0x889B,	/* PRF 64 MHz */
};

/* 0x23:0C – AGC_TUNE2. */
static const uint32_t dw1000_agc_tune2[] = {
	0x2502A907,
};

// /* 0x23:12 – AGC_TUNE3. */
// static const uint16_t dw1000_agc_tune3[] = {
// 	0x0035,
// };

/* 0x27:02 – DRX_TUNE0b. Indexed by
 * dw1000_drx_tune0b[DW1000_Data_Rate][Standard (0) / Non-Standard (1) SFD] */
static const uint16_t dw1000_drx_tune0b[3][2] = {
	{ 0x000A, 0x0016 },	/* 100 kbps: Standard SFD, Non-Standard SFD */
	{ 0x0001, 0x0006 },	/* 850 kbps: Standard SFD, Non-Standard SFD */
	{ 0x0001, 0x0002 },	/* 6.8 Mbps: Standard SFD, Non-Standard SFD */
};

/* 0x27:04 – DRX_TUNE1a. Indexed by dw1000_drx_tune1a[DW1000_Prf] */
static const uint16_t dw1000_drx_tune1a[] = {
	0x0087,	/* PRF 16 MHz */
	0x008D,	/* PRF 64 MHz */
};

/* 0x27:06 – DRX_TUNE1b. */
static const uint16_t dw1000_drx_tune1b[] = {
	0x0064, /* Preamble lengths > 1024 symbols, for 110 kbps operation */
	0x0020, /* Preamble lengths 128 to 1024 symbols, for 850 kbps and 6.8 Mbps operation */
	0x0010, /* Preamble length = 64 symbols, for 6.8 Mbps operation */
};

/* 0x27:08 – DRX_TUNE2. Indexed by dw1000_drx_tune2[DW1000_Pac][DW1000_Prf] */
static const uint32_t dw1000_drx_tune2[4][2] = {
	{ 0x311A002D, 0x313B006B },	/* PAC size 8:  16 MHz, 64 MHz */
	{ 0x331A0052, 0x333B00BE },	/* PAC size 16: 16 MHz, 64 MHz */
	{ 0x351A009A, 0x353B015E },	/* PAC size 32: 16 MHz, 64 MHz */
	{ 0x371A011D, 0x373B0296 },	/* PAC size 64: 16 MHz, 64 MHz */
};

/* 0x27:26 – DRX_TUNE4H. */
static const uint16_t dw1000_drx_tune4h[] = {
	0x0010, /* 64 Symbols: Expected Receive Preamble Length */
	0x0028, /* 128 or greater: Expected Receive Preamble Length */
};

/* Sub-Register 0x28:0B – RF_RXCTRLH. Indexed by dw1000_rf_rxctrlh[Channel - 1] */
static const uint16_t dw1000_rf_rxctrlh[] = {
	0xD8, /* Ch. 1. Narrow Bandwidth */
	0xD8, /* Ch. 2. Narrow Bandwidth */
	0xD8, /* Ch. 3. Narrow Bandwidth */
	0xBC, /* Ch. 4. Wide Bandwidth */
	0xD8, /* Ch. 5. Narrow Bandwidth */
	0x00, /* UNUSED */
	0xBC, /* Ch. 6. Wide Bandwidth */
};

/* 0x28:0C– RF_TXCTRL. Indexed by dw1000_rf_txctrl[Channel - 1] */
static const uint32_t dw1000_rf_txctrl[] = {
 	0x00005C40,	/* Ch. 1 */
 	0x00045CA0,	/* Ch. 2 */
 	0x00086CC0,	/* Ch. 3 */
 	0x00045C80,	/* Ch. 4 */
 	0x001E3FE3,	/* Ch. 5 */
 	0x00000000,	/* UNUSED */
 	0x001E7DE0,	/* Ch. 7 */
};

// /* 0x2A:0B – TC_PGDELAY. Indexed by dw1000_rf_txctrl[Channel - 1] */
// static const uint8_t dw1000_tc_pgdelay[] = {
// 	0xC9,	/* Ch. 1 */
// 	0xC2,	/* Ch. 2 */
// 	0xC5,	/* Ch. 3 */
// 	0x95,	/* Ch. 4 */
// 	0xC0,	/* Ch. 5 */
// 	0x00,	/* UNUSED */
// 	0x93,	/* Ch. 7 */
// };

/* 0x2B:07 – FS_PLLCFG. Indexed by dw1000_fs_pllcfg[Channel - 1] */
static const uint32_t dw1000_fs_pllcfg[] = {
	0x09000407, /* Ch. 1 */
	0x08400508, /* Ch. 2 */
	0x08401009, /* Ch. 3 */
	0x08400508, /* Ch. 4 */
	0x0800041D, /* Ch. 5 */
	0x00000000, /* UNUSED */
	0x0800041D, /* Ch. 7 */
};

/* 0x2B:0B – FS_PLLTUNE. Indexed by dw1000_rf_txctrl[Channel - 1] */
static const uint8_t dw1000_fs_plltune[] = {
	0x1E,	/* Ch. 1 */
	0x26,	/* Ch. 2 */
	0x56,	/* Ch. 3 */
	0x26,	/* Ch. 4 */
	0xBE,	/* Ch. 5 */
	0x00,	/* UNUSED */
	0xBE,	/* Ch. 7 */
};

/* 0x2E:1806 – LDE_CFG2. Indexed by dw1000_agc_tune1[DW1000_Prf] */
static const uint16_t dw1000_lde_cfg2[] = {
	0x1607,	/* PRF 16 MHz */
	0x0607,	/* PRF 64 MHz */
};

/* Indexed dw1000_lde_repc[Preamble Code - 1] */
static const uint16_t dw1000_lde_repc[] = {
	/* These values apply to 850 kbps and 6.8 Mbps data rates only. When operating at 110 kbps the
	 * values here have to be divided by 8 before programming into Sub-Register
	 * 0x2E:2804 – LDE_REPC. */
	0x5998, /* Preamble Code 1 */
	0x5998, /* Preamble Code 2 */
	0x51EA, /* Preamble Code 3 */
	0x428E, /* Preamble Code 4 */
	0x451E, /* Preamble Code 5 */
	0x2E14, /* Preamble Code 6 */
	0x8000, /* Preamble Code 7 */
	0x51EA, /* Preamble Code 8 */
	0x28F4, /* Preamble Code 9 */
	0x3332, /* Preamble Code 10 */
	0x3AE0, /* Preamble Code 11 */
	0x3D70, /* Preamble Code 12 */
	0x3AE0, /* Preamble Code 13 */
	0x35C2, /* Preamble Code 14 */
	0x2B84, /* Preamble Code 15 */
	0x35C2, /* Preamble Code 16 */
	0x3332, /* Preamble Code 17 */
	0x35C2, /* Preamble Code 18 */
	0x35C2, /* Preamble Code 19 */
	0x3AE0, /* Preamble Code 21 */
	0x47AE, /* Preamble Code 20 */
	0x3850, /* Preamble Code 22 */
	0x30A2, /* Preamble Code 23 */
	0x3850, /* Preamble Code 24 */
};


/* dw1000_init **********************************************************************************//**
 * @brief		Initializes the DW1000. */
void dw1000_init(DW1000* dw1000, uint32_t gpio_int_pin)
{
	/* Power up defaults
	 * Channel       = 5
 	 * Preamble Code = 4
 	 * Data Rate     = 6.8 Mbps
 	 * PRF           = 16 MHz
 	 * Preamble      = 128 symbols		(IEEE std: 16, 64, 1024, 4096)
 	 * LDERUNE       = Enabled			(Note: LDELOAD on DW1000 initialization) */
	dw1000->gpio_int_pin      = gpio_int_pin;
	dw1000->channel           = 5;
	dw1000->data_rate         = DW1000_DR_6800KBPS;
	dw1000->prf               = DW1000_PRF_16MHZ;
	dw1000->pac               = DW1000_PAC_SIZE_8;
	dw1000->preamble_length   = 128;
	dw1000->tx_code           = 4;
	dw1000->rx_code           = 4;
	dw1000->sfd_timeout       = 4096 + 64 + 1;
	nrf_gpio_cfg_input(gpio_int_pin, NRF_GPIO_PIN_NOPULL);
	spim_init();
	spim_set_freq(2000000);

	/* Wake up the DW1000 */
	dw1000_wakeup_by_cs(dw1000);
	usleep(2000);

	/* Make sure the device is completely reset before starting initialisation */
	dw1000_soft_reset(dw1000);

	/* Force system clock to be the 19.2 MHz XTI clock. This is required to ensure that the values
	 * read by dw1000_otp_read are reliable. */
	dw1000_enable_clock(dw1000, DW1000_PMSC_CTRL0_SYSCLKS_19_2_MHZ, DW1000_PMSC_CTRL0_SYSCLKS_MASK);

	/* Configure the CPLL lock detect */
	dw1000_spi_write8(dw1000, DW1000_EXT_SYNC, DW1000_EC_CTRL_OFFSET, DW1000_EXT_SYNC_PLLLDT);

	/* Load LDO tune from OTP */
	uint32_t ldo_tune = dw1000_otp_read(dw1000, DW1000_OTP_LDO_TUNE_ADDR);
	if((ldo_tune & 0xFF) != 0)
	{
		/* Set load LDE kick bit */
		dw1000_spi_write8(dw1000, DW1000_OTP_IF, DW1000_OTP_SF_OFFSET, DW1000_OTP_SF_LDO_KICK);

		/* LDO tune must be kicked at wake-up */
		dw1000->on_wake |= DW1000_AON_WCFG_ONW_LLD0;
	}

	uint32_t ant_delay = dw1000_otp_read(dw1000, DW1000_OTP_ANT_DELAY_ADDR);
	dw1000->ant_delay  = ant_delay & 0xFFFF;

	/* Read OTP revision and XTAL trim */
	uint32_t xtrim = dw1000_otp_read(dw1000, DW1000_OTP_XTRIM_ADDR) & 0xFFFF;
	dw1000->otp_revision = (xtrim >> 8) & 0xFF;
	dw1000->xtal_trim    = (xtrim & 0x1F) ? (xtrim & 0x1F) : 0x10;
	dw1000_set_xtal_trim(dw1000, dw1000->xtal_trim);

	/* Load leading edge detect microcode. Indicate that LDE microcode must be loaded at wakeup. */
	dw1000_ldeload(dw1000);
	dw1000->on_wake |= DW1000_AON_WCFG_ONW_LLDE;

	/* Enable all clocks */
	uint16_t clocks =
		DW1000_PMSC_CTRL0_SYSCLKS_AUTO |
		DW1000_PMSC_CTRL0_RXCLKS_AUTO |
		DW1000_PMSC_CTRL0_TXCLKS_AUTO;

	dw1000_enable_clock(dw1000, clocks, 0x1FF);

	/* AON_CFG1 must be cleared to ensure proper operation of the DW1000 in DEEPSLEEP mode  */
	dw1000_spi_write8(dw1000, DW1000_AON, DW1000_AON_CFG1_OFFSET, 0x00);

	/* Store copy of SYS_CFG register */
	dw1000->sys_cfg = dw1000_spi_read32(dw1000, DW1000_SYS_CFG, DW1000_NO_SUB_ADDR);

	spim_set_freq(8000000);
}


/* dw1000_lock **********************************************************************************//**
 * @brief		Locks the DW1000 radio. */
void dw1000_lock(DW1000* dw1000)
{
	(void)(dw1000);
	spim_lock();
}


/* dw1000_unlock ********************************************************************************//**
 * @brief		Unlocks the DW1000 radio. */
void dw1000_unlock(DW1000* dw1000)
{
	(void)(dw1000);
	spim_unlock();
}


/* dw1000_soft_reset ****************************************************************************//**
 * @brief		Performs a soft reset of the dw1000. */
void dw1000_soft_reset(DW1000* dw1000)
{
	/* Force system clock to be the 19.2 MHz XTI clock */
	dw1000_enable_clock(dw1000, DW1000_PMSC_CTRL0_SYSCLKS_19_2_MHZ, DW1000_PMSC_CTRL0_SYSCLKS_MASK);

	/* Disable PMSC ctrl of RF and RX clk blocks */
	dw1000_spi_write16(dw1000, DW1000_PMSC, DW1000_PMSC_CTRL1_OFFSET, 0x0000);

	/* Clear any AON auto download bits (as reset will trigger AON download) */
	dw1000_spi_write16(dw1000, DW1000_AON, DW1000_AON_WCFG_OFFSET, 0x0000);

	/* Clear the wake-up configuration */
	// dw1000_spi_write16(dw1000, DW1000_AON, DW1000_AON_CFG0_OFFSET, 0x0000);
	dw1000_spi_write8(dw1000, DW1000_AON, DW1000_AON_CFG0_OFFSET, 0x00);

	/* Upload the new configuration */
	dw1000_spi_write8(dw1000, DW1000_AON, DW1000_AON_CTRL_OFFSET, 0x00);
	dw1000_spi_write8(dw1000, DW1000_AON, DW1000_AON_CTRL_OFFSET, 0x02);

	/* Reset IC TX, RX, Host Interface, and the PMSC itself by writing all zeros to SOFTRESET */
	dw1000_spi_write8(dw1000, DW1000_PMSC, DW1000_PMSC_CTRL0_OFFSET + 3, 0x00);

	/* Wait ~10 us */
	usleep(10);

	/* Clear reset */
	dw1000_spi_write8(
		dw1000,
		DW1000_PMSC,
		DW1000_PMSC_CTRL0_OFFSET + 3,
		DW1000_PMSC_CTRL0_SOFTRESET >> DW1000_PMSC_CTRL0_SOFTRESET_SHIFT);
}


/* dw1000_ldeload *******************************************************************************//**
 * @brief		Loads LDE microcode. */
static void dw1000_ldeload(DW1000* dw1000)
{
	/* LDELOAD    = set
	 * 	Register accesses required to load LDE microcode
	 * 	Step	Instruction			Register				Length	Data (Write/Read)
	 * 								Address					(Bytes)
	 * 	L-1		Write Sub-Register	0x36:00 (PMSC_CTRL0)	2		0x0301
	 * 	L-2		Write Sub-Register	0x2D:06 (OTP_CTRL)		2		0x8000
	 * 			Wait 150 μs
	 * 	L-3		Write Sub-Register	0x36:00 (PMSC_CTRL0)	2		0x0200 */
	/* L-1 */
	dw1000_enable_clock(dw1000, 0x0301, 0xFFFF);

	/* L-2 */
	dw1000_spi_write16(dw1000, DW1000_OTP_IF, DW1000_OTP_CTRL_OFFSET, DW1000_OTP_CTRL_LDELOAD);

	/* Wait for 150us */
	usleep(150);

	/* L-3 */
	dw1000_enable_clock(dw1000, 0x0200, 0xFFFF);
}


/* dw1000_set_xtal_trim *************************************************************************//**
 * @brief		Adjusts the crystal frequency. Value is specified from 0x00 to 0x1F (31 steps) with a
 * 				resolution of ~1.5 ppm per step. */
static void dw1000_set_xtal_trim(DW1000* dw1000, uint8_t trim)
{
	/* TODO: bit definitions in dw1000.h */
	trim = (3 << 5) | (trim & 0x1F);
	dw1000_spi_write8(dw1000, DW1000_FS_CTRL, DW1000_FS_XTALT_OFFSET, trim);
}


/* dw1000_reconfig ******************************************************************************//**
 * @brief		Reconfigures the DW1000. */
bool dw1000_reconfig(DW1000* dw1000, DW1000_Config* config)
{
	/* Check that 1 <= ch <= 7 and ch != 7 */
	if(config->channel <= 0 || config->channel == 6 || config->channel > 7)
	{
		return false;
	}
	/* Check that tx codes match PRF */
	else if((config->prf == DW1000_PRF_16MHZ && (1 > config->tx_code || config->tx_code > 8)) ||
	        (config->prf == DW1000_PRF_64MHZ && (9 > config->tx_code || config->tx_code > 24)))
	{
		return false;
	}
	/* Check that rx codes match PRF */
	else if((config->prf == DW1000_PRF_16MHZ && (1 > config->rx_code && config->rx_code > 8)) ||
	        (config->prf == DW1000_PRF_64MHZ && (9 > config->rx_code && config->rx_code > 24)))
	{
		return false;
	}

	dw1000_set_channel        (dw1000, config->channel);
	dw1000_set_pac_prf        (dw1000, config->pac, config->prf);
	dw1000_set_data_rate      (dw1000, config->data_rate);
	dw1000_set_tx_code        (dw1000, config->tx_code);
	dw1000_set_rx_code        (dw1000, config->rx_code);
	dw1000_set_preamble_length(dw1000, config->preamble_length);
	dw1000_set_sfd_timeout    (dw1000, config->sfd_timeout);

	/* Disable smart tx power */
	dw1000->sys_cfg |=
		DW1000_SYS_CFG_PHR_MODE_LONG |
		DW1000_SYS_CFG_DIS_STXP_DISABLE_AUTO_PWR |
		DW1000_SYS_CFG_HIRQ_POL_ACTIVE_HIGH;
	// dw1000->sys_cfg |=
	// 	DW1000_SYS_CFG_DIS_STXP_DISABLE_AUTO_PWR |
	// 	DW1000_SYS_CFG_HIRQ_POL_ACTIVE_HIGH;

	/* Write SYS_CFG */
	dw1000_spi_write32(dw1000, DW1000_SYS_CFG, DW1000_NO_SUB_ADDR, dw1000->sys_cfg);

	/* Write CHAN_CTRL */
	dw1000_spi_write32(dw1000, DW1000_CHAN_CTRL, DW1000_NO_SUB_ADDR, dw1000->chan_ctrl);

	/* Write TX_FCTRL */
	dw1000_spi_write32(dw1000, DW1000_TX_FCTRL, DW1000_NO_SUB_ADDR, dw1000->tx_fctrl);

	/* Todo: just testing PLL SYNC */
	dw1000_spi_write8(dw1000, DW1000_EXT_SYNC, DW1000_EC_CTRL_OFFSET, DW1000_EXT_SYNC_PLLLDT);

	/* Workaround for SFD transmit pattern initialization. SFD transmit pattern is not initialized
	 * for auto-ACK'ed TX. Simultaneously initiating and aborting a transmission correctly
	 * initializes the SFD. */
	dw1000_spi_write8(
		dw1000,
		DW1000_SYS_CTRL,
		DW1000_NO_SUB_ADDR,
		DW1000_SYS_CTRL_TXSTRT | DW1000_SYS_CTRL_TRXOFF);

	return true;
}


/* dw1000_set_channel ***************************************************************************//**
 * @brief		Sets the DW1000 channel. Accepted values are 1, 2, 3, 4, 5, 7. */
static void dw1000_set_channel(DW1000* dw1000, unsigned ch)
{
	/* 0x1E – Transmit Power Control
	 * 0x1F – Channel Control (TX_CHAN and RX_CHAN field)
	 * 0x28:0B – RF_RXCTRLH
	 * 0x28:0C – RF_TXCTRL
	 * 0x2A:0B – TC_PGDELAY
	 * 0x2B:07 – FS_PLLCFG
	 * 0x2B:0B – FS_PLLTUNE */
	dw1000->channel = ch;

	/* Set 0x1F – Channel Control (TX_CHAN and RX_CHAN field) for later */
	dw1000->chan_ctrl &= ~(DW1000_CHAN_CTRL_RX_CHAN_MASK | DW1000_CHAN_CTRL_TX_CHAN_MASK);
	dw1000->chan_ctrl |= ch << DW1000_CHAN_CTRL_RX_CHAN_SHIFT;
	dw1000->chan_ctrl |= ch << DW1000_CHAN_CTRL_TX_CHAN_SHIFT;

	/* @TODO: Transmit Power Control */

	/* Write 0x28:0B - RF_RXCTRLH. Confiugures RF RX blocks for the specified channel and
	 * bandwidth. */
	dw1000_spi_write8(
		dw1000,
		DW1000_RF_CONF,
		DW1000_RF_RXCTRLH_OFFSET,
		dw1000_rf_rxctrlh[ch-1]);

	/* Write 0x28:0C – RF_TXCTRL. Configure RF TX blocks for the specified channel and PRF. */
	dw1000_spi_write32(
		dw1000,
		DW1000_RF_CONF,
		DW1000_RF_TXCTRL_OFFSET,
		dw1000_rf_txctrl[ch-1]);

	/* Write 0x2A:0B – TC_PGDELAY. @TODO */

	/* Write 0x2B:07 – FS_PLLCFG */
	dw1000_spi_write32(
		dw1000,
		DW1000_FS_CTRL,
		DW1000_FS_PLLCFG_OFFSET,
		dw1000_fs_pllcfg[ch-1]);

	/* Write 0x2B:0B – FS_PLLTUNE */
	dw1000_spi_write8(
		dw1000,
		DW1000_FS_CTRL,
		DW1000_FS_PLLTUNE_OFFSET,
		dw1000_fs_plltune[ch-1]);
}


/* dw1000_set_prf *******************************************************************************//**
 * @brief		Sets the DW1000 pulse repetition frequency. */
static void dw1000_set_pac_prf(DW1000* dw1000, DW1000_Pac pac, DW1000_Prf prf)
{
	/* 0x08 – Transmit Frame Control (TXPRF field)
	 * 0x1F – Channel Control (RXPRF field)
	 * 0x23:04 – AGC_TUNE1
	 * 0x27:04 – DRX_TUNE1a
	 * 0x27:08 – DRX_TUNE2
	 * 0x2E:1806 – LDE_CFG2 */
	dw1000->pac = pac;
	dw1000->prf = prf;

	/* Set 0x08 – Transmit Frame Control (TXPRF field) for later */
	dw1000->tx_fctrl &= ~(DW1000_TX_FCTRL_TXPRF_MASK);
	dw1000->tx_fctrl |= (prf + 1) << DW1000_TX_FCTRL_TXPRF_SHIFT;

	/* 0x1F – Channel Control (RXPRF field) for later */
	dw1000->chan_ctrl &= ~(DW1000_CHAN_CTRL_RXPRF_MASK);
	dw1000->chan_ctrl |= (prf + 1) << DW1000_CHAN_CTRL_RXPRF_SHIFT;

	/* Write 0x23:04 – AGC_TUNE1 */
	dw1000_spi_write16(
		dw1000,
		DW1000_AGC_CTRL,
		DW1000_AGC_TUNE1_OFFSET,
		dw1000_agc_tune1[prf]);

	/* Write 0x23:0C – AGC_TUNE2 */
	dw1000_spi_write32(
		dw1000,
		DW1000_AGC_CTRL,
		DW1000_AGC_TUNE2_OFFSET,
		dw1000_agc_tune2[0]);

	/* Write 0x27:04 – DRX_TUNE1a */
	dw1000_spi_write16(
		dw1000,
		DW1000_DRX_CONF,
		DW1000_DRX_TUNE1A_OFFSET,
		dw1000_drx_tune1a[prf]);

	/* Write 0x27:08 – DRX_TUNE2 */
	dw1000_spi_write32(
		dw1000,
		DW1000_DRX_CONF,
		DW1000_DRX_TUNE2_OFFSET,
		dw1000_drx_tune2[pac][prf]);

	/* Write 0x2E:1806 – LDE_CFG2 */
	dw1000_spi_write8(
		dw1000,
		DW1000_LDE_CTRL,
		DW1000_LDE_CFG1_OFFSET,
		(0x3 << 5) | (0xD << 0));

	dw1000_spi_write16(
		dw1000,
		DW1000_LDE_CTRL,
		DW1000_LDE_CFG2_OFFSET,
		dw1000_lde_cfg2[prf]);
}


/* dw1000_set_data_rate *************************************************************************//**
 * @brief		Sets the DW1000 data rate. */
static void dw1000_set_data_rate(DW1000* dw1000, DW1000_Data_Rate data_rate)
{
	/* 0x04 – System Configuration (RXM110K field)
	 * 0x08 – Transmit Frame Control (TXBR field)
	 * 0x1A – Acknowledgement time and response time (ACK_TIM field)
	 * 0x27:02 – DRX_TUNE0b */
	dw1000->data_rate = data_rate;

	/* Set 0x04 – System Configuration (RXM110K field) for later*/
	if(data_rate == DW1000_DR_110KBPS)
	{
		dw1000->sys_cfg |= DW1000_SYS_CFG_RXM110K_LONG_SFD;
	}
	else
	{
		dw1000->sys_cfg &= ~(DW1000_SYS_CFG_RXM110K_LONG_SFD);
	}

	/* Set 0x08 – Transmit Frame Control (TXBR field) for later */
	dw1000->tx_fctrl &= ~(DW1000_TX_FCTRL_TXBR_MASK);
	dw1000->tx_fctrl |= data_rate << DW1000_TX_FCTRL_TXBR_SHIFT;

	/* Write 0x1A – Acknowledgement time and response time (ACK_TIM field) */
	// dw1000_spi_write(
	// 	dw1000,
	// 	DW1000_ACK_RESP_T,
	// 	3,
	// 	dw1000_ack_tim[data_rate]);

	/* Write 0x27:02 – DRX_TUNE0b */
	dw1000_spi_write16(
		dw1000,
		DW1000_DRX_CONF,
		DW1000_DRX_TUNE0B_OFFSET,
		dw1000_drx_tune0b[data_rate][0]);
}


/* dw1000_set_tx_code ***************************************************************************//**
 * @brief		Sets the DW1000 tx preamble code.
 * Desc			Channel		Centre 		Bandwidth	Preamble Codes		Preamble Codes
 * 				Number		Freq (MHz)	(MHz)		(16 MHz PRF)		(64 MHz PRF)
 * 				1			3494.4		499.2		1, 2				9, 10, 11, 12
 * 				2			3993.6		499.2		3, 4				9, 10, 11, 12
 * 				3			4492.8		499.2		5, 6				9, 10, 11, 12
 * 				4			3993.6		1331.2*		7, 8				17, 18, 19, 20
 * 				5			6489.6		499.2		3, 4				9, 10, 11, 12
 * 				7			6489.6		1081.6*		7, 8				17, 18, 19, 20
 *
 * 				*DW1000 has a maximum bandwidth of 900 MHz.
 *
 * 				Additional codes used for 2-way ranging may use Dynamic Preamble Select (DPS) which
 * 				are preamble codes used solely for ranging. The codes for 64 MHz PRF are:
 * 				13, 14, 15, 16, 21, 22, 23, and 24.
 * @warning		Requires data rate to be configured prior to calling dw1000_set_tx_code. */
static void dw1000_set_tx_code(DW1000* dw1000, unsigned tx_code)
{
	/* 0x1F – Channel Control (TX_PCODE and RX_PCODE) */
	dw1000->tx_code = tx_code;

	/* Set 0x1F – Channel Control (TX_PCODE and RX_PCODE) for later */
	dw1000->chan_ctrl &= ~(DW1000_CHAN_CTRL_TX_PCODE_MASK);
	dw1000->chan_ctrl |= tx_code << DW1000_CHAN_CTRL_TX_PCODE_SHIFT;
}


/* dw1000_set_rx_code ***************************************************************************//**
 * @brief		Sets the DW1000 rx preamble code.
 * Desc			Channel		Centre 		Bandwidth	Preamble Codes		Preamble Codes
 * 				Number		Freq (MHz)	(MHz)		(16 MHz PRF)		(64 MHz PRF)
 * 				1			3494.4		499.2		1, 2				9, 10, 11, 12
 * 				2			3993.6		499.2		3, 4				9, 10, 11, 12
 * 				3			4492.8		499.2		5, 6				9, 10, 11, 12
 * 				4			3993.6		1331.2*		7, 8				17, 18, 19, 20
 * 				5			6489.6		499.2		3, 4				9, 10, 11, 12
 * 				7			6489.6		1081.6*		7, 8				17, 18, 19, 20
 *
 * 				*DW1000 has a maximum bandwidth of 900 MHz.
 *
 * 				Additional codes used for 2-way ranging may use Dynamic Preamble Select (DPS) which
 * 				are preamble codes used solely for ranging. The codes for 64 MHz PRF are:
 * 				13, 14, 15, 16, 21, 22, 23, and 24.
 * @warning		Requires data rate to be configured prior to calling dw1000_set_tx_code. */
static void dw1000_set_rx_code(DW1000* dw1000, unsigned rx_code)
{
	/* 0x1F – Channel Control (TX_PCODE and RX_PCODE)
	 * 0x2E:2804 – LDE_REPC (Applies to 850 kbps and 6.8 Mbps data rates only) */
	dw1000->rx_code = rx_code;

	/* Set 0x1F – Channel Control (TX_PCODE and RX_PCODE) for later */
	dw1000->chan_ctrl &= ~(DW1000_CHAN_CTRL_RX_PCODE_MASK);
	dw1000->chan_ctrl |= rx_code << DW1000_CHAN_CTRL_RX_PCODE_SHIFT;

	/* Write 0x2E:2804 – LDE_REPC */
	if(dw1000->data_rate == DW1000_DR_110KBPS)
	{
		dw1000_spi_write16(
			dw1000,
			DW1000_LDE_CTRL,
			DW1000_LDE_REPC_OFFSET,
			dw1000_lde_repc[rx_code] / 8);
	}
	else
	{
		dw1000_spi_write16(
			dw1000,
			DW1000_LDE_CTRL,
			DW1000_LDE_REPC_OFFSET,
			dw1000_lde_repc[rx_code]);
	}
}


/* dw1000_set_preamble_length *******************************************************************//**
 * @brief		Sets the DW1000 preamble length. Accepted values are 64, 1024, 4096.
 * @warning		Requires data rate to be configured prior to calling dw1000_set_preamble_length. */
static void dw1000_set_preamble_length(DW1000* dw1000, DW1000_Pre_Length preamble_length)
{
	/* 0x08 – Transmit Frame Control (TXPSR and PE fields)
	 * 0x27:06 – DRX_TUNE1b
	 * 0x27:26 – DRX_TUNE4H */
	dw1000->preamble_length = preamble_length;

	/* Set 0x08 – Transmit Frame Control (TXPSR and PE fields) for later */
	dw1000->tx_fctrl &= ~(DW1000_TX_FCTRL_PE_MASK | DW1000_TX_FCTRL_TXPSR_MASK);
	dw1000->tx_fctrl |= preamble_length << DW1000_TX_FCTRL_TXPSR_SHIFT;

	/* Write 0x27:06 – DRX_TUNE1b and 0x27:26 – DRX_TUNE4H */
	if(dw1000->data_rate == DW1000_DR_110KBPS)
	{
		dw1000_spi_write16(
			dw1000,
			DW1000_DRX_CONF,
			DW1000_DRX_TUNE1B_OFFSET,
			dw1000_drx_tune1b[0]);
	}
	else if(preamble_length == DW1000_PLEN_64)
	{
		dw1000_spi_write16(
			dw1000,
			DW1000_DRX_CONF,
			DW1000_DRX_TUNE1B_OFFSET, dw1000_drx_tune1b[2]);

		dw1000_spi_write8(
			dw1000,
			DW1000_DRX_CONF,
			DW1000_DRX_TUNE4H_OFFSET,
			dw1000_drx_tune4h[0]);
	}
	else
	{
		dw1000_spi_write16(
			dw1000,
			DW1000_DRX_CONF,
			DW1000_DRX_TUNE1B_OFFSET,
			dw1000_drx_tune1b[1]);

		dw1000_spi_write8(
			dw1000,
			DW1000_DRX_CONF,
			DW1000_DRX_TUNE4H_OFFSET,
			dw1000_drx_tune4h[1]);
	}
}


/* dw1000_set_sfd_timeout ***********************************************************************//**
 * @brief		Sets the SFD detection timeout.
 * @TODO: 		what are the units? */
static void dw1000_set_sfd_timeout(DW1000* dw1000, uint16_t sfd_timeout)
{
	if(sfd_timeout == 0)
	{
		sfd_timeout = 4096 + 64 + 1;
	}
	else
	{
		dw1000->sfd_timeout = sfd_timeout;
	}

	dw1000_spi_write16(dw1000, DW1000_DRX_CONF, DW1000_DRX_SFDTOC_OFFSET, dw1000->sfd_timeout);
}


/* dw1000_set_tx_ant_delay **********************************************************************//**
 * @brief		Sets the transmit antenna delay. Units are 499.2 MHz × 128 = ~15.65 ps. The transmit
 * 				antenna delay is used to account for the delay between the internal digital timestamp
 * 				of the RMARKER (at the start of the PHR) and the time the RMARKER is at the
 * 				antenna.
 * @warning		This register is not preserved during SLEEP or DEEPSLEEP and so needs reprogramming
 * 				after a wakeup event in order to obtain the correct adjustment of the TX_STAMP. */
void dw1000_set_tx_ant_delay(DW1000* dw1000, uint16_t delay)
{
	dw1000_spi_write16(dw1000, DW1000_TX_ANTD, DW1000_NO_SUB_ADDR, delay);
}


/* dw1000_set_rx_ant_delay **********************************************************************//**
 * @brief		Sets the receive antenna delay. Units are 499.2 MHz × 128 = ~15.65 ps. The receive
 * 				antenna delay is used by the LDE algorithm to to produce the fully adjusted receive
 * 				timestamp RX_STAMP. */
void dw1000_set_rx_ant_delay(DW1000* dw1000, uint16_t delay)
{
	dw1000_spi_write16(dw1000, DW1000_LDE_CTRL, DW1000_LDE_RXANTD_OFFSET, delay);
}


/* dw1000_handle_irq ****************************************************************************//**
 * @brief		Performs DW1000 specific tasks to handle an interrupt. Clears DW1000 interrupts and
 * 				resets the transceiver if an RX error occurs. */
uint32_t dw1000_handle_irq(DW1000* dw1000)
{
	uint32_t status = dw1000_read_status(dw1000);
	uint32_t clear  = 0;

	/* Todo: Testing PLL LOCK IRQ */
	if(status & DW1000_SYS_STATUS_CPLOCK)
	{
		clear |= DW1000_SYS_STATUS_CPLOCK;
	}

	/* Handle TX complete event */
	if(status & DW1000_SYS_STATUS_TXFRS)
	{
		/* Clear TX frame sent event */
		clear |= DW1000_SYS_STATUS_AAT   | DW1000_SYS_STATUS_TXFRB | DW1000_SYS_STATUS_TXFRS |
		         DW1000_SYS_STATUS_TXPHS | DW1000_SYS_STATUS_TXPRS;
	}

	/* Handle RX complete event */
	if(status & DW1000_SYS_STATUS_RXFCG)
	{
		/* Clear all receive status bits */
		clear |= DW1000_SYS_STATUS_RXDFR  | DW1000_SYS_STATUS_RXFCG | DW1000_SYS_STATUS_RXPRD |
		         DW1000_SYS_STATUS_RXSFDD | DW1000_SYS_STATUS_RXPHD | DW1000_SYS_STATUS_LDEDONE;
	}

	/* Handle reception / preamble detect timeout events */
	if((status & DW1000_SYS_STATUS_RXRFTO) || (status & DW1000_SYS_STATUS_RXPTO))
	{
		clear |= DW1000_SYS_STATUS_RXRFTO | DW1000_SYS_STATUS_RXPTO;
	}

	/* Handle RX error events */
	if((status & DW1000_SYS_STATUS_RXPHE)   ||
	   (status & DW1000_SYS_STATUS_RXFCE)   ||
	   (status & DW1000_SYS_STATUS_RXRFSL)  ||
	   (status & DW1000_SYS_STATUS_RXSFDTO) ||
	   (status & DW1000_SYS_STATUS_AFFREJ)  ||
	   (status & DW1000_SYS_STATUS_LDEERR))
	{
		clear |= DW1000_SYS_STATUS_RXPHE   | DW1000_SYS_STATUS_RXFCE  | DW1000_SYS_STATUS_RXRFSL |
		         DW1000_SYS_STATUS_RXSFDTO | DW1000_SYS_STATUS_AFFREJ | DW1000_SYS_STATUS_LDEERR;
	}

	/* Clear SFD Detect bit */
	if(status & DW1000_SYS_STATUS_RXSFDD)
	{
		clear |= DW1000_SYS_STATUS_RXSFDD;
	}

	if(clear)
	{
		dw1000_int_clear(dw1000, clear);
	}

	if((status & DW1000_SYS_STATUS_RXRFTO)  ||
	   (status & DW1000_SYS_STATUS_RXPTO)   ||
	   (status & DW1000_SYS_STATUS_RXPHE)   ||
	   (status & DW1000_SYS_STATUS_RXFCE)   ||
	   (status & DW1000_SYS_STATUS_RXRFSL)  ||
	   (status & DW1000_SYS_STATUS_RXSFDTO) ||
	   (status & DW1000_SYS_STATUS_AFFREJ)  ||
	   (status & DW1000_SYS_STATUS_LDEERR))
	{
		dw1000_force_trx_off(dw1000, status);
		dw1000_rx_reset(dw1000);
	}

	return status;
}





/* dw1000_read_dev_id ***************************************************************************//**
 * @brief		Reads the DW1000 device id register. */
uint32_t dw1000_read_dev_id(DW1000* dw1000)
{
	return dw1000_spi_read32(dw1000, DW1000_DEV_ID, DW1000_NO_SUB_ADDR);
}


/* dw1000_wait_for_irq **************************************************************************//**
 * @brief		Waits for an interrupt from the DW1000.
 * @param[in]	timeout: timeout in us. */
int dw1000_wait_for_irq(DW1000* dw1000, uint32_t timeout)
{
	if(timeout == -1u)
	{
		while(1)
		{
			if(nrf_gpio_pin_read(dw1000->gpio_int_pin))
			{
				return 0;
			}
		}
	}
	else
	{
		uint64_t start = ts_time_now();
		uint64_t end   = calc_addmod_u64(start, calc_round_u32(timeout + 30, 31), TS_PERIOD);

		/* Todo: calc_wrapdiff on unsigned */
		do {
			if(nrf_gpio_pin_read(dw1000->gpio_int_pin))
			{
				return 0;
			}
		} while(calc_wrapdiff_u64(end, ts_time_now(), TS_PERIOD) > 0);
	}

	return -1;
}


/* dw1000_set_trx_tstamp ************************************************************************//**
 * @brief		Sets the timestamp for delayed transmission or reception. Units of the delay time are
 * 				based off the internal 64 GHz clock. The resolution of the delay is
 *
 * 					~8 ns = 2^9 / (64 GHz)
 *
 * 				because the delay time ignores the 9 lower bits. Internally, the delay is a 40 bit
 * 				value. Therefore, the maximum delay is
 *
 * 					0xFFFFFFFE00 / (499.2 MHz * 128) = ~17.207 seconds.
 */
int32_t dw1000_set_trx_tstamp(DW1000* dw1000, uint64_t tstamp)
{
	/* Bottom 9 bits are ignored. Write a 32-bit value at index 1 instead.
	 *
	 *           3          2          1          0
	 * 98765432 10987654 32109876 54321098 76543210
	 *                                   x xxxxxxxx ignored
	 */
	int32_t offset = -(int32_t)(tstamp & 0x1FF);

	tstamp &= 0xFFFFFFFFFFull;	/* Mask of 40 bits */
	tstamp >>= 8;

	dw1000_spi_write32(dw1000, DW1000_DX_TIME, 1, tstamp);

	return offset;
}


/* dw1000_write_tx ******************************************************************************//**
 * @brief		Loads data into the transmit buffer. */
bool dw1000_write_tx(DW1000* dw1000, const void* tx, unsigned offset, unsigned txlen)
{
	if(txlen <= 1024 && offset < 1024 && offset + txlen <= 1024)
	{
		/* Write to 0x09 – TX_BUFFER */
		dw1000_spi_write(dw1000, DW1000_TX_BUFFER, offset, tx, txlen);
		return true;
	}
	else
	{
		return false;
	}
}


/* dw1000_write_tx_fctrl ************************************************************************//**
 * @brief		Writes the TX frame control. Note: assumes txlen includes the length of the crc. */
bool dw1000_write_tx_fctrl(DW1000* dw1000, unsigned offset, unsigned txlen)
{
	if(txlen <= 1024 && offset < 1024 && offset + txlen <= 1024)
	{
		/* Write to 0x08 – TX_FCTRL. Note:  */
		uint32_t tx_fctrl =
			dw1000->tx_fctrl |
			(txlen << DW1000_TX_FCTRL_TFLEN_SHIFT) |
			(offset << DW1000_TX_FCTRL_TXBOFFS_SHIFT) |
			DW1000_TX_FCTRL_TR_ENABLE_RANGING;

		dw1000_spi_write32(dw1000, DW1000_TX_FCTRL, DW1000_NO_SUB_ADDR, tx_fctrl);

		return true;
	}
	else
	{
		return false;
	}
}


/* dw1000_start_tx ******************************************************************************//**
 * @brief		Starts transmission.
 * @param[in]	dw1000: the transmitting radio.
 * @param[in]	expect_rx: true if a response is expected. In this case, the receiver will be
 * 				automatically enabled after wait_for_rx has elapsed. False causes the dw1000 to go
 * 				back to IDLE after transmission. */
bool dw1000_start_tx(DW1000* dw1000, bool expect_rx)
{
	uint8_t sys_ctrl =
		(expect_rx ? DW1000_SYS_CTRL_WAIT4RESP : 0) |
		DW1000_SYS_CTRL_SFCST_ENABLE_AUTO_FCS       |
		DW1000_SYS_CTRL_TXSTRT;

	/* Write to 0x0D – SYS_CTRL */
	dw1000_spi_write8(dw1000, DW1000_SYS_CTRL, DW1000_NO_SUB_ADDR, sys_ctrl);
	return true;
}


/* dw1000_start_delayed_tx **********************************************************************//**
 * @brief		Starts a delayed transmission. The delay time should be set by dw1000_set_delay.
 * @param[in]	dw1000: the transmitting radio.
  * @param[in]	expect_rx: true if a response is expected. In this case, the receiver will be
 * 				automatically enabled after wait_for_rx has elapsed. False causes the dw1000 to go
 * 				back to IDLE after transmission. */
bool dw1000_start_delayed_tx(DW1000* dw1000, bool expect_rx)
{
	uint8_t sys_ctrl =
		(expect_rx ? DW1000_SYS_CTRL_WAIT4RESP : 0) |
		DW1000_SYS_CTRL_SFCST_ENABLE_AUTO_FCS |
		DW1000_SYS_CTRL_TXDLYS |
		DW1000_SYS_CTRL_TXSTRT;

	/* Write to 0x0D – SYS_CTRL */
	dw1000_spi_write8(dw1000, DW1000_SYS_CTRL, DW1000_NO_SUB_ADDR, sys_ctrl);

	/* Check that the delay was not too short. Read bit 27 HPDWARN and bit bit 34 TXPUTE. */
	uint16_t status = dw1000_spi_read16(dw1000, DW1000_SYS_STATUS, 3);

	/* TXPUTE  (bit 34 - 24) = bit 10 = 0x0400
	 * HPDWARN (bit 27 - 24) = bit 3  = 0x0008 */
	if(status & (0x0408))
	{
		dw1000_spi_write8(dw1000, DW1000_SYS_CTRL, DW1000_NO_SUB_ADDR, DW1000_SYS_CTRL_TRXOFF);
		return false;
	}
	else
	{
		return true;
	}
}


/* dw1000_set_wait_for_rx ***********************************************************************//**
 * @brief		Sets the turnaround time between transmission and reception for transmitted packets
 * 				expecting a response. Turnaround time is a 20 bit field with units of 512 / 499.2 MHz
 * 				or ~1.0256 us. */
void dw1000_set_wait_for_rx(DW1000* dw1000, uint32_t turnaround_time)
{
	/* Clamp turnaround time to 2^20 - 1 */
	if(turnaround_time > 1048575)
	{
		turnaround_time = 1048575;
	}

	uint8_t buf[3];
	buf[0] = (turnaround_time >> 0)  & 0xFF;
	buf[1] = (turnaround_time >> 8)  & 0xFF;
	buf[2] = (turnaround_time >> 16) & 0x0F;

	/* Wait for response time is bits 0-19. No need for further shifts. */
	dw1000_spi_write(dw1000, DW1000_ACK_RESP_T, DW1000_NO_SUB_ADDR, buf, sizeof(buf));
}





/* dw1000_set_drxb ******************************************************************************//**
 * @brief		Enables or disables RX double buffering. */
void dw1000_set_drxb(DW1000* dw1000, bool enable)
{
	if(enable)
	{
		dw1000->sys_cfg &= ~(DW1000_SYS_CFG_DIS_DRXB_DISABLE_DBUF);
	}
	else
	{
		dw1000->sys_cfg |= DW1000_SYS_CFG_DIS_DRXB_DISABLE_DBUF;
	}

	dw1000_spi_write8(dw1000, DW1000_SYS_CFG, 1, dw1000->sys_cfg >> 8);
}


/* dw1000_get_drxb ******************************************************************************//**
 * @brief		Returns the RX double buffering state. */
bool dw1000_get_drxb(DW1000* dw1000)
{
	return (dw1000->sys_cfg & DW1000_SYS_CFG_DIS_DRXB_MASK) == DW1000_SYS_CFG_DIS_DRXB_ENABLE_DBUF;
}


/* dw1000_sync_drxb *****************************************************************************//**
 * @brief		Synchronizes host-side receive buffer and IC-side receive buffer. */
void dw1000_sync_drxb(DW1000* dw1000, uint32_t status)
{
	uint32_t hsrbp = (status & DW1000_SYS_STATUS_HSRBP_MASK) >> DW1000_SYS_STATUS_HSRBP_SHIFT;
	uint32_t icrbp = (status & DW1000_SYS_STATUS_ICRBP_MASK) >> DW1000_SYS_STATUS_ICRBP_SHIFT;

	if(hsrbp != icrbp)
	{
		dw1000_hrbpt(dw1000);
	}
}


/* dw1000_hrbpt *********************************************************************************//**
 * @brief		Toggles the host receive side buffer. */
void dw1000_hrbpt(DW1000* dw1000)
{
	dw1000_spi_write8(dw1000, DW1000_SYS_CTRL, 3, DW1000_SYS_CTRL_HRBPT >> 24);
}


/* dw1000_preamble_timeout **********************************************************************//**
 * @brief		Sets the preamble timeout. The units of the preamble timeout are in PAC size. PAC
 * 				size is programmed when the preamble length is set. */
void dw1000_set_preamble_timeout(DW1000* dw1000, uint16_t timeout)
{
	dw1000_spi_write16(dw1000, DW1000_DRX_CONF, DW1000_DRX_PRETOC_OFFSET, timeout);
}


/* dw1000_set_rx_timeout ************************************************************************//**
 * @brief		Sets the frame receive timeout. Units for the timeout are 512 / 499.2 MHz or
 * 				~1.0256 us. */
void dw1000_set_rx_timeout(DW1000* dw1000, uint16_t timeout)
{
	/* Set 0x0C – Receive Frame Wait Timeout Period */
	if(timeout)
	{
		dw1000_spi_write16(dw1000, DW1000_RX_FWTO, DW1000_NO_SUB_ADDR, timeout);

		dw1000->sys_cfg |= DW1000_SYS_CFG_RXWTOE_ENABLE_RX_WAIT;
	}
	else
	{
		dw1000->sys_cfg &= ~(DW1000_SYS_CFG_RXWTOE_ENABLE_RX_WAIT);
	}

	// dw1000_spi_write32(dw1000, DW1000_SYS_CFG, DW1000_NO_SUB_ADDR, dw1000->sys_cfg);
	dw1000_spi_write8(dw1000, DW1000_SYS_CFG, 3, dw1000->sys_cfg >> 24);
}


/* dw1000_start_rx ******************************************************************************//**
 * @brief		Starts reception. */
bool dw1000_start_rx(DW1000* dw1000)
{
	uint16_t sys_ctrl = DW1000_SYS_CTRL_RXENAB;

	/* Write to 0x0D – SYS_CTRL */
	dw1000_spi_write16(dw1000, DW1000_SYS_CTRL, DW1000_NO_SUB_ADDR, sys_ctrl);

	return true;
}


/* dw1000_start_delayed_rx **********************************************************************//**
 * @brief		Starts a delayed reception. The delay time should be set by dw1000_set_delay. The
 * 				transceiver will be switched off if the delay time is too short. It is up to the
 * 				caller to reenable the transceiver using dw1000_start_rx in this case. If double
 * 				buffering is enabled, then the caller must also call dw1000_sync_rx_buffers. */
bool dw1000_start_delayed_rx(DW1000* dw1000)
{
	uint16_t sys_ctrl = DW1000_SYS_CTRL_RXENAB | DW1000_SYS_CTRL_RXDLYE;

	/* Write to 0x0D – SYS_CTRL */
	dw1000_spi_write16(dw1000, DW1000_SYS_CTRL, DW1000_NO_SUB_ADDR, sys_ctrl);

	/* Check that the delay was not too short */
	uint8_t hpdwarn = dw1000_spi_read8(dw1000, DW1000_SYS_STATUS, 3);

	if(hpdwarn & (DW1000_SYS_STATUS_HPDWARN >> 24))
	{
		dw1000_force_trx_off(dw1000, dw1000_read_status(dw1000));
		return false;
	}
	else
	{
		return true;
	}
}


/* dw1000_read_rx *******************************************************************************//**
 * @brief		Reads data from the dw1000 receive buffer. */
bool dw1000_read_rx(DW1000* dw1000, void* rx, unsigned offset, unsigned rxlen)
{
	if(rxlen <= 1024 && offset < 1024 && offset + rxlen <= 1024)
	{
		dw1000_spi_read(dw1000, DW1000_RX_BUFFER, offset, rx, rxlen);
		return true;
	}
	else
	{
		return false;
	}
}


/* dw1000_read_status ***************************************************************************//**
 * @brief		Reads the SYS_STATUS register. */
uint32_t dw1000_read_status(DW1000* dw1000)
{
	return dw1000_spi_read32(dw1000, DW1000_SYS_STATUS, DW1000_NO_SUB_ADDR);
}


/* dw1000_read_rx_finfo *************************************************************************//**
 * @brief		Reads the RX_FINFO register. */
uint32_t dw1000_read_rx_finfo(DW1000* dw1000)
{
	return dw1000_spi_read32(dw1000, DW1000_RX_FINFO, DW1000_NO_SUB_ADDR);
}


/* dw1000_read_sys_tstamp ***********************************************************************//**
 * @brief		Reads the system timestamp. */
uint64_t dw1000_read_sys_tstamp(DW1000* dw1000)
{
	uint8_t buf[5];

	dw1000_spi_read(dw1000, DW1000_SYS_TIME, DW1000_NO_SUB_ADDR, buf, sizeof(buf));

	return ((uint64_t)(buf[0]) << 0)  |
	       ((uint64_t)(buf[1]) << 8)  |
	       ((uint64_t)(buf[2]) << 16) |
	       ((uint64_t)(buf[3]) << 24) |
	       ((uint64_t)(buf[4]) << 32);
}


/* dw1000_read_tx_tstamp ************************************************************************//**
 * @brief		Reads the tx timestamp. */
uint64_t dw1000_read_tx_tstamp(DW1000* dw1000)
{
	uint8_t buf[5];

	dw1000_spi_read(dw1000, DW1000_TX_TIME, DW1000_NO_SUB_ADDR, buf, sizeof(buf));

	return ((uint64_t)(buf[0]) << 0)  |
	       ((uint64_t)(buf[1]) << 8)  |
	       ((uint64_t)(buf[2]) << 16) |
	       ((uint64_t)(buf[3]) << 24) |
	       ((uint64_t)(buf[4]) << 32);
}


/* dw1000_read_rx_tstamp ************************************************************************//**
 * @brief		Reads the rx timestamp. */
uint64_t dw1000_read_rx_tstamp(DW1000* dw1000)
{
	uint8_t buf[5];

	dw1000_spi_read(dw1000, DW1000_RX_TIME, DW1000_NO_SUB_ADDR, buf, sizeof(buf));

	return ((uint64_t)(buf[0]) << 0)  |
	       ((uint64_t)(buf[1]) << 8)  |
	       ((uint64_t)(buf[2]) << 16) |
	       ((uint64_t)(buf[3]) << 24) |
	       ((uint64_t)(buf[4]) << 32);
}


/* dw1000_read_drx_car_int **********************************************************************//**
 * @brief		Reads the RX carrier integrator register value. */
static int32_t dw1000_read_drx_car_int(DW1000* dw1000)
{
	uint8_t  buf[3];
	uint32_t val;

	dw1000_spi_read(dw1000, DW1000_DRX_CONF, DW1000_DRX_CAR_INT_OFFSET, buf, sizeof(buf));

	val  = (uint32_t)(buf[2]) << 16;
	val |= (uint32_t)(buf[1]) << 8;
	val |= (uint32_t)(buf[0]) << 0;

	/* Sign extend */
	if(val & (1 << 20))
	{
		val |= (0xFFF00000ul);
	}
	else
	{
		val &= 0x001FFFFF;
	}

	return (int32_t)val;
}


/* dw1000_rx_clk_offset *************************************************************************//**
 * @brief		Returns the clock offset in Hz of the remote transmitter. The sign of the returned
 * 				value indicates that the remote transmitter's clock is faster (+) or slower (-) than
 * 				this receiver's clock. */
float dw1000_rx_clk_offset(DW1000* dw1000)
{
	int32_t drx_car_int = dw1000_read_drx_car_int(dw1000);

	if(dw1000_data_rate(dw1000) == DW1000_DR_110KBPS) {
		if(dw1000_channel(dw1000) == 1) {
			return DW1000_DRX_FOFFSET_110KBPS * DW1000_DRX_CLKOFFSET_CH1 * drx_car_int;
		} else if(dw1000_channel(dw1000) == 2) {
			return DW1000_DRX_FOFFSET_110KBPS * DW1000_DRX_CLKOFFSET_CH2 * drx_car_int;
		} else if(dw1000_channel(dw1000) == 3) {
			return DW1000_DRX_FOFFSET_110KBPS * DW1000_DRX_CLKOFFSET_CH3 * drx_car_int;
		} else if(dw1000_channel(dw1000) == 4) {
			return DW1000_DRX_FOFFSET_110KBPS * DW1000_DRX_CLKOFFSET_CH4 * drx_car_int;
		} else if(dw1000_channel(dw1000) == 5) {
			return DW1000_DRX_FOFFSET_110KBPS * DW1000_DRX_CLKOFFSET_CH5 * drx_car_int;
		} else {
			return DW1000_DRX_FOFFSET_110KBPS * DW1000_DRX_CLKOFFSET_CH7 * drx_car_int;
		}
	} else if(dw1000_data_rate(dw1000) == DW1000_DR_850KBPS) {
		if(dw1000_channel(dw1000) == 1) {
			return DW1000_DRX_FOFFSET_850KBPS * DW1000_DRX_CLKOFFSET_CH1 * drx_car_int;
		} else if(dw1000_channel(dw1000) == 2) {
			return DW1000_DRX_FOFFSET_850KBPS * DW1000_DRX_CLKOFFSET_CH2 * drx_car_int;
		} else if(dw1000_channel(dw1000) == 3) {
			return DW1000_DRX_FOFFSET_850KBPS * DW1000_DRX_CLKOFFSET_CH3 * drx_car_int;
		} else if(dw1000_channel(dw1000) == 4) {
			return DW1000_DRX_FOFFSET_850KBPS * DW1000_DRX_CLKOFFSET_CH4 * drx_car_int;
		} else if(dw1000_channel(dw1000) == 5) {
			return DW1000_DRX_FOFFSET_850KBPS * DW1000_DRX_CLKOFFSET_CH5 * drx_car_int;
		} else {
			return DW1000_DRX_FOFFSET_850KBPS * DW1000_DRX_CLKOFFSET_CH7 * drx_car_int;
		}
	} else {
		if(dw1000_channel(dw1000) == 1) {
			return DW1000_DRX_FOFFSET_6800KBPS * DW1000_DRX_CLKOFFSET_CH1 * drx_car_int;
		} else if(dw1000_channel(dw1000) == 2) {
			return DW1000_DRX_FOFFSET_6800KBPS * DW1000_DRX_CLKOFFSET_CH2 * drx_car_int;
		} else if(dw1000_channel(dw1000) == 3) {
			return DW1000_DRX_FOFFSET_6800KBPS * DW1000_DRX_CLKOFFSET_CH3 * drx_car_int;
		} else if(dw1000_channel(dw1000) == 4) {
			return DW1000_DRX_FOFFSET_6800KBPS * DW1000_DRX_CLKOFFSET_CH4 * drx_car_int;
		} else if(dw1000_channel(dw1000) == 5) {
			return DW1000_DRX_FOFFSET_6800KBPS * DW1000_DRX_CLKOFFSET_CH5 * drx_car_int;
		} else {
			return DW1000_DRX_FOFFSET_6800KBPS * DW1000_DRX_CLKOFFSET_CH7 * drx_car_int;
		}
	}
}


/* dw1000_sleep_after_tx ************************************************************************//**
 * @brief		Automatically enter sleep or deep sleep after transmitting. */
void dw1000_sleep_after_tx(DW1000* dw1000, bool enable)
{
	uint32_t pmsc_ctrl1 = dw1000_spi_read32(dw1000, DW1000_PMSC, DW1000_PMSC_CTRL1_OFFSET);

	if(enable)
	{
		/* This bit is automatically cleared when the DW1000 wakes up unless the PRES_SLEEP bit is
		 * set in 0x2C:00 – AON_WCFG. */
		pmsc_ctrl1 |= DW1000_PMSC_CTRL1_ATXSLP;
	}
	else
	{
		pmsc_ctrl1 &= ~(DW1000_PMSC_CTRL1_ATXSLP);
	}

	dw1000_spi_write32(dw1000, DW1000_PMSC, DW1000_PMSC_CTRL1_OFFSET, pmsc_ctrl1);
}


/* dw1000_sleep_after_rx ************************************************************************//**
 * @brief		Automatically enter sleep or deep sleep after receiving. */
void dw1000_sleep_after_rx(DW1000* dw1000, bool enable)
{
	uint32_t pmsc_ctrl1 = dw1000_spi_read32(dw1000, DW1000_PMSC, DW1000_PMSC_CTRL1_OFFSET);

	if(enable)
	{
		pmsc_ctrl1 |= DW1000_PMSC_CTRL1_ARXSLP;
	}
	else
	{
		pmsc_ctrl1 &= ~(DW1000_PMSC_CTRL1_ARXSLP);
	}

	dw1000_spi_write32(dw1000, DW1000_PMSC, DW1000_PMSC_CTRL1_OFFSET, pmsc_ctrl1);
}


/* dw1000_int_enable ****************************************************************************//**
 * @brief		Enables the specified interrupts. */
void dw1000_int_enable(DW1000* dw1000, uint32_t ints)
{
	dw1000->sys_mask |= ints;

	dw1000_spi_write32(dw1000, DW1000_SYS_MASK, DW1000_NO_SUB_ADDR, dw1000->sys_mask);
}


/* dw1000_int_clear *****************************************************************************//**
 * @brief		Clears the specified interrupts. */
void dw1000_int_clear(DW1000* dw1000, uint32_t ints)
{
	dw1000_spi_write32(dw1000, DW1000_SYS_STATUS, DW1000_NO_SUB_ADDR, ints);
}


/* dw1000_int_disable ***************************************************************************//**
 * @brief		Disables the specified interrupts. */
void dw1000_int_disable(DW1000* dw1000, uint32_t ints)
{
	dw1000->sys_mask &= ~(ints);

	dw1000_spi_write32(dw1000, DW1000_SYS_MASK, DW1000_NO_SUB_ADDR, dw1000->sys_mask);
}





/* dw1000_force_trx_off *************************************************************************//**
 * @brief		Turns the transceiver off immediately. Any in progress TX or RX activity is
 * 				aborted. */
void dw1000_force_trx_off(DW1000* dw1000, uint32_t status)
{
	// /* Save enabled interrupts */
	// uint32_t mask = dw1000_spi_read32(dw1000, DW1000_SYS_MASK, DW1000_NO_SUB_ADDR);

	// /* Disable all interrupts */
	// dw1000_spi_write32(dw1000, DW1000_SYS_MASK, DW1000_NO_SUB_ADDR, 0x00000000);

	/* Disable the radio */
	dw1000_spi_write8(dw1000, DW1000_SYS_CTRL, DW1000_NO_SUB_ADDR, DW1000_SYS_CTRL_TRXOFF);

	/* Clear all events */
	dw1000_spi_write32(
		dw1000,
		DW1000_SYS_STATUS,
		DW1000_NO_SUB_ADDR,
		DW1000_SYS_STATUS_AAT    | DW1000_SYS_STATUS_TXFRB  | DW1000_SYS_STATUS_TXPRS   |
		DW1000_SYS_STATUS_TXPHS  | DW1000_SYS_STATUS_TXFRS  | DW1000_SYS_STATUS_RXPHE   |
		DW1000_SYS_STATUS_RXFCE  | DW1000_SYS_STATUS_RXRFSL | DW1000_SYS_STATUS_RXSFDTO |
		DW1000_SYS_STATUS_AFFREJ | DW1000_SYS_STATUS_LDEERR | DW1000_SYS_STATUS_RXRFTO  |
		DW1000_SYS_STATUS_RXPTO  | DW1000_SYS_STATUS_RXDFR  | DW1000_SYS_STATUS_RXFCG   |
		DW1000_SYS_STATUS_RXPRD  | DW1000_SYS_STATUS_RXSFDD | DW1000_SYS_STATUS_RXPHD   |
		DW1000_SYS_STATUS_LDEDONE);

	/* Sync double receive buffers */
	dw1000_sync_drxb(dw1000, status);

	// /* Restore interrupts */
	// dw1000_spi_write32(dw1000, DW1000_SYS_MASK, DW1000_NO_SUB_ADDR, mask);
}


/* dw1000_rx_reset ******************************************************************************//**
 * @brief		Resets the receiver. Due to an issue in the re-initialisation of the receiver, it is
 * 				necessary to apply a receiver reset after certain receiver error or timeout events
 * 				(i.e. RXPHE (PHY Header Error), RXRFSL (Reed Solomon error), RXRFTO (Frame wait
 * 				timeout), etc.). This ensures that the next good frame will have correctly calculated
 * 				timestamp. It is not necessary to do this in the cases of RXPTO (Preamble detection
 * 				Timeout) and RXSFDTO (SFD timeout). */
void dw1000_rx_reset(DW1000* dw1000)
{
	/* Reset RX. Write directly to the SOFTRESET field. */
	dw1000_spi_write8(dw1000, DW1000_PMSC, DW1000_PMSC_CTRL0_OFFSET + 3, 0xE0);

	/* Clear reset. Write directly to the SOFTRESET field. */
	dw1000_spi_write8(dw1000, DW1000_PMSC, DW1000_PMSC_CTRL0_OFFSET + 3, 0xF0);
}





/* dw1000_config_sleep **************************************************************************//**
 * @brief		Configures the DW1000's sleep mode.
 * @desc		The DW1000 can be put to sleep using:
 *
 * 					void dw1000_sleep_after_tx(DW1000*, bool);
 * 					void dw1000_sleep_after_rx(DW1000*, bool);
 * 					void dw1000_enter_sleep(DW1000*);
 *
 * 				Power is maintained to the AON memory in SLEEP and DEEPSLEEP. The main configurations
 * 				are copied to and from AON memory when entering and exiting SLEEP and DEEPSLEEP
 * 				modes. Restoration of configurations during the WAKEUP state is only done if the
 * 				ONW_LDC configuration bit is set in 0x2C:00 – AON_WCFG.
 *
 * 				The DW1000 can be woken up from sleep by:
 *
 * 					1. 	Driving the wakeup pin high for ~500 us. (WAKE_PIN config bit must be setup
 * 						in 0x2C:06 – AON_CFG0).
 *
 * 					2. 	Driving SPI CSn pin low for ~500 us. (WAKE_SPI config bit must be setup in
 * 						0x2C:06 – AON_CFG0). Note: MOSI line must be held low for the duration of
 * 						the wake to ensure that no spurious writes occur.
 *
 * 					3.	Internal sleep counter expires. (WAKE_CNT config bit must be setup in
 * 						0x2C:06 – AON_CFG0 along with the appropriate SLEEP_TIM).
 *
 * 				In all cases, the DW1000 returns to IDLE mode.
 *
 * @param[in]	dw1000: the DW1000 to program the sleep configuration.
 * @param[in]	on_wake: bitmask of actions to take on waking up:
 *
 * 					DW1000_AON_WCFG_ONW_RADC
 * 					DW1000_AON_WCFG_ONW_RX
 * 					DW1000_AON_WCFG_ONW_LEUI
 * 					DW1000_AON_WCFG_ONW_LDC
 * 					DW1000_AON_WCFG_ONW_L64P
 * 					DW1000_AON_WCFG_PRES_SLEEP
 * 					DW1000_AON_WCFG_ONW_LLDE
 * 					DW1000_AON_WCFG_ONW_LLD0
 *
 * @param[in]	wake_trig: bitmask of triggers which can wake up the DW1000.
 *
 * 					DW1000_AON_CFG0_WAKE_PIN
 * 					DW1000_AON_CFG0_WAKE_SPI
 * 					DW1000_AON_CFG0_WAKE_CNT
 */
void dw1000_config_sleep(DW1000* dw1000, uint16_t on_wake, uint8_t wake_trig)
{
	/* Note: If LDOTUNE_CAL has been programmed into the OTP, then LDOTUNE_CAL must be loaded when
 	 * waking from SLEEP or DEEPSLEEP. This can be achieved by setting ONW_LLDO in
	 * 0x2C:00 – AON_WCFG. Note: this must only be done if LDOTUNE_CAL has been written to the OTP.
	 * See: dw1000_init. */
	on_wake   |= dw1000->on_wake;
	wake_trig |= DW1000_AON_CFG0_SLEEP_EN;

	dw1000_spi_write16(dw1000, DW1000_AON, DW1000_AON_WCFG_OFFSET, on_wake);

	/* TODO: this write overwrites LPDIV_EN and some of LPCLKDIVA. Is this ok? */
	dw1000_spi_write8(dw1000, DW1000_AON, DW1000_AON_CFG0_OFFSET, wake_trig);
}


/* dw1000_enter_sleep ***************************************************************************//**
 * @brief		Puts the device into SLEEP or DEEPSLEEP mode. Call dw1000_config_sleep before calling
 * 				dw1000_enter_sleep. */
void dw1000_enter_sleep(DW1000* dw1000)
{
	/* Setting the AON_CTRL_SAVE bit causes the DW1000 to enter SLEEP mode if SLEEP_EN is set. */
	dw1000_spi_write8(dw1000, DW1000_AON, DW1000_AON_CTRL_OFFSET, 0x00);
	dw1000_spi_write8(dw1000, DW1000_AON, DW1000_AON_CTRL_OFFSET, DW1000_AON_CTRL_SAVE);
	spim_power_down();
}


/* dw1000_wakeup_by_cs **************************************************************************//**
 * @brief		Wakes up the DW1000 by asserting the chip select signal */
void dw1000_wakeup_by_cs(DW1000* dw1000)
{
	spim_power_up();
	spim_cs(1);
	usleep(500);
	spim_cs(0);

	/* Wait 2ms for XTAL to start and stabilize. Note: could wait for MCPLOCK. */
	// usleep(2000);
}


/* dw1000_wakeup_by_pin **************************************************************************//**
 * @brief		Wakes up the DW1000 by asserting the wakeup pin. */
// void dw1000_wakeup_by_pin(DW1000* dw1000)
// {

// }





/* dw1000_enable_clock **************************************************************************//**
 * @brief		Enables / Disables the clocks specified in the 'clock' bitmask. Mask indicates the
 * 				bits that are changing. For example:
 *
 * 					clock = DW1000_PMSC_CTRL0_SYSCLKS_19_2_MHZ and
 * 					mask = DW1000_PMSC_CTRL0_SYSCLKS_MASK
 *
 * 				Only changes the DW1000_PMSC_CTRL0_SYSCLKS_MASK bits. Other bits in PMSC are left
 * 				unchanged. */
static void dw1000_enable_clock(DW1000* dw1000, uint16_t clock, uint16_t mask)
{
	uint16_t pmsc = dw1000_spi_read16(dw1000, DW1000_PMSC, DW1000_PMSC_CTRL0_OFFSET);

	pmsc = (pmsc & ~mask) | (clock & mask);

	/* Must write lower byte separately */
	dw1000_spi_write8(dw1000, DW1000_PMSC, DW1000_PMSC_CTRL0_OFFSET + 0, (pmsc >> 0) & 0xFF);
	dw1000_spi_write8(dw1000, DW1000_PMSC, DW1000_PMSC_CTRL0_OFFSET + 1, (pmsc >> 8) & 0xFF);
}


/* dw1000_otp_read ******************************************************************************//**
 * @brief		Reads a uint32_t from OTP memory.
 * @warning		Ensure that MR, MRa, MRb are reset to 0. */
static uint32_t dw1000_otp_read(DW1000* dw1000, uint32_t addr)
{
	/* Write address */
	dw1000_spi_write16(dw1000, DW1000_OTP_IF, DW1000_OTP_ADDR_OFFSET, addr);

	/* Perform OTP Read. Manual read mode must be set. */
	dw1000_spi_write8(
		dw1000,
		DW1000_OTP_IF,
		DW1000_OTP_CTRL_OFFSET,
		DW1000_OTP_CTRL_OTPREAD | DW1000_OTP_CTRL_OTPRDEN);

	/* OTPREAD is self clearing. OTPRDEN is not self clearing. */
	dw1000_spi_write8(
		dw1000,
		DW1000_OTP_IF,
		DW1000_OTP_CTRL_OFFSET,
		0x00);

	return dw1000_spi_read32(dw1000, DW1000_OTP_IF, DW1000_OTP_RDAT_OFFSET);
}





// ----------------------------------------------------------------------------------------------- //
// SPI Write / Read Functions                                                                      //
// ----------------------------------------------------------------------------------------------- //
/* dw1000_spi_write8 ****************************************************************************//**
 * @brief		Writes a uint8_t to the DW1000 register address and sub-address. Blocks until the
 * 				write is complete.
 * @param[in]	dw1000: the device to write to.
 * @param[in]	addr: the register address to write to.
 * @param[in]	sub: the sub-register address to write to.
 * @param[in]	value: the uint8_t value to write. */
static void dw1000_spi_write8(DW1000* dw1000, uint8_t addr, uint16_t sub, uint8_t value)
{
	dw1000_spi_write(dw1000, addr, sub, &value, 1);
}


/* dw1000_spi_write16 ***************************************************************************//**
 * @brief		Writes a uint16_t to the DW1000 register address and sub-address. Blocks until the
 * 				write is complete.
 * @param[in]	dw1000: the device to write to.
 * @param[in]	addr: the register address to write to.
 * @param[in]	sub: the sub-register address to write to.
 * @param[in]	value: the uint16_t value to write. */
static void dw1000_spi_write16(DW1000* dw1000, uint8_t addr, uint16_t sub, uint16_t value)
{
	value = le_u16(value);

	dw1000_spi_write(dw1000, addr, sub, &value, 2);
}


/* dw1000_spi_write32 ***************************************************************************//**
 * @brief		Writes a uint32_t to the DW1000 register address and sub-address. Blocks until the
 * 				write is complete.
 * @param[in]	dw1000: the device to write to.
 * @param[in]	addr: the register address to write to.
 * @param[in]	sub: the sub-register address to write to.
 * @param[in]	value: the uint32_t value to write. */
static void dw1000_spi_write32(DW1000* dw1000, uint8_t addr, uint16_t sub, uint32_t value)
{
	value = le_u32(value);

	dw1000_spi_write(dw1000, addr, sub, &value, 4);
}


/* dw1000_spi_read8 *****************************************************************************//**
 * @brief		Reads and returns a uint8_t from the DW1000 register address and sub-address. Blocks
 * 				until the read is complete.
 * @param[in]	dw1000: the device to read from.
 * @param[in]	addr: the register address to read from.
 * @param[in]	sub: the sub-register address to read from. */
static uint8_t dw1000_spi_read8(DW1000* dw1000, uint8_t addr, uint16_t sub)
{
	/* Note: workaround NRF52832 errata 58 by reading 2 bytes. Errata 58:  SPIM: An additional byte
	 * is clocked out when RXD.MAXCNT == 1 and TXD.MAXCNT <= 1. */
	uint8_t buf[2];

	dw1000_spi_read(dw1000, addr, sub, buf, sizeof(buf));

	return buf[0];
}


/* dw1000_spi_read16 ****************************************************************************//**
 * @brief		Reads and returns a uint16_t from the DW1000 register address and sub-address. Blocks
 * 				until the read is complete.
 * @param[in]	dw1000: the device to read from.
 * @param[in]	addr: the register address to read from.
 * @param[in]	sub: the sub-register address to read from. */
static uint16_t dw1000_spi_read16(DW1000* dw1000, uint8_t addr, uint16_t sub)
{
	uint8_t buf[2];

	dw1000_spi_read(dw1000, addr, sub, buf, sizeof(buf));

	return le_get_u16(buf);

	// return ((uint16_t)(buf[0]) << 0) |
	//        ((uint16_t)(buf[1]) << 8);
}


/* dw1000_spi_read32*****************************************************************************//**
 * @brief		Reads and returns a uint32_t from the DW1000 register address and sub-address. Blocks
 * 				until the read is complete.
 * @param[in]	dw1000: the device to read from.
 * @param[in]	addr: the register address to read from.
 * @param[in]	sub: the sub-register address to read from. */
static uint32_t dw1000_spi_read32(DW1000* dw1000, uint8_t addr, uint16_t sub)
{
	uint8_t buf[4];

	dw1000_spi_read(dw1000, addr, sub, buf, sizeof(buf));

	return le_get_u32(buf);

	// return ((uint32_t)(buf[0]) << 0)  |
	//        ((uint32_t)(buf[1]) << 8)  |
	//        ((uint32_t)(buf[2]) << 16) |
	//        ((uint32_t)(buf[3]) << 24);
}


/* dw1000_spi_write *****************************************************************************//**
 * @brief		Performs a blocking write to the DW1000 register address and sub-address.
 * @param[in]	dw1000: the device to write to.
 * @param[in]	addr: the register address to write to.
 * @param[in]	sub: the sub-register address to write to.
 * @param[in]	tx: the bytes to write.
 * @param[in]	txlen: the number of bytes to write. */
static void dw1000_spi_write(
	DW1000* dw1000,
	uint8_t addr,
	uint16_t sub,
	const void* tx,
	unsigned txlen)
{
	(void)dw1000;	/* dw1000 unused */

	uint8_t header[3];

	const Spibuf txbufs[] = {
		{ .ptr = header,    .len = dw1000_set_write_header(header, addr, sub) },
		{ .ptr = (void*)tx, .len = txlen },
	};

	spim_trx(txbufs, 2, 0, 0);

	// const struct spi_buf tx_bufs[] = {
	// 	{ .buf = header,    .len = dw1000_set_write_header(header, addr, sub) },
	// 	{ .buf = (void*)tx, .len = txlen },
	// };

	// const struct spi_buf_set tx_set = {
	// 	.buffers = tx_bufs,
	// 	.count   = ARRAY_SIZE(tx_bufs),
	// };

	// spim_transceive(&dw1000->spi_cfg, &tx_set, 0);

	// spi_transceive(dw1000->spi, &dw1000->spi_cfg, &tx_set, 0);

	// int err = spi_transceive(dw1000->spi, &dw1000->spi_cfg, &tx_set, 0);

	// printk("spi_transceive = %d\n", err);
}


/* dw1000_spi_read ******************************************************************************//**
 * @brief		Performs a blocking read to the DW1000 register address and sub-address.
 * @param[in]	dw1000: the device to read from.
 * @param[in]	addr: the register address to read from.
 * @param[in]	sub: the sub-register address to read from.
 * @param[in]	rx: the buffer to read bytes into.
 * @param[in]	rxlen: the number of bytes to read. */
static void dw1000_spi_read(
	DW1000* dw1000,
	uint8_t addr,
	uint16_t sub,
	void* rx,
	unsigned rxlen)
{
	(void)dw1000;	/* dw1000 unused */

	uint8_t header[3];

	unsigned len = dw1000_set_read_header(header, addr, sub);

	const Spibuf txbufs[] = {
		{ .ptr = header, .len = len },
	};

	const Spibuf rxbufs[] = {
		{ .ptr = 0,  .len = len   },
		{ .ptr = rx, .len = rxlen },
	};

	spim_trx(txbufs, 1, rxbufs, 2);

	// const struct spi_buf txbufs[] = {
	// 	{ .ptr = header, .len = len },
	// };

	// const struct spi_buf rxbufs[] = {
	// 	{ .ptr = 0,  .len = len   },
	// 	{ .ptr = rx, .len = rxlen },
	// };

	// const struct spi_buf_set tx_set = {
	// 	.buffers = tx_bufs,
	// 	.count   = ARRAY_SIZE(tx_bufs),
	// };

	// const struct spi_buf_set rx_set = {
	// 	.buffers = rx_bufs,
	// 	.count   = ARRAY_SIZE(rx_bufs),
	// };

	// spim_transceive(&dw1000->spi_cfg, &tx_set, &rx_set);
	// // spi_transceive(dw1000->spi, &dw1000->spi_cfg, &tx_set, &rx_set);
}


/* dw1000_set_write_header **********************************************************************//**
 * @brief		Sets the SPI write transaction's addressing fields. */
static unsigned dw1000_set_write_header(void* buf, uint8_t addr, uint16_t sub)
{
	return dw1000_set_header(buf, addr, sub, true);
}


/* dw1000_set_read_header ***********************************************************************//**
 * @brief		Sets the SPI read transaction's addressing fields. */
static unsigned dw1000_set_read_header(void* buf, uint8_t addr, uint16_t sub)
{
	return dw1000_set_header(buf, addr, sub, false);
}


/* dw1000_set_header ****************************************************************************//**
 * @brief		Sets the SPI transaction's addressing fields. */
static unsigned dw1000_set_header(void* buf, uint8_t addr, uint16_t sub, bool write)
{
	uint8_t* p = buf;
	p[0] = (write ? DW1000_SPI_WRITE : DW1000_SPI_READ) | DW1000_SPI_NO_SUB_ADDR | (addr & 0x3F);

	/* No sub address */
	if(sub == DW1000_NO_SUB_ADDR)
	{
		return 1;
	}
	/* Set short sub address */
	else if(sub <= 127)
	{
		p[0] |= DW1000_SPI_SUB_ADDR;
		p[1]  = DW1000_SPI_EXT_1 | (sub & 0x7F);
		return 2;
	}
	/* Set long sub address */
	else
	{
		p[0] |= DW1000_SPI_SUB_ADDR;
		p[1]  = DW1000_SPI_EXT_2 | (sub & 0x7F);
		p[2]  = (addr >> 7) & 0xFF;
		return 3;
	}
}


// /* dw1000_header_length *************************************************************************//**
//  * @brief		Returns the length of the addressing bytes in the SPI transaction buffer. */
// static unsigned dw1000_header_length(const void* buf)
// {
// 	const uint8_t* p = buf;

// 	if((p[0] & DW1000_SPI_SUB_MASK) == 0)
// 	{
// 		return 1;
// 	}
// 	else if((p[1] & DW1000_SPI_EXT_MASK) == DW1000_SPI_EXT_1)
// 	{
// 		return 2;
// 	}
// 	else
// 	{
// 		return 3;
// 	}
// }


/******************************************* END OF FILE *******************************************/
