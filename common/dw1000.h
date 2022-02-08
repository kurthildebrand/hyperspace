/************************************************************************************************//**
 * @file		dw1000.h
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
 * @brief
 * @desc		Notes:
 *
 * 				Channel		Center 		Bandwidth	Preamble Codes		Preamble Codes
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
 *
 * 				Change channel
 * 					0x1E – Transmit Power Control
 * 					0x1F – Channel Control (TX_CHAN and RX_CHAN field)
 * 					0x28:0B – RF_RXCTRLH
 * 					0x28:0C – RF_TXCTRL
 * 					0x2A:0B – TC_PGDELAY
 * 					0x2B:07 – FS_PLLCFG
 * 					0x2B:0B – FS_PLLTUNE
 *
 * 				Change preamble length (16, 64, 1024, and 4096)
 * 					0x08 – Transmit Frame Control (TXPSR and PE fields)
 * 					0x10 – RX Frame Information Register (RXPSR and RXNSPL fields)
 * 					0x27:06 – DRX_TUNE1b
 * 					0x27:26 – DRX_TUNE4H
 *
 * 				Change preamble code
 * 					0x10 – RX Frame Information Register (TX_PCODE and RX_PCODE fields)
 * 					0x1F – Channel Control (TX_PCODE and RX_PCODE)
 * 					0x2E:2804 – LDE_REPC (RX_PCODE. Applies to 850 kbps and 6.8 Mbps data rates only)
 *
 * 				Change PRF (16 MHz, 64 MHz)
 * 					0x08 – Transmit Frame Control (TXPRF field)
 * 					0x10 – RX Frame Information Register (RXPRFR field)
 * 					0x1F – Channel Control (RXPRF field)
 * 					0x23:04 – AGC_TUNE1
 * 					0x27:04 – DRX_TUNE1a
 * 					0x27:08 – DRX_TUNE2
 * 					0x2E:1806 – LDE_CFG2
 *
 * 				Change PAC
 * 					0x27:08 – DRX_TUNE2
 *
 * 				Change Data Rate
 * 					0x08 – Transmit Frame Control (TXBR field)
 * 					0x10 – RX Frame Information Register (RXBR field)
 * 					0x1A – Acknowledgement time and response time (ACK_TIM field)
 * 					0x27:02 – DRX_TUNE0b
 *
 * 				dw1000_channel
 * 				dw1000_preamble_length
 * 				dw1000_preamble_code
 * 				dw1000_prf
 * 				dw1000_data_rate
 * 				dw1000_pac
 *
 * 				dw1000_set_channel
 * 				dw1000_set_preamble_length
 * 				dw1000_set_preamble_code
 * 				dw1000_set_prf
 * 				dw1000_set_data_rate
 *
 * 				Sleep Mode:
 * 					SMXX
 *
 * 				To Transmit:
 * 					1.	Write data to Register file: 0x09 – Transmit Data Buffer
 * 					2.	Set preamble length, data rate and PRF in Register file: 0x08 – Transmit
 * 						Frame Control
 * 					3.	Set TXSTRT bit in Register file: 0x0D – System Control Register
 * 					4.	End of transmission is signalled via the TXFRS event status bit in Register
 * 						file: 0x0F – System Event Status Register. The DW1000 returns to idle.
 *
 * 				To Receive:
 * 					1.	Set PAC in Sub-Register 0x27:08 – DRX_TUNE2
 * 					2.	Set preamble detection timeout in Sub-Register 0x27:24 – DRX_PRETOC
 * 					3.	Set SFD timeout in Sub-Register 0x27:20 – DRX_SFDTOC
 * 					4.	End of reception is signalled via the RXDFR and RXFCG event status bits in
 * 						Register file: 0x0F – System Event Status
 *
 ***************************************************************************************************/
#ifndef DW1000_H
#define DW1000_H

#if __STDC_VERSION__ < 199901L
#error Compile with C99 or higher!
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* Includes -------------------------------------------------------------------------------------- */
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>


/* Public Constants ------------------------------------------------------------------------------ */
#define DW1000_TSTAMP_PERIOD   (1ull << 40)
#define DW1000_FC_CH1          (3494.4E6f)
#define DW1000_FC_CH2          (3993.6E6f)
#define DW1000_FC_CH3          (4492.8E6f)
#define DW1000_FC_CH4          (3993.6E6f)
#define DW1000_FC_CH5          (6489.6E6f)
#define DW1000_FC_CH7          (6489.6E6f)

#define DW1000_BW_CH1          (499.2E6f)
#define DW1000_BW_CH2          (499.2E6f)
#define DW1000_BW_CH3          (499.2E6f)
#define DW1000_BW_CH4          (1331.2E6f)
#define DW1000_BW_CH5          (499.2E6f)
#define DW1000_BW_CH7          (1081.6E6f)

#define DW1000_SPI_READ_SHIFT  (7)
#define DW1000_SPI_SUB_SHIFT   (6)
#define DW1000_SPI_EXT_SHIFT   (7)

#define DW1000_SPI_READ_MASK   (0x1 << DW1000_SPI_READ_SHIFT)
#define DW1000_SPI_SUB_MASK    (0x1 << DW1000_SPI_SUB_SHIFT)
#define DW1000_SPI_EXT_MASK    (0x1 << DW1000_SPI_EXT_SHIFT)

#define DW1000_SPI_READ        (0x0 << DW1000_SPI_READ_SHIFT)
#define DW1000_SPI_WRITE       (0x1 << DW1000_SPI_READ_SHIFT)
#define DW1000_SPI_NO_SUB_ADDR (0x0 << DW1000_SPI_SUB_SHIFT)
#define DW1000_SPI_SUB_ADDR    (0x1 << DW1000_SPI_SUB_SHIFT)
#define DW1000_SPI_EXT_1       (0x0 << DW1000_SPI_EXT_SHIFT)
#define DW1000_SPI_EXT_2       (0x1 << DW1000_SPI_EXT_SHIFT)


/* Public Types ---------------------------------------------------------------------------------- */
/* ID			Octets	Type	Mnemonic	Description
 * 0x00			4		RO		DEV_ID		Device Identifier. Includes device type and revision info
 * 0x01			8		RW		EUI			Extended Unique Identifier
 * 0x02			-		-		- 			Reserved
 * 0x03			4		RW		PANADR		PAN Identifier and Short Address
 * 0x04			4		RW 		SYS_CFG		System Configuration bitmap
 * 0x05			-		-		-			Reserved
 * 0x06			5		RO		SYS_TIME	System Time Counter (40-bit)
 * 0x07			-		-		-			Reserved
 * 0x08			5		RW		TX_FCTRL	Transmit Frame Control
 * 0x09			1024	WO		TX_BUFFER	Transmit Data Buffer
 * 0x0A			5		RW		DX_TIME		Delayed Send or Receive Time (40-bit)
 * 0x0B			-		-		-			Reserved
 * 0x0C			2		RW		RX_FWTO		Receive Frame Wait Timeout Period
 * 0x0D			4		SRW		SYS_CTRL	System Control Register
 * 0x0E			4		RW		SYS_MASK	System Event Mask Register
 * 0x0F			5		SRW		SYS_STATUS	System Event Status Register
 * 0x10			4		ROD		RX_FINFO 	RX Frame Information (in double buffer set)
 * 0x11			1024	ROD		RX_BUFFER	Receive Data (in double buffer set)
 * 0x12			8		ROD		RX_FQUAL 	Rx Frame Quality information (in double buffer set)
 * 0x13			4		ROD		RX_TTCKI	Receiver Time Tracking Interval (in double buffer set)
 * 0x14			5		ROD		RX_TTCKO	Receiver Time Tracking Offset (in double buffer set)
 * 0x15			14		ROD 	RX_TIME		Receive Message Time of Arrival (in double buffer set)
 * 0x16			-		-		-			Reserved
 * 0x17 		10		RO 		TX_TIME		Transmit Message Time of Sending
 * 0x18 		2		RW 		TX_ANTD		16-bit Delay from Transmit to Antenna
 * 0x19 		5		RO 		SYS_STATE	System State information
 * 0x1A 		4		RW		ACK_RESP_T	Acknowledgement Time and Response Time
 * 0x1B 		-		- 		-			Reserved
 * 0x1C 		-		-		-			Reserved
 * 0x1D 		4		RW		RX_SNIFF	Pulsed Preamble Reception Configuration
 * 0x1E 		4		RW		TX_POWER	TX Power Control
 * 0x1F			4		RW		CHAN_CTRL	Channel Control
 * 0x20			-		-		-			Reserved
 * 0x21			41		RW 		USR_SFD		User-specified short/long TX/RX SFD sequences
 * 0x22			-		- 		-			Reserved
 * 0x23			33		RW		AGC_CTRL	Automatic Gain Control configuration
 * 0x24			12		RW		EXT_SYNC	External synchronisation control
 * 0x25 		4064 	RO 		ACC_MEM		Read access to accumulator data
 * 0x26 		44 		RW 		GPIO_CTRL	Peripheral register bus 1 access – GPIO control
 * 0x27 		44 		RW 		DRX_CONF	Digital Receiver configuration
 * 0x28 		58 		RW		RF_CONF		Analog RF Configuration
 * 0x29			- 		-		-			Reserved
 * 0x2A			52		RW		TX_CAL		Transmitter calibration block
 * 0x2B 		21		RW 		FS_CTRL		Frequency synthesiser control block
 * 0x2C 		12		RW 		AON			Always-On register set
 * 0x2D			18		RW		OTP_IF		One Time Programmable Memory Interface
 * 0x2E			-		RW		LDE_CTRL	Leading edge detection control block
 * 0x2F			41		RW		DIG_DIAG	Digital Diagnostics Interface
 * 0x30 to 0x35	- 		-		-			Reserved
 * 0x36			48 		RW		PMSC		Power Management System Control Block
 * 0x37 to 0x3F	-		-		-			Reserved */
#define DW1000_NO_SUB_ADDR  (0x00)
#define DW1000_DEV_ID		(0x00)
#define DW1000_EUI			(0x01)
#define DW1000_PANADR		(0x03)
#define DW1000_SYS_CFG		(0x04)
#define DW1000_SYS_TIME		(0x06)
#define DW1000_TX_FCTRL		(0x08)
#define DW1000_TX_BUFFER	(0x09)
#define DW1000_DX_TIME		(0x0A)
#define DW1000_RX_FWTO		(0x0C)
#define DW1000_SYS_CTRL		(0x0D)
#define DW1000_SYS_MASK		(0x0E)
#define DW1000_SYS_STATUS	(0x0F)
#define DW1000_RX_FINFO		(0x10)
#define DW1000_RX_BUFFER	(0x11)
#define DW1000_RX_FQUAL		(0x12)
#define DW1000_RX_TTCKI		(0x13)
#define DW1000_RX_TTCKO		(0x14)
#define DW1000_RX_TIME		(0x15)
#define DW1000_TX_TIME		(0x17)
#define DW1000_TX_ANTD		(0x18)
#define DW1000_SYS_STATE	(0x19)
#define DW1000_ACK_RESP_T	(0x1A)
#define DW1000_RX_SNIFF		(0x1D)
#define DW1000_TX_POWER		(0x1E)
#define DW1000_CHAN_CTRL	(0x1F)
#define DW1000_USR_SFD		(0x21)
#define DW1000_AGC_CTRL		(0x23)
#define DW1000_EXT_SYNC		(0x24)
#define DW1000_ACC_MEM		(0x25)
#define DW1000_GPIO_CTRL	(0x26)
#define DW1000_DRX_CONF		(0x27)
#define DW1000_RF_CONF		(0x28)
#define DW1000_TX_CAL		(0x2A)
#define DW1000_FS_CTRL		(0x2B)
#define DW1000_AON			(0x2C)
#define DW1000_OTP_IF		(0x2D)
#define DW1000_LDE_CTRL		(0x2E)
#define DW1000_DIG_DIAG		(0x2F)
#define DW1000_PMSC			(0x36)


/* OTP Memory Map
 * OTP   | Size | Byte 3         | Byte 2         | Byte 1         | Byte 0         | Programmed By
 * Addr  | (B)  |                |                |                |                |
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x000 | 4    | 64 bit EUID                                                       | Customer
 * 0x001 | 4    |                                                                   |
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x002 | 4    | Alternative 64bit EUID                                            | Customer
 * 0x003 | 4    |                                                                   |
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x004 | 4    |  40 bit LDOTUNE_CAL                                               | Decawave Test
 * 0x005 | 1    |                                                                   |
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x006 | 4    | PART ID / CHIP ID (32bits)                                        | Decawave Test
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x007 | 4    | LOT ID (32 bits)                                                  | Decawave Test
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x008 | 2    |                |                | Vmeas @ 3.7 V  | Vmeas @ 3.3 V  | Decawave Test
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x009 | 1/1  |                                 | Tmeas @        | Tmeas @        | Customer /
 *       |      |                                 | Ant Cal        | 23 °C          | Decawave Test
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x00A | 0    |                                                                   | Reserved
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x00B | 4    |                                                                   | Reserved
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x00C | 2    |                                                                   | Reserved
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x00D | 4    |                                                                   | Reserved
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x00E | 4    |                                                                   | Reserved
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x00F | 4    |                                                                   | Reserved
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x010 | 4    | CH1 TX Power Level PRF 16                                         | Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x011 | 4    | CH1 TX Power Level PRF 64                                         | Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x012 | 4    | CH2 TX Power Level PRF 16                                         | Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x013 | 4    | CH2 TX Power Level PRF 64                                         | Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x014 | 4    | CH3 TX Power Level PRF 16                                         | Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x015 | 4    | CH3 TX Power Level PRF 64                                         | Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x016 | 4    | CH4 TX Power Level PRF 16                                         | Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x017 | 4    | CH4 TX Power Level PRF 64                                         | Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x018 | 4    | CH5 TX Power Level PRF 16                                         | Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x019 | 4    | CH5 TX Power Level PRF 64                                         | Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x01A | 4    | CH7 TX Power Level PRF 16                                         | Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x01B | 4    | CH7 TX Power Level PRF 64                                         | Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x01C | 4    | Antenna Delay – PRF 64          | Antenna Delay – PRF 16          | Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x01D | 0    |                |                |                |                | Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x01E | 2    |                |                | OTP Revision   | XTAL_Trim[4:0] | Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x01F | 0    |                |                |                |                | Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * :     :      :                :                :                :                | Reserved
 * ------+------+----------------+----------------+----------------+----------------+--------------
 * 0x400 | 4    | SR (SPI mode OTP setting) Customer
 * ------+------+----------------+----------------+----------------+----------------+--------------
 */
#define DW1000_OTP_EUI_64_ADDR              (0x0000)
#define DW1000_OTP_ALT_EUI_64_ADDR          (0x0002)
#define DW1000_OTP_LDO_TUNE_ADDR            (0x0004)
#define DW1000_OTP_PART_ID_ADDR             (0x0006)
#define DW1000_OTP_LOT_ID_ADDR              (0x0007)
#define DW1000_OTP_VMEAS_ADDR               (0x0008)
#define DW1000_OTP_TMEAS_ADDR               (0x0009)
#define DW1000_OTP_CH1_TX_PWR_PRF_16_ADDR   (0x0010)
#define DW1000_OTP_CH1_TX_PWR_PRF_64_ADDR   (0x0011)
#define DW1000_OTP_CH2_TX_PWR_PRF_16_ADDR   (0x0012)
#define DW1000_OTP_CH2_TX_PWR_PRF_64_ADDR   (0x0013)
#define DW1000_OTP_CH3_TX_PWR_PRF_16_ADDR   (0x0014)
#define DW1000_OTP_CH3_TX_PWR_PRF_64_ADDR   (0x0015)
#define DW1000_OTP_CH4_TX_PWR_PRF_16_ADDR   (0x0016)
#define DW1000_OTP_CH4_TX_PWR_PRF_64_ADDR   (0x0017)
#define DW1000_OTP_CH5_TX_PWR_PRF_16_ADDR   (0x0018)
#define DW1000_OTP_CH5_TX_PWR_PRF_64_ADDR   (0x0019)
#define DW1000_OTP_CH7_TX_PWR_PRF_16_ADDR   (0x001A)
#define DW1000_OTP_CH7_TX_PWR_PRF_64_ADDR   (0x001B)
#define DW1000_OTP_ANT_DELAY_ADDR           (0x001C)
#define DW1000_OTP_XTRIM_ADDR               (0x001E)


/* 0x00 Device Identifier
 * Octets		Type		Mnemonic	Description
 * 4			RO			DEV_ID		Device Identifier (includes device type and rev)
 *
 * reg:00:00	bits:3–0	REV			Revision
 * reg:00:00	bits:7–4	VER			Version
 * reg:00:00	bits:15–8	MODEL		Model
 * reg:00:00	bits:31–16	RIDTAG		Register Identification Tag */
#define DW1000_DEV_ID_REV_SHIFT    (0)
#define DW1000_DEV_ID_VER_SHIFT    (4)
#define DW1000_DEV_ID_MOD_SHIFT    (8)
#define DW1000_DEV_ID_RIDTAG_SHIFT (16)

#define DW1000_DEV_ID_REV_MASK    (0xF    << DW1000_DEV_ID_REV_SHIFT)
#define DW1000_DEV_ID_VER_MASK    (0xF    << DW1000_DEV_ID_VER_SHIFT)
#define DW1000_DEV_ID_MOD_MASK    (0xFF   << DW1000_DEV_ID_MOD_SHIFT)
#define DW1000_DEV_ID_RIDTAG_MASK (0xFFFF << DW1000_DEV_ID_RIDTAG_SHIFT)

#define DW1000_DEV_ID_DEFAULT (0xDECA0130)


/* 0x01 Extended Unique Identifier
 * Octets		Type		Mnemonic	Description
 * 8			RW			EUI			Extended Unique Identifier (64-bit IEEE device address)
 *
 * 76543210		Octet 		Description
 * 0xHH			0			Bits 7 to 0 of the extension identifier
 * 0xHH			1			Bits 15 to 8 of the extension identifier
 * 0xHH			2			Bits 23 to 16 of the extension identifier
 * 0xHH			3			Bits 31 to 24 of the extension identifier
 * 0xHH			4			Bits 39 to 32 of the extension identifier
 * 0xNN			5			Bits 7 to 0 of the OUI (manufacturer company ID)
 * 0xNN			6			Bits 15 to 8 of the OUI (manufacturer company ID)
 * 0xNN			7			Bits 23 to 16 of the OUI (manufacturer company ID) */


/* 0x03 PANADR
 * Octets		Type		Mnemonic	Description
 * 4			RW			PANADR		PAN Identifier and Short Address
 *
 * reg:03:00	bits:15–0	SHORT_ADDR	Short Address
 * reg:03:00	bits:31–16	PAN_ID		PAN Identifier */
#define DW1000_PANADR_SHORT_ADDR_SHIFT	(0)
#define DW1000_PANADR_PAN_ID_SHIFT		(16)

#define DW1000_PANADR_SHORT_ADDR_MASK	(0xFFFF << DW1000_PANADR_SHORT_ADDR_SHIFT)
#define DW1000_PANADR_PAN_ID_MASK		(0xFFFF << DW1000_PANADR_PAN_ID_SHIFT)


/* 0x04 SYS_CFG
 * Octets		Type		Mnemonic	Description
 * 4			RW			SYS_CFG		System Configuration Bitmap
 *
 * reg:04:00	bit  0		FFEN		Frame Filtering Enable
 * reg:04:00	bit  1		FFBC		Frame Filtering Behave as a Coordinator
 * reg:04:00	bit  2		FFAB		Frame Filtering Allow Beacon frame reception
 * reg:04:00	bit  3		FFAD		Frame Filtering Allow Data frame reception
 * reg:04:00	bit  4		FFAA		Frame Filtering Allow Acknowledgment frame reception
 * reg:04:00	bit  5		FFAM		Frame Filtering Allow MAC command frame reception
 * reg:04:00	bit  6		FFAR		Frame Filtering Allow Reserved frame types
 * reg:04:00	bit  7		FFA4		Frame Filtering Allow frames with frame type field of 4
 * reg:04:00	bit  8		FFA5		Frame Filtering Allow frames with frame type field of 5
 * reg:04:00	bit  9		HIRQ_POL	Host interrupt polarity
 * reg:04:00	bit  10		SPI_EDGE	SPI data launch edge
 * reg:04:00	bit  11		DIS_FCE		Disable frame check error handling
 * reg:04:00	bit  12		DIS_DRXB	Disable Double RX Buffer
 * reg:04:00	bit  13		DIS_PHE		Disable receiver abort on PHR error
 * reg:04:00	bit  14		DIS_RSDE	Disable Receiver Abort on RSD error
 * reg:04:00	bit  15		FCS_INIT2F	Initialize FCS to 0xFFFF
 * reg:04:00	bits 17-16	PHR_MODE	Enable proprietary PHR encoding (long frames)
 * reg:04:00	bit	 18		DIS_STXP	Disable Smart TX Power control
 * reg:04:00	bit  22		RXM110K		Receiver Mode 110 kbps data rate
 * reg:04:00	bit  28		RXWTOE		Receive Wait Timeout Enable
 * reg:04:00	bit  29		RXAUTR		Receiver Auto-Re-enable
 * reg:04:00	bit  30		AUTOACK		Automatic Acknowledgement Enable
 * reg:04:00	bit  31		AACKPEND 	Automatic Acknowledgement Pending bit control */
#define DW1000_SYS_CFG_FFEN_SHIFT		(0)
#define DW1000_SYS_CFG_FFBC_SHIFT		(1)
#define DW1000_SYS_CFG_FFAB_SHIFT		(2)
#define DW1000_SYS_CFG_FFAD_SHIFT		(3)
#define DW1000_SYS_CFG_FFAA_SHIFT		(4)
#define DW1000_SYS_CFG_FFAM_SHIFT		(5)
#define DW1000_SYS_CFG_FFAR_SHIFT		(6)
#define DW1000_SYS_CFG_FFA4_SHIFT		(7)
#define DW1000_SYS_CFG_FFA5_SHIFT		(8)
#define DW1000_SYS_CFG_HIRQ_POL_SHIFT	(9)
#define DW1000_SYS_CFG_SPI_EDGE_SHIFT	(10)
#define DW1000_SYS_CFG_DIS_FCE_SHIFT	(11)
#define DW1000_SYS_CFG_DIS_DRXB_SHIFT	(12)
#define DW1000_SYS_CFG_DIS_PHE_SHIFT	(13)
#define DW1000_SYS_CFG_DIS_RSDE_SHIFT	(14)
#define DW1000_SYS_CFG_FCS_INIT2F_SHIFT	(15)
#define DW1000_SYS_CFG_PHR_MODE_SHIFT	(16)
#define DW1000_SYS_CFG_DIS_STXP_SHIFT	(18)
#define DW1000_SYS_CFG_RXM110K_SHIFT	(22)
#define DW1000_SYS_CFG_RXWTOE_SHIFT		(28)
#define DW1000_SYS_CFG_RXAUTR_SHIFT		(29)
#define DW1000_SYS_CFG_AUTOACK_SHIFT	(30)
#define DW1000_SYS_CFG_AACKPEND_SHIFT	(31)

#define DW1000_SYS_CFG_FFEN_MASK		(0x1 << DW1000_SYS_CFG_FFEN_SHIFT)
#define DW1000_SYS_CFG_FFBC_MASK		(0x1 << DW1000_SYS_CFG_FFBC_SHIFT)
#define DW1000_SYS_CFG_FFAB_MASK		(0x1 << DW1000_SYS_CFG_FFAB_SHIFT)
#define DW1000_SYS_CFG_FFAD_MASK		(0x1 << DW1000_SYS_CFG_FFAD_SHIFT)
#define DW1000_SYS_CFG_FFAA_MASK		(0x1 << DW1000_SYS_CFG_FFAA_SHIFT)
#define DW1000_SYS_CFG_FFAM_MASK		(0x1 << DW1000_SYS_CFG_FFAM_SHIFT)
#define DW1000_SYS_CFG_FFAR_MASK		(0x1 << DW1000_SYS_CFG_FFAR_SHIFT)
#define DW1000_SYS_CFG_FFA4_MASK		(0x1 << DW1000_SYS_CFG_FFA4_SHIFT)
#define DW1000_SYS_CFG_FFA5_MASK		(0x1 << DW1000_SYS_CFG_FFA5_SHIFT)
#define DW1000_SYS_CFG_HIRQ_POL_MASK	(0x1 << DW1000_SYS_CFG_HIRQ_POL_SHIFT)
#define DW1000_SYS_CFG_SPI_EDGE_MASK	(0x1 << DW1000_SYS_CFG_SPI_EDGE_SHIFT)
#define DW1000_SYS_CFG_DIS_FCE_MASK		(0x1 << DW1000_SYS_CFG_DIS_FCE_SHIFT)
#define DW1000_SYS_CFG_DIS_DRXB_MASK	(0x1 << DW1000_SYS_CFG_DIS_DRXB_SHIFT)
#define DW1000_SYS_CFG_DIS_PHE_MASK		(0x1 << DW1000_SYS_CFG_DIS_PHE_SHIFT)
#define DW1000_SYS_CFG_DIS_RSDE_MASK	(0x1 << DW1000_SYS_CFG_DIS_RSDE_SHIFT)
#define DW1000_SYS_CFG_FCS_INIT2F_MASK	(0x1 << DW1000_SYS_CFG_FCS_INIT2F_SHIFT)
#define DW1000_SYS_CFG_PHR_MODE_MASK	(0x3 << DW1000_SYS_CFG_PHR_MODE_SHIFT)
#define DW1000_SYS_CFG_DIS_STXP_MASK	(0x1 << DW1000_SYS_CFG_DIS_STXP_SHIFT)
#define DW1000_SYS_CFG_RXM110K_MASK		(0x1 << DW1000_SYS_CFG_RXM110K_SHIFT)
#define DW1000_SYS_CFG_RXWTOE_MASK		(0x1 << DW1000_SYS_CFG_RXWTOE_SHIFT)
#define DW1000_SYS_CFG_RXAUTR_MASK		(0x1 << DW1000_SYS_CFG_RXAUTR_SHIFT)
#define DW1000_SYS_CFG_AUTOACK_MASK		(0x1 << DW1000_SYS_CFG_AUTOACK_SHIFT)
#define DW1000_SYS_CFG_AACKPEND_MASK	(0x1 << DW1000_SYS_CFG_AACKPEND_SHIFT)

/* Frame filtering: see section 5.2 */
#define DW1000_SYS_CFG_FFEN_DISABLE                (0x0 << DW1000_SYS_CFG_FFEN_SHIFT)
#define DW1000_SYS_CFG_FFEN_ENABLE                 (0x1 << DW1000_SYS_CFG_FFEN_SHIFT)
#define DW1000_SYS_CFG_FFBC_NODE                   (0x0 << DW1000_SYS_CFG_FFBC_SHIFT)
#define DW1000_SYS_CFG_FFBC_COORDINATOR            (0x1 << DW1000_SYS_CFG_FFBC_SHIFT)
#define DW1000_SYS_CFG_FFAB_IGNORE_BEACONS         (0x0 << DW1000_SYS_CFG_FFAB_SHIFT)
#define DW1000_SYS_CFG_FFAB_ACCEPT_BEACONS         (0x1 << DW1000_SYS_CFG_FFAB_SHIFT)
#define DW1000_SYS_CFG_FFAD_IGNORE_DATA            (0x0 << DW1000_SYS_CFG_FFAD_SHIFT)
#define DW1000_SYS_CFG_FFAD_ACCEPT_DATA            (0x1 << DW1000_SYS_CFG_FFAD_SHIFT)
#define DW1000_SYS_CFG_FFAA_IGNORE_ACKS            (0x0 << DW1000_SYS_CFG_FFAA_SHIFT)
#define DW1000_SYS_CFG_FFAA_ACCEPT_ACKS            (0x1 << DW1000_SYS_CFG_FFAA_SHIFT)
#define DW1000_SYS_CFG_FFAM_IGNORE_MAC             (0x0 << DW1000_SYS_CFG_FFAM_SHIFT)
#define DW1000_SYS_CFG_FFAM_ACCEPT_MAC             (0x1 << DW1000_SYS_CFG_FFAM_SHIFT)
#define DW1000_SYS_CFG_FFAR_IGNORE_RESERVED        (0x0 << DW1000_SYS_CFG_FFAR_SHIFT)
#define DW1000_SYS_CFG_FFAR_ACCEPT_RESERVED        (0x1 << DW1000_SYS_CFG_FFAR_SHIFT)
#define DW1000_SYS_CFG_FFA4_IGNORE_TYPE_4          (0x0 << DW1000_SYS_CFG_FFA4_SHIFT)
#define DW1000_SYS_CFG_FFA4_ACCEPT_TYPE_4          (0x1 << DW1000_SYS_CFG_FFA4_SHIFT)
#define DW1000_SYS_CFG_FFA5_IGNORE_TYPE_5          (0x0 << DW1000_SYS_CFG_FFA5_SHIFT)
#define DW1000_SYS_CFG_FFA5_ACCEPT_TYPE_5          (0x1 << DW1000_SYS_CFG_FFA5_SHIFT)
#define DW1000_SYS_CFG_HIRQ_POL_ACTIVE_LOW         (0x0 << DW1000_SYS_CFG_HIRQ_POL_SHIFT)
#define DW1000_SYS_CFG_HIRQ_POL_ACTIVE_HIGH        (0x1 << DW1000_SYS_CFG_HIRQ_POL_SHIFT)		/* Recommended for low power applications */
#define DW1000_SYS_CFG_SPI_EDGE_SAMPLING           (0x0 << DW1000_SYS_CFG_SPI_EDGE_SHIFT)		/* @TODO: clarify. */
#define DW1000_SYS_CFG_SPI_EDGE_OPPOSITE           (0x1 << DW1000_SYS_CFG_SPI_EDGE_SHIFT)		/* @TODO: clarify. May give more robust operation */
#define DW1000_SYS_CFG_DIS_FCE_CHECK_ERRORS        (0x0 << DW1000_SYS_CFG_DIS_FCE_SHIFT)		/* Normal IEEE 802.15.4-2001 operation */
#define DW1000_SYS_CFG_DIS_FCE_IGNORE_ERRORS       (0x1 << DW1000_SYS_CFG_DIS_FCE_SHIFT)		/* Treat all frames as valid */
#define DW1000_SYS_CFG_DIS_DRXB_ENABLE_DBUF        (0x0 << DW1000_SYS_CFG_DIS_DRXB_SHIFT)		/* Section 4.3. Double buffering is enabled */
#define DW1000_SYS_CFG_DIS_DRXB_DISABLE_DBUF       (0x1 << DW1000_SYS_CFG_DIS_DRXB_SHIFT)		/* Section 4.3. Double buffering is diesabled */
#define DW1000_SYS_CFG_DIS_PHE_RX_ABORT_ON_ERR     (0x0 << DW1000_SYS_CFG_DIS_PHE_SHIFT)		/* Recommended */
#define DW1000_SYS_CFG_DIS_PHE_RX_CONTINUE_ON_ERR  (0x1 << DW1000_SYS_CFG_DIS_PHE_SHIFT)		/* Debug only */
#define DW1000_SYS_CFG_DIS_RSDE_RX_ABORT_ON_ERR    (0x0 << DW1000_SYS_CFG_DIS_RSDE_SHIFT)		/* Abort receiver on Reed Solomon decoder error */
#define DW1000_SYS_CFG_DIS_RSDE_RX_CONTINUE_ON_ERR (0x1 << DW1000_SYS_CFG_DIS_RSDE_SHIFT)		/* Do not abort on Reed Solomon error */
#define DW1000_SYS_CFG_FCS_INIT2F_0000             (0x0 << DW1000_SYS_CFG_FCS_INIT2F_SHIFT)	/* FCS initialized to 0x0000 */
#define DW1000_SYS_CFG_FCS_INIT2F_FFFF             (0x1 << DW1000_SYS_CFG_FCS_INIT2F_SHIFT)	/* FCS initialized to 0xFFFF */
#define DW1000_SYS_CFG_PHR_MODE_STANDARD           (0x0 << DW1000_SYS_CFG_PHR_MODE_SHIFT)		/* IEEE 802.15.4 complicance */
#define DW1000_SYS_CFG_PHR_MODE_LONG               (0x1 << DW1000_SYS_CFG_PHR_MODE_SHIFT)		/* Frame length 0-1023. Proprietary PHR encoding */
#define DW1000_SYS_CFG_DIS_STXP_ENABLE_AUTO_PWR    (0x0 << DW1000_SYS_CFG_DIS_STXP_SHIFT)
#define DW1000_SYS_CFG_DIS_STXP_DISABLE_AUTO_PWR   (0x1 << DW1000_SYS_CFG_DIS_STXP_SHIFT)
#define DW1000_SYS_CFG_RXM110K_SHORT_SFD           (0x0 << DW1000_SYS_CFG_RXM110K_SHIFT)
#define DW1000_SYS_CFG_RXM110K_LONG_SFD            (0x1 << DW1000_SYS_CFG_RXM110K_SHIFT)
#define DW1000_SYS_CFG_RXWTOE_DISABLE_RX_WAIT      (0x0 << DW1000_SYS_CFG_RXWTOE_SHIFT)
#define DW1000_SYS_CFG_RXWTOE_ENABLE_RX_WAIT       (0x1 << DW1000_SYS_CFG_RXWTOE_SHIFT)
#define DW1000_SYS_CFG_RXAUTR_DISABLE_RX_AUTO_EN   (0x0 << DW1000_SYS_CFG_RXAUTR_SHIFT)
#define DW1000_SYS_CFG_RXAUTR_ENABLE_RX_AUTO_EN    (0x1 << DW1000_SYS_CFG_RXAUTR_SHIFT)
#define DW1000_SYS_CFG_AUTOACK_DISABLE             (0x0 << DW1000_SYS_CFG_AUTOACK_SHIFT)
#define DW1000_SYS_CFG_AUTOACK_ENABLE              (0x1 << DW1000_SYS_CFG_AUTOACK_SHIFT)
#define DW1000_SYS_CFG_AACKPEND_IGNORE_FPEND       (0x0 << DW1000_SYS_CFG_AACKPEND_SHIFT)
#define DW1000_SYS_CFG_AACKPEND_COPY_FPEND         (0x1 << DW1000_SYS_CFG_AACKPEND_SHIFT)


/* 0x06 SYS_TIME
 * Octets		Type		Mnemonic	Description
 * 5			RO			SYS_TIME	Systime Time Counter (40-bit) */


/* 0x08 TX_FCTRL
 * Octets		Type		Mnemonic	Description
 * 5			RW			TX_FCTRL	Transmit Frame Control
 *
 * reg:08:00	bits 6-0	TFLEN		Transmit Frame Length
 * reg:08:00	bits 9-7	TFLE		Transmit Frame Length Extension
 * reg:08:00	bits 14-13	TXBR		Transmit Bit Rate
 * reg:08:00	bit  15		TR			Transmit Ranging enable
 * reg:08:00	bits 17-16	TXPRF		Transmit Pulse Repetition Frequency
 * reg:08:00	bits 19-18	TXPSR		Transmit Preamble Symbol Repetitions (PSR)
 * reg:08:00	bits 21-20	PE			Preamble Extension
 * reg:08:00	bits 31-22	TXBOFFS		Transmit buffer index offset
 * reg:08:04	bits 7-0	IFSDELAY	Inter-Frame Spacing */
/* reg 0x08:00 */
#define DW1000_TX_FCTRL_TFLEN_SHIFT		(0)
#define DW1000_TX_FCTRL_TFLE_SHIFT		(7)
#define DW1000_TX_FCTRL_TXBR_SHIFT		(13)
#define DW1000_TX_FCTRL_TR_SHIFT		(15)
#define DW1000_TX_FCTRL_TXPRF_SHIFT		(16)
#define DW1000_TX_FCTRL_TXPSR_SHIFT		(18)
#define DW1000_TX_FCTRL_PE_SHIFT		(20)
#define DW1000_TX_FCTRL_TXBOFFS_SHIFT	(22)

/* reg 0x08:04 */
#define DW1000_TX_FCTRL_IFSDELAY_SHIFT	(0)

/* reg 0x08:00 */
#define DW1000_TX_FCTRL_TFLEN_MASK		(0x7F  << DW1000_TX_FCTRL_TFLEN_SHIFT)
#define DW1000_TX_FCTRL_TFLE_MASK		(0x7   << DW1000_TX_FCTRL_TFLE_SHIFT)
#define DW1000_TX_FCTRL_TXBR_MASK		(0x3   << DW1000_TX_FCTRL_TXBR_SHIFT)
#define DW1000_TX_FCTRL_TR_MASK			(0x1   << DW1000_TX_FCTRL_TR_SHIFT)
#define DW1000_TX_FCTRL_TXPRF_MASK		(0x3   << DW1000_TX_FCTRL_TXPRF_SHIFT)
#define DW1000_TX_FCTRL_TXPSR_MASK		(0x3   << DW1000_TX_FCTRL_TXPSR_SHIFT)
#define DW1000_TX_FCTRL_PE_MASK			(0x3   << DW1000_TX_FCTRL_PE_SHIFT)
#define DW1000_TX_FCTRL_TXBOFFS_MASK	(0x3FF << DW1000_TX_FCTRL_TXBOFFS_SHIFT)

/* reg 0x08:04 */
#define DW1000_TX_FCTRL_IFSDELAY_MASK	(0xFF  << DW1000_TX_FCTRL_IFSDELAY_SHIFT)

#define DW1000_TX_FCTRL_TXBR_100K          (0x0 << DW1000_TX_FCTRL_TXBR_SHIFT)
#define DW1000_TX_FCTRL_TXBR_850K          (0x1 << DW1000_TX_FCTRL_TXBR_SHIFT)
#define DW1000_TX_FCTRL_TXBR_6800K         (0x2 << DW1000_TX_FCTRL_TXBR_SHIFT)
#define DW1000_TX_FCTRL_TR_DISABLE_RANGING (0x0 << DW1000_TX_FCTRL_TR_SHIFT)
#define DW1000_TX_FCTRL_TR_ENABLE_RANGING  (0x1 << DW1000_TX_FCTRL_TR_SHIFT)
#define DW1000_TX_FCTRL_TXPRF_4_MHZ        (0x0 << DW1000_TX_FCTRL_TXPRF_SHIFT)
#define DW1000_TX_FCTRL_TXPRF_16_MHZ       (0x1 << DW1000_TX_FCTRL_TXPRF_SHIFT)
#define DW1000_TX_FCTRL_TXPRF_64_MHZ       (0x2 << DW1000_TX_FCTRL_TXPRF_SHIFT)
#define DW1000_TX_FCTRL_TXPSR_64           (0x1 << DW1000_TX_FCTRL_TXPSR_SHIFT)
#define DW1000_TX_FCTRL_TXPSR_1024         (0x2 << DW1000_TX_FCTRL_TXPSR_SHIFT)
#define DW1000_TX_FCTRL_TXPSR_4096         (0x3 << DW1000_TX_FCTRL_TXPSR_SHIFT)
#define DW1000_TX_FCTRL_PE_64              (0x1 << DW1000_TX_FCTRL_TXPSR_SHIFT)
#define DW1000_TX_FCTRL_PE_128             (0x5 << DW1000_TX_FCTRL_TXPSR_SHIFT)
#define DW1000_TX_FCTRL_PE_256             (0x9 << DW1000_TX_FCTRL_TXPSR_SHIFT)
#define DW1000_TX_FCTRL_PE_512             (0xD << DW1000_TX_FCTRL_TXPSR_SHIFT)
#define DW1000_TX_FCTRL_PE_1024            (0x2 << DW1000_TX_FCTRL_TXPSR_SHIFT)
#define DW1000_TX_FCTRL_PE_1536            (0x6 << DW1000_TX_FCTRL_TXPSR_SHIFT)
#define DW1000_TX_FCTRL_PE_2048            (0xA << DW1000_TX_FCTRL_TXPSR_SHIFT)
#define DW1000_TX_FCTRL_PE_4096            (0x3 << DW1000_TX_FCTRL_TXPSR_SHIFT)


/* 0x09 Transmit Data Buffer
 * Octets		Type		Mnemonic	Description
 * 1024			WO			TX_BUFFER	Transmit Data Buffer */


/* 0x0A Delayed Send or Receive Time
 * Octets		Type		Mnemonic	Description
 * 5			RW			DX_TIME		Delayed Send or Receive Time (40-bit) */


/* 0x0C Receive Frame Wait Timeout Period RX_FWTO
 * Octets		Type		Mnemonic	Description
 * 2			RW			RX_FWTO		Receive Frame Wait Timeout Period
 *
 * reg 0C:00	bits 15-0	RXFWTO		Receive Frame Wait Timeout period (~1us) */
#define DW1000_RX_FWTO_SHIFT (0)

#define DW1000_RX_FWTO_MASK  (0xFF << DW1000_RX_FWTO_SHIFT)


/* 0x0D System Control Register
 * Octets		Type		Mnemonic	Description
 * 4			RSRW		SYS_CTRL	System Control Register
 *
 * reg:0D:00	bit 0		SFCST		Suppress auto-FCS Transmission (next frame)
 * reg:0D:00	bit 1		TXSTRT		Transmit Start
 * reg:0D:00	bit 2		TXDLYS		Transmitter Delayed Sending
 * reg:0D:00	bit 3		CANSFCS		Cancel Suppression of auto-FCS (current frame)
 * reg:0D:00	bit 6		TRXOFF		Transceiver Off
 * reg:0D:00	bit 7		WAIT4RESP	Wait for Response
 * reg:0D:00	bit 8		RXENAB		Enable Receiver
 * reg:0D:00	bit 9		RXDLYE		Receiver Delayed Enable
 * reg:0D:00	bit 24		HRBPT		Host Side Receive Buffer Pointer Toggle */
#define DW1000_SYS_CTRL_SFCST_SHIFT		(0)
#define DW1000_SYS_CTRL_TXSTRT_SHIFT	(1)
#define DW1000_SYS_CTRL_TXDLYS_SHIFT	(2)
#define DW1000_SYS_CTRL_CANSFCS_SHIFT	(3)
#define DW1000_SYS_CTRL_TRXOFF_SHIFT	(6)
#define DW1000_SYS_CTRL_WAIT4RESP_SHIFT	(7)
#define DW1000_SYS_CTRL_RXENAB_SHIFT	(8)
#define DW1000_SYS_CTRL_RXDLYE_SHIFT	(9)
#define DW1000_SYS_CTRL_HRBPT_SHIFT		(24)

#define DW1000_SYS_CTRL_SFCST_MASK		(0x1 << DW1000_SYS_CTRL_SFCST_SHIFT)
#define DW1000_SYS_CTRL_TXSTRT_MASK		(0x1 << DW1000_SYS_CTRL_TXSTRT_SHIFT)
#define DW1000_SYS_CTRL_TXDLYS_MASK		(0x1 << DW1000_SYS_CTRL_TXDLYS_SHIFT)
#define DW1000_SYS_CTRL_CANSFCS_MASK	(0x1 << DW1000_SYS_CTRL_CANSFCS_SHIFT)
#define DW1000_SYS_CTRL_TRXOFF_MASK		(0x1 << DW1000_SYS_CTRL_TRXOFF_SHIFT)
#define DW1000_SYS_CTRL_WAIT4RESP_MASK	(0x1 << DW1000_SYS_CTRL_WAIT4RESP_SHIFT)
#define DW1000_SYS_CTRL_RXENAB_MASK		(0x1 << DW1000_SYS_CTRL_RXENAB_SHIFT)
#define DW1000_SYS_CTRL_RXDLYE_MASK		(0x1 << DW1000_SYS_CTRL_RXDLYE_SHIFT)
#define DW1000_SYS_CTRL_HRBPT_MASK		(0x1 << DW1000_SYS_CTRL_HRBPT_SHIFT)

#define DW1000_SYS_CTRL_SFCST_ENABLE_AUTO_FCS  (0x0 << DW1000_SYS_CTRL_SFCST_SHIFT)	/* @TODO: When SFCST is clear TFLEN-2 octets are fetched ans sent from the TX buffer, and the final two octets are the automatically generated FCS bytes */
#define DW1000_SYS_CTRL_SFCST_DISABLE_AUFO_FCS (0x1 << DW1000_SYS_CTRL_SFCST_SHIFT)
#define DW1000_SYS_CTRL_TXSTRT                 (0x1 << DW1000_SYS_CTRL_TXSTRT_SHIFT)
#define DW1000_SYS_CTRL_TXDLYS                 (0x1 << DW1000_SYS_CTRL_TXDLYS_SHIFT)
#define DW1000_SYS_CTRL_CANSFCS                (0x1 << DW1000_SYS_CTRL_CANSFCS_SHIFT)	/* Section 3.5 */
#define DW1000_SYS_CTRL_TRXOFF                 (0x1 << DW1000_SYS_CTRL_TRXOFF_SHIFT)
#define DW1000_SYS_CTRL_WAIT4RESP              (0x1 << DW1000_SYS_CTRL_WAIT4RESP_SHIFT)
#define DW1000_SYS_CTRL_RXENAB                 (0x1 << DW1000_SYS_CTRL_RXENAB_SHIFT)
#define DW1000_SYS_CTRL_RXDLYE                 (0x1 << DW1000_SYS_CTRL_RXDLYE_SHIFT)
#define DW1000_SYS_CTRL_HRBPT                  (0x1 << DW1000_SYS_CTRL_HRBPT_SHIFT)


/* 0x0E System Event Mask Register
 * Octets		Type		Mnemonic	Description
 * 4			RW			SYS_MASK	System Event Mask Register
 *
 * reg:0E:00	bit 1		MCPLOCK		Mask clock PLL lock event
 * reg:0E:00	bit 2		MESYNCR		Mask external sync clock reset event
 * reg:0E:00	bit 3		MAAT		Mask automatic acknowledge trigger event
 * reg:0E:00	bit 4		MTXFRB		Mask transmit frame begins event
 * reg:0E:00	bit 5		MTXPRS		Mask transmit preamble sent event.
 * reg:0E:00	bit 6		MTXPHS		Mask transmit PHY Header Sent event
 * reg:0E:00	bit 7		MTXFRS		Mask transmit frame sent event
 * reg:0E:00	bit 8		MRXPRD		Mask receiver preamble detected event
 * reg:0E:00	bit 9		MRXSFDD		Mask receiver SFD detected event
 * reg:0E:00	bit 10		MLDEDONE	Mask LDE processing done event
 * reg:0E:00	bit 11		MRXPHD		Mask receiver PHY header detect event
 * reg:0E:00	bit 12		MRXPHE		Mask receiver PHY header error event
 * reg:0E:00	bit 13		MRXDFR		Mask receiver data frame ready event
 * reg:0E:00	bit 14		MRXFCG		Mask receiver FCS good event
 * reg:0E:00	bit 15		MRXFCE		Mask receiver FCS error event
 * reg:0E:00	bit 16		MRXRFSL		Mask receiver Reed Solomon Frame Sync Loss event
 * reg:0E:00	bit 17		MRXRFTO		Mask Receive Frame Wait Timeout event
 * reg:0E:00	bit 18		MLDEERR		Mask leading edge detection processing error event
 * reg:0E:00	bit 20		MRXOVRR		Mask Receiver Overrun event
 * reg:0E:00	bit 21		MRXPTO		Mask Preamble detection timeout event
 * reg:0E:00	bit 22		MGPIOIRQ	Mask GPIO interrupt event
 * reg:0E:00	bit 23		MSLP2INIT	Mask SLEEP to INIT event
 * reg:0E:00	bit 24		MRFPLLLL	Mask RF PLL Losing Lock warning event
 * reg:0E:00	bit 25		MCPLLLL		Mask Clock PLL Losing Lock warning event
 * reg:0E:00	bit 26		MRXSFDTO	Mask Receive SFD timeout event
 * reg:0E:00	bit 27		MHPDWARN	Mask Half Period Delay Warning event
 * reg:0E:00	bit 28		MTXBERR		Mask Transmit Buffer Error event
 * reg:0E:00	bit 29		MAFFREJ		Mask Automatic Frame Filtering rejection event */
#define DW1000_SYS_MASK_MCPLOCK_SHIFT	(1)
#define DW1000_SYS_MASK_MESYNCR_SHIFT	(2)
#define DW1000_SYS_MASK_MAAT_SHIFT		(3)
#define DW1000_SYS_MASK_MTXFRB_SHIFT	(4)
#define DW1000_SYS_MASK_MTXPRS_SHIFT	(5)
#define DW1000_SYS_MASK_MTXPHS_SHIFT	(6)
#define DW1000_SYS_MASK_MTXFRS_SHIFT	(7)
#define DW1000_SYS_MASK_MRXPRD_SHIFT	(8)
#define DW1000_SYS_MASK_MRXSFDD_SHIFT	(9)
#define DW1000_SYS_MASK_MLDEDONE_SHIFT	(10)
#define DW1000_SYS_MASK_MRXPHD_SHIFT	(11)
#define DW1000_SYS_MASK_MRXPHE_SHIFT	(12)
#define DW1000_SYS_MASK_MRXDFR_SHIFT	(13)
#define DW1000_SYS_MASK_MRXFCG_SHIFT	(14)
#define DW1000_SYS_MASK_MRXFCE_SHIFT	(15)
#define DW1000_SYS_MASK_MRXRFSL_SHIFT	(16)
#define DW1000_SYS_MASK_MRXRFTO_SHIFT	(17)
#define DW1000_SYS_MASK_MLDEERR_SHIFT	(18)
#define DW1000_SYS_MASK_MRXOVRR_SHIFT	(20)
#define DW1000_SYS_MASK_MRXPTO_SHIFT	(21)
#define DW1000_SYS_MASK_MGPIOIRQ_SHIFT	(22)
#define DW1000_SYS_MASK_MSLP2INIT_SHIFT	(23)
#define DW1000_SYS_MASK_MRFPLLLL_SHIFT	(24)
#define DW1000_SYS_MASK_MCPLLLL_SHIFT	(25)
#define DW1000_SYS_MASK_MRXSFDTO_SHIFT	(26)
#define DW1000_SYS_MASK_MHPDWARN_SHIFT	(27)
#define DW1000_SYS_MASK_MTXBERR_SHIFT	(28)
#define DW1000_SYS_MASK_MAFFREJ_SHIFT	(29)

#define DW1000_SYS_MASK_MCPLOCK_MASK	(0x1 << DW1000_SYS_MASK_MCPLOCK_SHIFT)
#define DW1000_SYS_MASK_MESYNCR_MASK	(0x1 << DW1000_SYS_MASK_MESYNCR_SHIFT)
#define DW1000_SYS_MASK_MAAT_MASK		(0x1 << DW1000_SYS_MASK_MAAT_SHIFT	)
#define DW1000_SYS_MASK_MTXFRB_MASK		(0x1 << DW1000_SYS_MASK_MTXFRB_SHIFT)
#define DW1000_SYS_MASK_MTXPRS_MASK		(0x1 << DW1000_SYS_MASK_MTXPRS_SHIFT)
#define DW1000_SYS_MASK_MTXPHS_MASK		(0x1 << DW1000_SYS_MASK_MTXPHS_SHIFT)
#define DW1000_SYS_MASK_MTXFRS_MASK		(0x1 << DW1000_SYS_MASK_MTXFRS_SHIFT)
#define DW1000_SYS_MASK_MRXPRD_MASK		(0x1 << DW1000_SYS_MASK_MRXPRD_SHIFT)
#define DW1000_SYS_MASK_MRXSFDD_MASK	(0x1 << DW1000_SYS_MASK_MRXSFDD_SHIFT)
#define DW1000_SYS_MASK_MLDEDONE_MASK	(0x1 << DW1000_SYS_MASK_MLDEDONE_SHIFT)
#define DW1000_SYS_MASK_MRXPHD_MASK		(0x1 << DW1000_SYS_MASK_MRXPHD_SHIFT)
#define DW1000_SYS_MASK_MRXPHE_MASK		(0x1 << DW1000_SYS_MASK_MRXPHE_SHIFT)
#define DW1000_SYS_MASK_MRXDFR_MASK		(0x1 << DW1000_SYS_MASK_MRXDFR_SHIFT)
#define DW1000_SYS_MASK_MRXFCG_MASK		(0x1 << DW1000_SYS_MASK_MRXFCG_SHIFT)
#define DW1000_SYS_MASK_MRXFCE_MASK		(0x1 << DW1000_SYS_MASK_MRXFCE_SHIFT)
#define DW1000_SYS_MASK_MRXRFSL_MASK	(0x1 << DW1000_SYS_MASK_MRXRFSL_SHIFT)
#define DW1000_SYS_MASK_MRXRFTO_MASK	(0x1 << DW1000_SYS_MASK_MRXRFTO_SHIFT)
#define DW1000_SYS_MASK_MLDEERR_MASK	(0x1 << DW1000_SYS_MASK_MLDEERR_SHIFT)
#define DW1000_SYS_MASK_MRXOVRR_MASK	(0x1 << DW1000_SYS_MASK_MRXOVRR_SHIFT)
#define DW1000_SYS_MASK_MRXPTO_MASK		(0x1 << DW1000_SYS_MASK_MRXPTO_SHIFT)
#define DW1000_SYS_MASK_MGPIOIRQ_MASK	(0x1 << DW1000_SYS_MASK_MGPIOIRQ_SHIFT)
#define DW1000_SYS_MASK_MSLP2INIT_MASK	(0x1 << DW1000_SYS_MASK_MSLP2INIT_SHIFT)
#define DW1000_SYS_MASK_MRFPLLLL_MASK	(0x1 << DW1000_SYS_MASK_MRFPLLLL_SHIFT)
#define DW1000_SYS_MASK_MCPLLLL_MASK	(0x1 << DW1000_SYS_MASK_MCPLLLL_SHIFT)
#define DW1000_SYS_MASK_MRXSFDTO_MASK	(0x1 << DW1000_SYS_MASK_MRXSFDTO_SHIFT)
#define DW1000_SYS_MASK_MHPDWARN_MASK	(0x1 << DW1000_SYS_MASK_MHPDWARN_SHIFT)
#define DW1000_SYS_MASK_MTXBERR_MASK	(0x1 << DW1000_SYS_MASK_MTXBERR_SHIFT)
#define DW1000_SYS_MASK_MAFFREJ_MASK	(0x1 << DW1000_SYS_MASK_MAFFREJ_SHIFT)

#define DW1000_SYS_MASK_MCPLOCK		(0x1 << DW1000_SYS_MASK_MCPLOCK_SHIFT)
#define DW1000_SYS_MASK_MESYNCR		(0x1 << DW1000_SYS_MASK_MESYNCR_SHIFT)
#define DW1000_SYS_MASK_MAAT		(0x1 << DW1000_SYS_MASK_MAAT_SHIFT	)
#define DW1000_SYS_MASK_MTXFRB		(0x1 << DW1000_SYS_MASK_MTXFRB_SHIFT)
#define DW1000_SYS_MASK_MTXPRS		(0x1 << DW1000_SYS_MASK_MTXPRS_SHIFT)
#define DW1000_SYS_MASK_MTXPHS		(0x1 << DW1000_SYS_MASK_MTXPHS_SHIFT)
#define DW1000_SYS_MASK_MTXFRS		(0x1 << DW1000_SYS_MASK_MTXFRS_SHIFT)
#define DW1000_SYS_MASK_MRXPRD		(0x1 << DW1000_SYS_MASK_MRXPRD_SHIFT)
#define DW1000_SYS_MASK_MRXSFDD		(0x1 << DW1000_SYS_MASK_MRXSFDD_SHIFT)
#define DW1000_SYS_MASK_MLDEDONE	(0x1 << DW1000_SYS_MASK_MLDEDONE_SHIFT)
#define DW1000_SYS_MASK_MRXPHD		(0x1 << DW1000_SYS_MASK_MRXPHD_SHIFT)
#define DW1000_SYS_MASK_MRXPHE		(0x1 << DW1000_SYS_MASK_MRXPHE_SHIFT)
#define DW1000_SYS_MASK_MRXDFR		(0x1 << DW1000_SYS_MASK_MRXDFR_SHIFT)
#define DW1000_SYS_MASK_MRXFCG		(0x1 << DW1000_SYS_MASK_MRXFCG_SHIFT)
#define DW1000_SYS_MASK_MRXFCE		(0x1 << DW1000_SYS_MASK_MRXFCE_SHIFT)
#define DW1000_SYS_MASK_MRXRFSL		(0x1 << DW1000_SYS_MASK_MRXRFSL_SHIFT)
#define DW1000_SYS_MASK_MRXRFTO		(0x1 << DW1000_SYS_MASK_MRXRFTO_SHIFT)
#define DW1000_SYS_MASK_MLDEERR		(0x1 << DW1000_SYS_MASK_MLDEERR_SHIFT)
#define DW1000_SYS_MASK_MRXOVRR		(0x1 << DW1000_SYS_MASK_MRXOVRR_SHIFT)
#define DW1000_SYS_MASK_MRXPTO		(0x1 << DW1000_SYS_MASK_MRXPTO_SHIFT)
#define DW1000_SYS_MASK_MGPIOIRQ	(0x1 << DW1000_SYS_MASK_MGPIOIRQ_SHIFT)
#define DW1000_SYS_MASK_MSLP2INIT	(0x1 << DW1000_SYS_MASK_MSLP2INIT_SHIFT)
#define DW1000_SYS_MASK_MRFPLLLL	(0x1 << DW1000_SYS_MASK_MRFPLLLL_SHIFT)
#define DW1000_SYS_MASK_MCPLLLL		(0x1 << DW1000_SYS_MASK_MCPLLLL_SHIFT)
#define DW1000_SYS_MASK_MRXSFDTO	(0x1 << DW1000_SYS_MASK_MRXSFDTO_SHIFT)
#define DW1000_SYS_MASK_MHPDWARN	(0x1 << DW1000_SYS_MASK_MHPDWARN_SHIFT)
#define DW1000_SYS_MASK_MTXBERR		(0x1 << DW1000_SYS_MASK_MTXBERR_SHIFT)
#define DW1000_SYS_MASK_MAFFREJ		(0x1 << DW1000_SYS_MASK_MAFFREJ_SHIFT)


/* 0x0F System Event Status Register
 * Octets		Type		Mnemonic	Description
 * 5			SRW			SYS_STATUS	System Event Status Register
 *
 * reg:0F:00	bit:0		IRQS		Interrupt Request Status
 * reg:0F:00	bit:1		CPLOCK		Clock PLL Lock
 * reg:0F:00	bit:2		ESYNCR		External Sync Clock Reset
 * reg:0F:00	bit:3		AAT			Automatic Acknowledge Trigger
 * reg:0F:00	bit:4		TXFRB		Transmit Frame Begins
 * reg:0F:00	bit:5		TXPRS		Transmit Preamble Sent
 * reg:0F:00	bit:6		TXPHS		Transmit PHY Header Sent
 * reg:0F:00	bit:7		TXFRS		Transmit Frame Sent
 * reg:0F:00	bit:8		RXPRD		Receiver Preamble Detected status
 * reg:0F:00	bit:9		RXSFDD		Receiver SFD Detected
 * reg:0F:00	bit:10		LDEDONE		LDE processing done
 * reg:0F:00	bit:11		RXPHD		Receiver PHY Header Detect
 * reg:0F:00	bit:12		RXPHE		Receiver PHY Header Error
 * reg:0F:00	bit:13		RXDFR		Receiver Data Frame Ready
 * reg:0F:00	bit:14		RXFCG		Receiver FCS Good
 * reg:0F:00	bit:15		RXFCE		Receiver FCS Error
 * reg:0F:00	bit:16		RXRFSL		Receiver Reed Solomon Frame Sync Loss
 * reg:0F:00	bit:17		RXRFTO		Receive Frame Wait Timeout
 * reg:0F:00	bit:18		LDEERR		Leading edge detection processing error
 * reg:0F:00	bit:20		RXOVRR		Receiver Overrun
 * reg:0F:00	bit:21		RXPTO		Preamble detection timeout
 * reg:0F:00	bit:22		GPIOIRQ		GPIO interrupt
 * reg:0F:00	bit:23		SLP2INIT	SLEEP to INIT
 * reg:0F:00	bit:24		RFPLL_LL	RF PLL Losing Lock
 * reg:0F:00	bit:25		CLKPLL_LL	Clock PLL Losing Lock
 * reg:0F:00	bit:26		RXSFDTO		Receive SFD timeout
 * reg:0F:00	bit:27		HPDWARN		Half Period Delay Warning
 * reg:0F:00	bit:28		TXBERR		Transmit Buffer Error
 * reg:0F:00	bit:29		AFFREJ		Automatic Frame Filtering rejection
 * reg:0F:00	bit:30		HSRBP		Host Side Receive Buffer Pointer
 * reg:0F:00	bit:31		ICRBP		IC side Receive Buffer Pointer
 * reg:0F:04	bit:0		RXRSCS		Receiver Reed-Solomon Correction Status
 * reg:0F:04	bit:1		RXPREJ		Receiver Preamble Rejection
 * reg:0F:04	bit:2		TXPUTE		Transmit power up time error */
/* reg:0F:00 */
#define DW1000_SYS_STATUS_IRQS_SHIFT		(0)
#define DW1000_SYS_STATUS_CPLOCK_SHIFT		(1)
#define DW1000_SYS_STATUS_ESYNCR_SHIFT		(2)
#define DW1000_SYS_STATUS_AAT_SHIFT			(3)
#define DW1000_SYS_STATUS_TXFRB_SHIFT		(4)
#define DW1000_SYS_STATUS_TXPRS_SHIFT		(5)
#define DW1000_SYS_STATUS_TXPHS_SHIFT		(6)
#define DW1000_SYS_STATUS_TXFRS_SHIFT		(7)
#define DW1000_SYS_STATUS_RXPRD_SHIFT		(8)
#define DW1000_SYS_STATUS_RXSFDD_SHIFT		(9)
#define DW1000_SYS_STATUS_LDEDONE_SHIFT		(10)
#define DW1000_SYS_STATUS_RXPHD_SHIFT		(11)
#define DW1000_SYS_STATUS_RXPHE_SHIFT		(12)
#define DW1000_SYS_STATUS_RXDFR_SHIFT		(13)
#define DW1000_SYS_STATUS_RXFCG_SHIFT		(14)
#define DW1000_SYS_STATUS_RXFCE_SHIFT		(15)
#define DW1000_SYS_STATUS_RXRFSL_SHIFT		(16)
#define DW1000_SYS_STATUS_RXRFTO_SHIFT		(17)
#define DW1000_SYS_STATUS_LDEERR_SHIFT		(18)
#define DW1000_SYS_STATUS_RXOVRR_SHIFT		(20)
#define DW1000_SYS_STATUS_RXPTO_SHIFT		(21)
#define DW1000_SYS_STATUS_GPIOIRQ_SHIFT		(22)
#define DW1000_SYS_STATUS_SLP2INIT_SHIFT	(23)
#define DW1000_SYS_STATUS_RFPLL_LL_SHIFT	(24)
#define DW1000_SYS_STATUS_CLKPLL_LL_SHIFT	(25)
#define DW1000_SYS_STATUS_RXSFDTO_SHIFT		(26)
#define DW1000_SYS_STATUS_HPDWARN_SHIFT		(27)
#define DW1000_SYS_STATUS_TXBERR_SHIFT		(28)
#define DW1000_SYS_STATUS_AFFREJ_SHIFT		(29)
#define DW1000_SYS_STATUS_HSRBP_SHIFT		(30)
#define DW1000_SYS_STATUS_ICRBP_SHIFT		(31)

/* reg:0F:04 */
#define DW1000_SYS_STATUS_RXRSCS_SHIFT		(0)
#define DW1000_SYS_STATUS_RXPREJ_SHIFT		(1)
#define DW1000_SYS_STATUS_TXPUTE_SHIFT		(2)

/* reg:0F:00 */
#define DW1000_SYS_STATUS_IRQS_MASK			(0x1 << DW1000_SYS_STATUS_IRQS_SHIFT)
#define DW1000_SYS_STATUS_CPLOCK_MASK		(0x1 << DW1000_SYS_STATUS_CPLOCK_SHIFT)
#define DW1000_SYS_STATUS_ESYNCR_MASK		(0x1 << DW1000_SYS_STATUS_ESYNCR_SHIFT)
#define DW1000_SYS_STATUS_AAT_MASK			(0x1 << DW1000_SYS_STATUS_AAT_SHIFT)
#define DW1000_SYS_STATUS_TXFRB_MASK		(0x1 << DW1000_SYS_STATUS_TXFRB_SHIFT)
#define DW1000_SYS_STATUS_TXPRS_MASK		(0x1 << DW1000_SYS_STATUS_TXPRS_SHIFT)
#define DW1000_SYS_STATUS_TXPHS_MASK		(0x1 << DW1000_SYS_STATUS_TXPHS_SHIFT)
#define DW1000_SYS_STATUS_TXFRS_MASK		(0x1 << DW1000_SYS_STATUS_TXFRS_SHIFT)
#define DW1000_SYS_STATUS_RXPRD_MASK		(0x1 << DW1000_SYS_STATUS_RXPRD_SHIFT)
#define DW1000_SYS_STATUS_RXSFDD_MASK		(0x1 << DW1000_SYS_STATUS_RXSFDD_SHIFT)
#define DW1000_SYS_STATUS_LDEDONE_MASK		(0x1 << DW1000_SYS_STATUS_LDEDONE_SHIFT)
#define DW1000_SYS_STATUS_RXPHD_MASK		(0x1 << DW1000_SYS_STATUS_RXPHD_SHIFT)
#define DW1000_SYS_STATUS_RXPHE_MASK		(0x1 << DW1000_SYS_STATUS_RXPHE_SHIFT)
#define DW1000_SYS_STATUS_RXDFR_MASK		(0x1 << DW1000_SYS_STATUS_RXDFR_SHIFT)
#define DW1000_SYS_STATUS_RXFCG_MASK		(0x1 << DW1000_SYS_STATUS_RXFCG_SHIFT)
#define DW1000_SYS_STATUS_RXFCE_MASK		(0x1 << DW1000_SYS_STATUS_RXFCE_SHIFT)
#define DW1000_SYS_STATUS_RXRFSL_MASK		(0x1 << DW1000_SYS_STATUS_RXRFSL_SHIFT)
#define DW1000_SYS_STATUS_RXRFTO_MASK		(0x1 << DW1000_SYS_STATUS_RXRFTO_SHIFT)
#define DW1000_SYS_STATUS_LDEERR_MASK		(0x1 << DW1000_SYS_STATUS_LDEERR_SHIFT)
#define DW1000_SYS_STATUS_RXOVRR_MASK		(0x1 << DW1000_SYS_STATUS_RXOVRR_SHIFT)
#define DW1000_SYS_STATUS_RXPTO_MASK		(0x1 << DW1000_SYS_STATUS_RXPTO_SHIFT)
#define DW1000_SYS_STATUS_GPIOIRQ_MASK		(0x1 << DW1000_SYS_STATUS_GPIOIRQ_SHIFT)
#define DW1000_SYS_STATUS_SLP2INIT_MASK		(0x1 << DW1000_SYS_STATUS_SLP2INIT_SHIFT)
#define DW1000_SYS_STATUS_RFPLL_LL_MASK		(0x1 << DW1000_SYS_STATUS_RFPLL_LL_SHIFT)
#define DW1000_SYS_STATUS_CLKPLL_LL_MASK	(0x1 << DW1000_SYS_STATUS_CLKPLL_LL_SHIFT)
#define DW1000_SYS_STATUS_RXSFDTO_MASK		(0x1 << DW1000_SYS_STATUS_RXSFDTO_SHIFT)
#define DW1000_SYS_STATUS_HPDWARN_MASK		(0x1 << DW1000_SYS_STATUS_HPDWARN_SHIFT)
#define DW1000_SYS_STATUS_TXBERR_MASK		(0x1 << DW1000_SYS_STATUS_TXBERR_SHIFT)
#define DW1000_SYS_STATUS_AFFREJ_MASK		(0x1 << DW1000_SYS_STATUS_AFFREJ_SHIFT)
#define DW1000_SYS_STATUS_HSRBP_MASK		(0x1 << DW1000_SYS_STATUS_HSRBP_SHIFT)
#define DW1000_SYS_STATUS_ICRBP_MASK		(0x1 << DW1000_SYS_STATUS_ICRBP_SHIFT)

/* reg:0F:04 */
#define DW1000_SYS_STATUS_RXRSCS_MASK		(0x1 << DW1000_SYS_STATUS_RXRSCS_SHIFT)
#define DW1000_SYS_STATUS_RXPREJ_MASK		(0x1 << DW1000_SYS_STATUS_RXPREJ_SHIFT)
#define DW1000_SYS_STATUS_TXPUTE_MASK		(0x1 << DW1000_SYS_STATUS_TXPUTE_SHIFT)


/* reg:0F:00 */
#define DW1000_SYS_STATUS_IRQS		(0x1 << DW1000_SYS_STATUS_IRQS_SHIFT)
#define DW1000_SYS_STATUS_CPLOCK	(0x1 << DW1000_SYS_STATUS_CPLOCK_SHIFT)
#define DW1000_SYS_STATUS_ESYNCR	(0x1 << DW1000_SYS_STATUS_ESYNCR_SHIFT)
#define DW1000_SYS_STATUS_AAT		(0x1 << DW1000_SYS_STATUS_AAT_SHIFT)
#define DW1000_SYS_STATUS_TXFRB		(0x1 << DW1000_SYS_STATUS_TXFRB_SHIFT)
#define DW1000_SYS_STATUS_TXPRS		(0x1 << DW1000_SYS_STATUS_TXPRS_SHIFT)
#define DW1000_SYS_STATUS_TXPHS		(0x1 << DW1000_SYS_STATUS_TXPHS_SHIFT)
#define DW1000_SYS_STATUS_TXFRS		(0x1 << DW1000_SYS_STATUS_TXFRS_SHIFT)
#define DW1000_SYS_STATUS_RXPRD		(0x1 << DW1000_SYS_STATUS_RXPRD_SHIFT)
#define DW1000_SYS_STATUS_RXSFDD	(0x1 << DW1000_SYS_STATUS_RXSFDD_SHIFT)
#define DW1000_SYS_STATUS_LDEDONE	(0x1 << DW1000_SYS_STATUS_LDEDONE_SHIFT)
#define DW1000_SYS_STATUS_RXPHD		(0x1 << DW1000_SYS_STATUS_RXPHD_SHIFT)
#define DW1000_SYS_STATUS_RXPHE		(0x1 << DW1000_SYS_STATUS_RXPHE_SHIFT)
#define DW1000_SYS_STATUS_RXDFR		(0x1 << DW1000_SYS_STATUS_RXDFR_SHIFT)
#define DW1000_SYS_STATUS_RXFCG		(0x1 << DW1000_SYS_STATUS_RXFCG_SHIFT)
#define DW1000_SYS_STATUS_RXFCE		(0x1 << DW1000_SYS_STATUS_RXFCE_SHIFT)
#define DW1000_SYS_STATUS_RXRFSL	(0x1 << DW1000_SYS_STATUS_RXRFSL_SHIFT)
#define DW1000_SYS_STATUS_RXRFTO	(0x1 << DW1000_SYS_STATUS_RXRFTO_SHIFT)
#define DW1000_SYS_STATUS_LDEERR	(0x1 << DW1000_SYS_STATUS_LDEERR_SHIFT)
#define DW1000_SYS_STATUS_RXOVRR	(0x1 << DW1000_SYS_STATUS_RXOVRR_SHIFT)
#define DW1000_SYS_STATUS_RXPTO		(0x1 << DW1000_SYS_STATUS_RXPTO_SHIFT)
#define DW1000_SYS_STATUS_GPIOIRQ	(0x1 << DW1000_SYS_STATUS_GPIOIRQ_SHIFT)
#define DW1000_SYS_STATUS_SLP2INIT	(0x1 << DW1000_SYS_STATUS_SLP2INIT_SHIFT)
#define DW1000_SYS_STATUS_RFPLL_LL	(0x1 << DW1000_SYS_STATUS_RFPLL_LL_SHIFT)
#define DW1000_SYS_STATUS_CLKPLL_LL	(0x1 << DW1000_SYS_STATUS_CLKPLL_LL_SHIFT)
#define DW1000_SYS_STATUS_RXSFDTO	(0x1 << DW1000_SYS_STATUS_RXSFDTO_SHIFT)
#define DW1000_SYS_STATUS_HPDWARN	(0x1 << DW1000_SYS_STATUS_HPDWARN_SHIFT)
#define DW1000_SYS_STATUS_TXBERR	(0x1 << DW1000_SYS_STATUS_TXBERR_SHIFT)
#define DW1000_SYS_STATUS_AFFREJ	(0x1 << DW1000_SYS_STATUS_AFFREJ_SHIFT)
#define DW1000_SYS_STATUS_HSRBP		(0x1 << DW1000_SYS_STATUS_HSRBP_SHIFT)
#define DW1000_SYS_STATUS_ICRBP		(0x1 << DW1000_SYS_STATUS_ICRBP_SHIFT)

// /* Bitmask of all rx timeout bits */
// #define DW1000_SYS_STATUS_RXTO (DW1000_SYS_STATUS_RXRFTO | DW1000_SYS_STATUS_RXPTO)

// /* Bitmask of all rx error bits */
// #define DW1000_SYS_STATUS_RXERR (
// 		DW1000_SYS_STATUS_RXPHE   | DW1000_SYS_STATUS_RXFCE  | DW1000_SYS_STATUS_RXRFSL |
// 		DW1000_SYS_STATUS_RXSFDTO | DW1000_SYS_STATUS_AFFREJ | DW1000_SYS_STATUS_LDEERR)

/* reg:0F:04 */
#define DW1000_SYS_STATUS_RXRSCS	(0x1 << DW1000_SYS_STATUS_RXRSCS_SHIFT)
#define DW1000_SYS_STATUS_RXPREJ	(0x1 << DW1000_SYS_STATUS_RXPREJ_SHIFT)
#define DW1000_SYS_STATUS_TXPUTE	(0x1 << DW1000_SYS_STATUS_TXPUTE_SHIFT)


/* 0x10 RX Frame Information Register
 * Octets		Type		Mnemonic	Description
 * 4			ROD			RX_FINFO	RX Frame Information (included in swinging set)
 *
 * reg:10:00	bits:6–0	RXFLEN		Receive Frame Length
 * reg:10:00	bits:9–7	RXFLE		Receive Frame Length Extension
 * reg:10:00	bits:12-11	RXNSPL		Receive non-standard preamble length
 * reg:10:00	bits:14-13	RXBR 		Receive Bit Rate report
 * reg:10:00	bit:15		RNG 		Receiver Ranging
 * reg:10:00	bits:17-16	RXPRFR		RX Pulse Repetition Rate report
 * reg:10:00	bits:19-18	RXPSR 		RX Preamble Repetition
 * reg:10:00	bits:31-20	RXPACC		Preamble Accumulation Count
 *
 * RXBR
 * 00	110 kbps
 * 01	850 kbps
 * 10	6.8 Mbps
 *
 * Bit 19 Bit 18 Bit 12 Bit 11
 * RXPSR RXNSPL RX Preamble Length
 * 0100	64
 * 0101	128
 * 0110	256
 * 0111	512
 * 1000	1024
 * 1001	1536
 * 1010	2048
 * 1100	4096 */
#define DW1000_RX_FINFO_RXFLEN_SHIFT	(0)
#define DW1000_RX_FINFO_RXFLE_SHIFT		(7)
#define DW1000_RX_FINFO_RXNSPL_SHIFT	(11)
#define DW1000_RX_FINFO_RXBR_SHIFT 		(13)
#define DW1000_RX_FINFO_RNG_SHIFT 		(15)
#define DW1000_RX_FINFO_RXPRFR_SHIFT	(16)
#define DW1000_RX_FINFO_RXPSR_SHIFT 	(18)
#define DW1000_RX_FINFO_RXPACC_SHIFT	(20)

#define DW1000_RX_FINFO_RXFLEN_MASK		(0x7F  << DW1000_RX_FINFO_RXFLEN_SHIFT)
#define DW1000_RX_FINFO_RXFLE_MASK		(0x7   << DW1000_RX_FINFO_RXFLE_SHIFT)
#define DW1000_RX_FINFO_RXNSPL_MASK		(0x3   << DW1000_RX_FINFO_RXNSPL_SHIFT)
#define DW1000_RX_FINFO_RXBR_MASK 		(0x3   << DW1000_RX_FINFO_RXBR_SHIFT)
#define DW1000_RX_FINFO_RNG_MASK 		(0x1   << DW1000_RX_FINFO_RNG_SHIFT)
#define DW1000_RX_FINFO_RXPRFR_MASK		(0x3   << DW1000_RX_FINFO_RXPRFR_SHIFT)
#define DW1000_RX_FINFO_RXPSR_MASK 		(0x3   << DW1000_RX_FINFO_RXPSR_SHIFT)
#define DW1000_RX_FINFO_RXPACC_MASK		(0xFFF << DW1000_RX_FINFO_RXPACC_SHIFT)

#define DW1000_RX_FINFO_RXNSPL_64	  = (0x1 << DW1000_RX_FINFO_RXPSR_SHIFT) | (0x0 << DW1000_RX_FINFO_RXNSPL_SHIFT),
#define DW1000_RX_FINFO_RXNSPL_128	  = (0x1 << DW1000_RX_FINFO_RXPSR_SHIFT) | (0x1 << DW1000_RX_FINFO_RXNSPL_SHIFT),
#define DW1000_RX_FINFO_RXNSPL_256	  = (0x1 << DW1000_RX_FINFO_RXPSR_SHIFT) | (0x2 << DW1000_RX_FINFO_RXNSPL_SHIFT),
#define DW1000_RX_FINFO_RXNSPL_512	  = (0x1 << DW1000_RX_FINFO_RXPSR_SHIFT) | (0x3 << DW1000_RX_FINFO_RXNSPL_SHIFT),
#define DW1000_RX_FINFO_RXNSPL_1024	  = (0x2 << DW1000_RX_FINFO_RXPSR_SHIFT) | (0x0 << DW1000_RX_FINFO_RXNSPL_SHIFT),
#define DW1000_RX_FINFO_RXNSPL_1536	  = (0x2 << DW1000_RX_FINFO_RXPSR_SHIFT) | (0x1 << DW1000_RX_FINFO_RXNSPL_SHIFT),
#define DW1000_RX_FINFO_RXNSPL_2048	  = (0x2 << DW1000_RX_FINFO_RXPSR_SHIFT) | (0x2 << DW1000_RX_FINFO_RXNSPL_SHIFT),
#define DW1000_RX_FINFO_RXNSPL_4096	  = (0x3 << DW1000_RX_FINFO_RXPSR_SHIFT)
#define DW1000_RX_FINFO_RXBR_110K	  = (0x0 << DW1000_RX_FINFO_RXBR_SHIFT)
#define DW1000_RX_FINFO_RXBR_850K	  = (0x1 << DW1000_RX_FINFO_RXBR_SHIFT)
#define DW1000_RX_FINFO_RXBR_6800K	  = (0x2 << DW1000_RX_FINFO_RXBR_SHIFT)
#define DW1000_RX_FINFO_RNG           = (0x1 << DW1000_RX_FINFO_RNG_SHIFT)
#define DW1000_RX_FINFO_RXPRFR_16_MHZ = (0x1 << DW1000_RX_FINFO_RXPRFR_SHIFT)
#define DW1000_RX_FINFO_RXPRFR_64_MHZ = (0x2 << DW1000_RX_FINFO_RXPRFR_SHIFT)
#define DW1000_RX_FINFO_RXPSR_16      = (0x0 << DW1000_RX_FINFO_RXPSR_SHIFT)
#define DW1000_RX_FINFO_RXPSR_64      = (0x1 << DW1000_RX_FINFO_RXPSR_SHIFT)
#define DW1000_RX_FINFO_RXPSR_1024    = (0x2 << DW1000_RX_FINFO_RXPSR_SHIFT)
#define DW1000_RX_FINFO_RXPSR_4096    = (0x3 << DW1000_RX_FINFO_RXPSR_SHIFT)
#define DW1000_RX_FINFO_RXPACC        = (0x1 << DW1000_RX_FINFO_RXPACC_SHIFT)


/* 0x11 Rx Frame Buffer
 * Octets		Type		Mnemonic	Description
 * 1024			ROD			RX_BUFFER	RX Frame Data Buffer (included in swinging set) */


/* 0x12 RX Frame Quality Information
 * Octets		Type		Mnemonic	Description
 * 8			ROD			RX_FQUAL	RX Frame Quality Information (included in swinging set)
 *
 * reg:12:00	bits:15–0	STD_NOISE	Standard Deviation of Noise
 * reg:12:00	bits:31–16	FP_AMPL2	First Path Amplitude point 2
 * reg:12:04 	bits:15–0	FP_AMPL3 	First Path Amplitude point 3
 * reg:12:04	bits:31–16	CIR_PWR		Channel Impulse Response Power */
/* reg:12:00 */
#define DW1000_RX_FQUAL_STD_NOISE_SHIFT	(0)
#define DW1000_RX_FQUAL_FP_AMPL2_SHIFT	(16)

/* reg:12:04 */
#define DW1000_RX_FQUAL_FP_AMPL3_SHIFT 	(0)
#define DW1000_RX_FQUAL_CIR_PWR_SHIFT	(16)

/* reg:12:00 */
#define DW1000_RX_FQUAL_STD_NOISE_MASK	(0xFFFF << DW1000_RX_FQUAL_STD_NOISE_SHIFT)
#define DW1000_RX_FQUAL_FP_AMPL2_MASK	(0xFFFF << DW1000_RX_FQUAL_FP_AMPL2_SHIFT)

/* reg:12:04 */
#define DW1000_RX_FQUAL_FP_AMPL3_MASK 	(0xFFFF << DW1000_RX_FQUAL_FP_AMPL3_SHIFT)
#define DW1000_RX_FQUAL_CIR_PWR_MASK	(0xFFFF << DW1000_RX_FQUAL_CIR_PWR_SHIFT)


/* 0x13 Receiver Time Tracking Interval
 * Octets		Type		Mnemonic	Description
 * 4			ROD			RX_TTCKI	Receiver Time Tracking Interval (included in swinging set)
 *
 * reg:13:00	bits:31-0	RXTTCKI		RX time tracking interval */
#define DW1000_RX_TTCKI_SHIFT (0)

#define DW1000_RX_TTCKI_MASK (0xFFFFFFFF)


/* 0x14 Receiver Time Tracking Offset
 * Octets		Type		Mnemonic	Description
 * 5			ROD			RX_TTCKO	Receiver Time Tracking Offset (included in swinging set)
 *
 * reg:14:00	bits:18–0	RXTOFS		RX Time Tracking Offset
 * reg:14:00	bits:31–24	RSMPDEL		Internal Re-sampler Delay Value
 * reg:14:04	bits:6–0	RCPHASE		Receive Carrier Phase Adjustment During Ranging */
/* reg:14:00 */
#define DW1000_RX_TTCKO_RXTOFS_SHIFT	(0)
#define DW1000_RX_TTCKO_RSMPDEL_SHIFT	(24)

/* reg:14:04 */
#define DW1000_RX_TTCKO_RCPHASE_SHIFT	(0)

/* reg:14:00 */
#define DW1000_RX_TTCKO_RXTOFS_MASK		(0x7FFFF << DW1000_RX_TTCKO_RXTOFS_SHIFT)
#define DW1000_RX_TTCKO_RSMPDEL_MASK	(0xFF    << DW1000_RX_TTCKO_RSMPDEL_SHIFT)

/* reg:14:04 */
#define DW1000_RX_TTCKO_RCPHASE_MASK	(0x7F    << DW1000_RX_TTCKO_RCPHASE_SHIFT)


/* 0x15 Receive Time Stamp
 * Octets		Type		Mnemonic	Description
 * 14			ROD			RX_TIME		Receive Time Stamp (included in swinging set)
 *
 * reg:15:00	bits:39–0	RX_STAMP	Adjusted time of reception (~15.65 picosecond resolution)
 * reg:15:05	bits:15–0	FP_INDEX	First path index.
 * reg:15:05	bits:31–16	FP_AMPL1	First Path Amplitude point 1.
 * reg:15:09	bits:39–0	RX_RAWST	Raw timestamp */
/* reg:15:00 */
#define DW1000_RX_TIME_RX_STAMP_LOW_SHIFT	(0)

/* reg:15:04 */
#define DW1000_RX_TIME_RX_STAMP_HIGH_SHIFT	(0)
#define DW1000_RX_TIME_FP_INDEX_SHIFT		(8)
#define DW1000_RX_TIME_FP_AMPL1_LOW_SHIFT	(24)

/* reg:15:08 */
#define DW1000_RX_TIME_FP_AMPL1_HIGH_SHIFT	(0)
#define DW1000_RX_TIME_RX_RAWST_LOW_SHIFT	(8)

/* reg:15:0C */
#define DW1000_RX_TIME_RX_RAWST_HIGH_SHIFT	(0)

/* reg:15:00 */
#define DW1000_RX_TIME_RX_STAMP_LOW_MASK	(0xFFFFFFFF << DW1000_RX_TIME_RX_STAMP_LOW_SHIFT)

/* reg:15:04 */
#define DW1000_RX_TIME_RX_STAMP_HIGH_MASK	(0xFF       << DW1000_RX_TIME_RX_STAMP_HIGH_SHIFT)
#define DW1000_RX_TIME_FP_INDEX_MASK		(0xFFFF     << DW1000_RX_TIME_FP_INDEX_SHIFT)
#define DW1000_RX_TIME_FP_AMPL1_LOW_MASK	(0xFF       << DW1000_RX_TIME_FP_AMPL1_LOW_SHIFT)

/* reg:15:08 */
#define DW1000_RX_TIME_FP_AMPL1_HIGH_MASK	(0xFF       << DW1000_RX_TIME_FP_AMPL1_HIGH_SHIFT)
#define DW1000_RX_TIME_RX_RAWST_LOW_MASK	(0xFFFFFF   << DW1000_RX_TIME_RX_RAWST_LOW_SHIFT)

/* reg:15:0C */
#define DW1000_RX_TIME_RX_RAWST_HIGH_MASK	(0xFFFF     << DW1000_RX_TIME_RX_RAWST_HIGH_SHIFT)


/* 0x17 Transmit Time Stamp
 * Octets		Type		Mnemonic	Description
 * 10			RO			TX_TIME		Transmit Time Stamp
 *
 * reg:17:00	bits:39–0	TX_STAMP	Adjusted time of transmission (~15.65 picosecond resolution)
 * reg:17:05	bits:39–0	TX_RAWST	Raw timestamp */
/* reg:17:00 */
/* reg:17:04 */
/* reg:17:08 */
#define DW1000_TX_TIME_TX_STAMP_LOW_SHIFT	(0)
#define DW1000_TX_TIME_TX_STAMP_HIGH_SHIFT	(0)
#define DW1000_TX_TIME_TX_RAWST_LOW_SHIFT	(8)
#define DW1000_TX_TIME_TX_RAWST_HIGH_SHIFT	(0)

#define DW1000_TX_TIME_TX_STAMP_LOW_MASK	(0xFFFFFFFF << DW1000_TX_TIME_TX_STAMP_LOW_SHIFT)
#define DW1000_TX_TIME_TX_STAMP_HIGH_MASK	(0xFF       << DW1000_TX_TIME_TX_STAMP_HIGH_SHIFT)
#define DW1000_TX_TIME_TX_RAWST_LOW_MASK	(0xFFFFFF   << DW1000_TX_TIME_TX_RAWST_LOW_SHIFT)
#define DW1000_TX_TIME_TX_RAWST_HIGH_MASK	(0xFFFF     << DW1000_TX_TIME_TX_RAWST_HIGH_SHIFT)


/* 0x18 Transmitter Antenna Delay
 * Octets		Type		Mnemonic	Description
 * 2			RW			TX_ANTD		16-bit Delay from Transmit to Antenna */


/* 0x1A Acknowledgement time and response time
 * Octets		Type		Mnemonic	Description
 * 4			RW			ACK_RESP_T	Acknowledgement Time and Response Time
 *
 * reg:1A:00	bits:19–0	W4R_TIM		Wait-for-Response turn-around Time
 * reg:1A:00	bits:31–24	ACK_TIM		Auto-Acknowledgement turn-around Time.
 *
 * Recommended minimum ACK_TIM settings
 * Data Rate	ACK_TIM
 * 110 kbps	0
 * 850 kbps	2
 * 6.8 Mbps	3 */
#define DW1000_ACK_RESP_T_W4R_TIM_SHIFT	(0)
#define DW1000_ACK_RESP_T_ACK_TIM_SHIFT	(24)

#define DW1000_ACK_RESP_T_W4R_TIM_MASK	(0xFFFFF << DW1000_ACK_RESP_T_W4R_TIM_SHIFT)
#define DW1000_ACK_RESP_T_ACK_TIM_MASK	(0xFF    << DW1000_ACK_RESP_T_ACK_TIM_SHIFT)


/* 0x1D Sniff Mode
 * Octets		Type		Mnemonic	Description
 * 4			RW			RX_SNIFF	Sniff Mode Configuration
 *
 * reg:1D:00	bits:3–0	SNIFF_ONT	SNIFF Mode ON time
 * reg:1D:00	bits:15–8	SNIFF_OFFT	SNIFF Mode OFF time specified in μs */
#define DW1000_RX_SNIFF_ONT_SHIFT	(0)
#define DW1000_RX_SNIFF_OFFT_SHIFT	(8)

#define DW1000_RX_SNIFF_ONT_MASK	(0xF  << DW1000_RX_SNIFF_ONT_SHIFT)
#define DW1000_RX_SNIFF_OFFT_MASK	(0xFF << DW1000_RX_SNIFF_OFFT_SHIFT)


/* 0x1E Transmit Power Control
 * Octets		Type		Mnemonic	Description
 * 4			RW			TX_POWER	TX Power Control */


/* 0x1F Channel Control
 * Octets		Type		Mnemonic	Description
 * 4			RW			CHAN_CTRL	Channel Control Register
 *
 * reg:1F:00	bits:3–0	TX_CHAN		Select Transmit Channel
 * reg:1F:00	bits:7–4	RX_CHAN		Select Receive Channel
 * reg:1F:00	bit:17		DWSFD		Enable Decawave propriertary (non-standard) SFD sequence
 * reg:1F:00	bits:19–18	RXPRF		Receiver PRF Select
 * reg:1F:00	bit:20		TNSSFD		Enable User Specified (non-standard) SFD in the transmitter
 * reg:1F:00	bit:21		RNSSFD		Enable User Specified (non-standard) SFD in receiver
 * reg:1F:00	bits:26–22	TX_PCODE	Select Transmitter Preamble Code
 * reg:1F:00	bits:31–27	RX_PCODE	Select Receiver Preamble Code */
#define DW1000_CHAN_CTRL_TX_CHAN_SHIFT	(0)
#define DW1000_CHAN_CTRL_RX_CHAN_SHIFT	(4)
#define DW1000_CHAN_CTRL_DWSFD_SHIFT	(17)
#define DW1000_CHAN_CTRL_RXPRF_SHIFT	(18)
#define DW1000_CHAN_CTRL_TNSSFD_SHIFT	(20)
#define DW1000_CHAN_CTRL_RNSSFD_SHIFT	(21)
#define DW1000_CHAN_CTRL_TX_PCODE_SHIFT	(22)
#define DW1000_CHAN_CTRL_RX_PCODE_SHIFT	(27)

#define DW1000_CHAN_CTRL_TX_CHAN_MASK	(0xF  << DW1000_CHAN_CTRL_TX_CHAN_SHIFT)
#define DW1000_CHAN_CTRL_RX_CHAN_MASK	(0xF  << DW1000_CHAN_CTRL_RX_CHAN_SHIFT)
#define DW1000_CHAN_CTRL_DWSFD_MASK		(0x1  << DW1000_CHAN_CTRL_DWSFD_SHIFT)
#define DW1000_CHAN_CTRL_RXPRF_MASK		(0x3  << DW1000_CHAN_CTRL_RXPRF_SHIFT)
#define DW1000_CHAN_CTRL_TNSSFD_MASK	(0x1  << DW1000_CHAN_CTRL_TNSSFD_SHIFT)
#define DW1000_CHAN_CTRL_RNSSFD_MASK	(0x1  << DW1000_CHAN_CTRL_RNSSFD_SHIFT)
#define DW1000_CHAN_CTRL_TX_PCODE_MASK	(0x1F << DW1000_CHAN_CTRL_TX_PCODE_SHIFT)
#define DW1000_CHAN_CTRL_RX_PCODE_MASK	(0x1F << DW1000_CHAN_CTRL_RX_PCODE_SHIFT)

#define DW1000_CHAN_CTRL_DWSFD        (0x1 << DW1000_CHAN_CTRL_DWSFD_SHIFT)
#define DW1000_CHAN_CTRL_RXPRF_16_MHZ (0x1 << DW1000_CHAN_CTRL_RXPRF_SHIFT)
#define DW1000_CHAN_CTRL_RXPRF_64_MHZ (0x2 << DW1000_CHAN_CTRL_RXPRF_SHIFT)
#define DW1000_CHAN_CTRL_TNSSFD       (0x1 << DW1000_CHAN_CTRL_TNSSFD_SHIFT)
#define DW1000_CHAN_CTRL_RNSSFD       (0x1 << DW1000_CHAN_CTRL_RNSSFD_SHIFT)
/* TODO: TX_PCODE, RX_PCODE. Section 10.5 */


/* 0x21 User Defined SFD Sequence
 * Octets		Type		Mnemonic	Description
 * 41			RW			USR_SFD		User-specified short/long TX/RX SFD sequences */


/* 0x23 AGC Configuration And Control
 * Octets		Type		Mnemonic	Description
 * 33			RW			AGC_CTRL	Automatic Gain Control configuration and control
 *
 * OFFSET		Mnemonic	Description
 * 0x00			-			reserved
 * 0x02			AGC_CTRL1	AGC Control #1
 * 0x04			AGC_TUNE1	AGC Tuning register 1
 * 0x06			-			reserved
 * 0x0C			AGC_TUNE2	AGC Tuning register 2
 * 0x10			-			reserved
 * 0x12			AGC_TUNE3	AGC Tuning register 3
 * 0x14			-			reserved
 * 0x1E			AGC_STAT1	AGC Status */
#define DW1000_AGC_CTRL1_OFFSET (0x02)
#define DW1000_AGC_TUNE1_OFFSET (0x04)
#define DW1000_AGC_TUNE2_OFFSET (0x0C)
#define DW1000_AGC_TUNE3_OFFSET (0x12)
#define DW1000_AGC_STAT1_OFFSET (0x1E)


/* 0x23:02 AGC_CTRL
 * Octets		Type		Mnemonic	Description
 * 2			RW			AGC_CTRL1	AGC Control #1
 *
 * reg:23:02	bit:0		DIS_AM		Disable AGC Measurement */
#define DW1000_AGC_CTRL_DIS_AM_SHIFT	(0)

#define DW1000_AGC_CTRL_DIS_AM_MASK		(0x1 << DW1000_AGC_CTRL_DIS_AM_SHIFT)

#define DW1000_AGC_CTRL_DIS_AM			(0x1 << DW1000_AGC_CTRL_DIS_AM_SHIFT)


/* 0x23:04 AGC_TUNE1
 * Octets		Type		Mnemonic	Description
 * 2			RW			AGC_TUNE1	AGC Tuning register 1 */
#define DW1000_AGC_TUNE1_16_MHZ (0x8870)
#define DW1000_AGC_TUNE1_64_MHZ (0x889B)


/* 0x23:0C AGC_TUNE2
 * Octets		Type		Mnemonic	Description
 * 4			RW			AGC_TUNE2	AGC Tuning register 2 */
#define DW1000_AGC_TUNE2 (0x2502A907)


/* 0x23:12 AGC_TUNE3
 * Octets		Type		Mnemonic	Description
 * 2			RW			AGC_TUNE3	AGC Tuning register 2 */
#define DW1000_AGC_TUNE3 (0x0035)

/* 0x23:1E AGC_STAT1
 * Octets		Type		Mnemonic	Description
 * 3			RW			AGC_STAT1	AGC Status
 *
 * reg:23:1E	bits:10–6	EDG1		Input noise power measurement
 * reg:23:1E	bits:19–11	EDV2		Input noise power measurement
 *
 * (EDV2 - 40) * 10^(EDG1) * SCH
 *
 * Channel	SCH Scaling Factor
 * 1 to 4	1.3335
 * 5 and 7	1.0000 */
#define DW1000_AGC_STAT1_EDG1_SHIFT		(0)
#define DW1000_AGC_STAT1_EDV2_SHIFT		(11)

#define DW1000_AGC_STAT1_EDG1_MASK		(0x1F  << DW1000_AGC_STAT1_EDG1_SHIFT)
#define DW1000_AGC_STAT1_EDV2_MASK		(0x1FF << DW1000_AGC_STAT1_EDV2_SHIFT)


/* 0x24 External Synchronisation Control
 * Octets		Type		Mnemonic	Description
 * 12			RW			EXT_SYNC	External synchronisation control
 *
 * OFFSET		Mnemonic	Description
 * 0x00			EC_CTRL		External clock synchronisation counter configuration
 * 0x04			EC_RXTC		External clock counter captured on RMARKER
 * 0x08			EC_GOLP		External clock offset to first path 1 GHz counter */
#define DW1000_EC_CTRL_OFFSET (0x00)
#define DW1000_EC_RXTC_OFFSET (0x04)
#define DW1000_EC_GOLP_OFFSET (0x08)


/* 0x24:00 EC_CTRL
 * Octets		Type		Mnemonic	Description
 * 12			RW			EXT_SYNC	External synchronisation control
 *
 * reg:24:00	bit:0		OSTSM		External transmit synchronisation mode enable
 * reg:24:00	bit:1		OSRSM		External receive synchronisation mode enable
 * reg:24:00	bit:2		PLLLDT		Clock PLL lock detect tune
 * reg:24:00	bits:10-3	WAIT		Wait counter (ext tx sync and external timebase reset)
 * reg:24:00	bit:11		OSTRM		External timebase reset mode enable */
#define DW1000_EXT_SYNC_OSTSM_SHIFT		(0)
#define DW1000_EXT_SYNC_OSRSM_SHIFT		(1)
#define DW1000_EXT_SYNC_PLLLDT_SHIFT	(2)
#define DW1000_EXT_SYNC_WAIT_SHIFT		(3)
#define DW1000_EXT_SYNC_OSTRM_SHIFT		(11)

#define DW1000_EXT_SYNC_OSTSM_MASK		(0x1  << DW1000_EXT_SYNC_OSTSM_SHIFT)
#define DW1000_EXT_SYNC_OSRSM_MASK		(0x1  << DW1000_EXT_SYNC_OSRSM_SHIFT)
#define DW1000_EXT_SYNC_PLLLDT_MASK		(0x1  << DW1000_EXT_SYNC_PLLLDT_SHIFT)
#define DW1000_EXT_SYNC_WAIT_MASK		(0xFF << DW1000_EXT_SYNC_WAIT_SHIFT)
#define DW1000_EXT_SYNC_OSTRM_MASK		(0x1  << DW1000_EXT_SYNC_OSTRM_SHIFT)

#define DW1000_EXT_SYNC_OSTSM	(0x1 << DW1000_EXT_SYNC_OSTSM_SHIFT)
#define DW1000_EXT_SYNC_OSRSM	(0x1 << DW1000_EXT_SYNC_OSRSM_SHIFT)
#define DW1000_EXT_SYNC_PLLLDT	(0x1 << DW1000_EXT_SYNC_PLLLDT_SHIFT)
#define DW1000_EXT_SYNC_OSTRM	(0x1 << DW1000_EXT_SYNC_OSTRM_SHIFT)


/* 0x24:04 EC_RXTC
 * Octets		Type		Mnemonic	Description
 * 4			RO			EC_RXTC		External clock synchronisation counter captured on RMARKER
 *
 * reg:24:04	bits31-0	RX_TS_EST	External clock synchronisation counter captured on RMARKER */
#define DW1000_EC_RXTC_RX_TS_EST_SHIFT	(0)

#define DW1000_EC_RXTC_RX_TS_EST_MASK	(0xFFFFFFFF << DW1000_EC_RXTC_RX_TS_EST_SHIFT)


/* 0x24:08 EC_GOLP
 * Octets		Type		Mnemonic	Description
 * 4			RO			EC_GOLP		External clock offset to first path 1 GHz counter
 *
 * reg:24:08	bits:5-0	OFFSET_EXT	1GHz count from arrival of RMARKER to the next edge of ext clock */
#define DW1000_EC_GOLP_OFFSET_EXT_SHIFT (0)

#define DW1000_EC_GOLP_OFFSET_EXT_MASK (0x3F << DW1000_EC_GOLP_OFFSET_EXT_SHIFT)


/* 0x25 Accumulator CIR Memory
 * Octets		Type		Mnemonic	Description
 * 4064			RO			ACC_MEM		Read access to accumulator data memory */


/* 0x26 GPIO Control and Status
 * Octets		Type		Mnemonic	Description
 * 44			RW			GPIO_CTRL	Peripheral Register (bus 1 access, GPIO control)
 *
 * OFFSET		Mnemonic	Description
 * 0x00			GPIO_MODE	GPIO Mode Control Register
 * 0x04			-			reserved
 * 0x08			GPIO_DIR	GPIO Direction Control Register
 * 0x0C			GPIO_DOUT	GPIO Data Output register
 * 0x10			GPIO_IRQE	GPIO Interrupt Enable
 * 0x14			GPIO_ISEN	GPIO Interrupt Sense Selection
 * 0x18			GPIO_IMODE	GPIO Interrupt Mode (Level / Edge)
 * 0x1C			GPIO_IBES	GPIO Interrupt “Both Edge” Select
 * 0x20			GPIO_ICLR	GPIO Interrupt Latch Clear
 * 0x24			GPIO_IDBE	GPIO Interrupt De-bounce Enable
 * 0x28			GPIO_RAW	GPIO raw state */
#define DW1000_GPIO_MODE_OFFSET		(0x00)
#define DW1000_GPIO_DIR_OFFSET		(0x08)
#define DW1000_GPIO_DOUT_OFFSET		(0x0C)
#define DW1000_GPIO_IRQE_OFFSET		(0x10)
#define DW1000_GPIO_ISEN_OFFSET		(0x14)
#define DW1000_GPIO_IMODE_OFFSET	(0x18)
#define DW1000_GPIO_IBES_OFFSET		(0x1C)
#define DW1000_GPIO_ICLR_OFFSET		(0x20)
#define DW1000_GPIO_IDBE_OFFSET		(0x24)
#define DW1000_GPIO_RAW_OFFSET		(0x28)


/* 0x26:00 GPIO_MODE
 * Octets		Type		Mnemonic	Description
 * 4			RW			GPIO_MODE	GPIO Mode Control Register
 *
 * reg:26:00	bits:7-6	MSGP0		Mode Selection for GPIO0/RXOKLED
 * reg:26:00	bits:9-8	MSGP1		Mode Selection for GPIO1/SFDLED
 * reg:26:00	bits:11-10	MSGP2		Mode Selection for GPIO2/RXLED
 * reg:26:00	bits:13-12	MSGP3		Mode Selection for GPIO3/TXLED
 * reg:26:00	bits:15-14	MSGP4		Mode Selection for GPIO4/EXTPA
 * reg:26:00	bits:17-16	MSGP5		Mode Selection for GPIO5/EXTTXE
 * reg:26:00	bits:19-18	MSGP6		Mode Selection for GPIO6/EXTRXE
 * reg:26:00	bits:21-20	MSGP7		Mode Selection for SYNC/GPIO7
 * reg:26:00	bits:23-22	MSGP8		Mode Selection for IRQ/GPIO8 */
#define DW1000_GPIO_MODE_MSGP0_SHIFT	(6)
#define DW1000_GPIO_MODE_MSGP1_SHIFT	(8)
#define DW1000_GPIO_MODE_MSGP2_SHIFT	(10)
#define DW1000_GPIO_MODE_MSGP3_SHIFT	(12)
#define DW1000_GPIO_MODE_MSGP4_SHIFT	(14)
#define DW1000_GPIO_MODE_MSGP5_SHIFT	(16)
#define DW1000_GPIO_MODE_MSGP6_SHIFT	(18)
#define DW1000_GPIO_MODE_MSGP7_SHIFT	(20)
#define DW1000_GPIO_MODE_MSGP8_SHIFT	(22)

#define DW1000_GPIO_MODE_MSGP0_MASK		(0x3 << DW1000_GPIO_MODE_MSGP0_SHIFT)
#define DW1000_GPIO_MODE_MSGP1_MASK		(0x3 << DW1000_GPIO_MODE_MSGP1_SHIFT)
#define DW1000_GPIO_MODE_MSGP2_MASK		(0x3 << DW1000_GPIO_MODE_MSGP2_SHIFT)
#define DW1000_GPIO_MODE_MSGP3_MASK		(0x3 << DW1000_GPIO_MODE_MSGP3_SHIFT)
#define DW1000_GPIO_MODE_MSGP4_MASK		(0x3 << DW1000_GPIO_MODE_MSGP4_SHIFT)
#define DW1000_GPIO_MODE_MSGP5_MASK		(0x3 << DW1000_GPIO_MODE_MSGP5_SHIFT)
#define DW1000_GPIO_MODE_MSGP6_MASK		(0x3 << DW1000_GPIO_MODE_MSGP6_SHIFT)
#define DW1000_GPIO_MODE_MSGP7_MASK		(0x3 << DW1000_GPIO_MODE_MSGP7_SHIFT)
#define DW1000_GPIO_MODE_MSGP8_MASK		(0x3 << DW1000_GPIO_MODE_MSGP8_SHIFT)

#define DW1000_GPIO_MODE_MSGP0_GPIO0	(0x0 << DW1000_GPIO_MODE_MSGP0_SHIFT)
#define DW1000_GPIO_MODE_MSGP0_RXOKLED	(0x1 << DW1000_GPIO_MODE_MSGP0_SHIFT)
#define DW1000_GPIO_MODE_MSGP0_SYSCLK	(0x2 << DW1000_GPIO_MODE_MSGP0_SHIFT)
#define DW1000_GPIO_MODE_MSGP1_GPIO1	(0x0 << DW1000_GPIO_MODE_MSGP1_SHIFT)
#define DW1000_GPIO_MODE_MSGP1_SFDLED	(0x1 << DW1000_GPIO_MODE_MSGP1_SHIFT)
#define DW1000_GPIO_MODE_MSGP2_GPIO2	(0x0 << DW1000_GPIO_MODE_MSGP2_SHIFT)
#define DW1000_GPIO_MODE_MSGP2_RXLED	(0x1 << DW1000_GPIO_MODE_MSGP2_SHIFT)
#define DW1000_GPIO_MODE_MSGP3_GPIO3	(0x0 << DW1000_GPIO_MODE_MSGP3_SHIFT)
#define DW1000_GPIO_MODE_MSGP3_TXLED	(0x1 << DW1000_GPIO_MODE_MSGP3_SHIFT)
#define DW1000_GPIO_MODE_MSGP4_GPIO4	(0x0 << DW1000_GPIO_MODE_MSGP4_SHIFT)
#define DW1000_GPIO_MODE_MSGP4_EXTPA	(0x1 << DW1000_GPIO_MODE_MSGP4_SHIFT)
#define DW1000_GPIO_MODE_MSGP5_GPIO5	(0x0 << DW1000_GPIO_MODE_MSGP5_SHIFT)
#define DW1000_GPIO_MODE_MSGP5_EXTTXE	(0x1 << DW1000_GPIO_MODE_MSGP5_SHIFT)
#define DW1000_GPIO_MODE_MSGP6_GPIO6	(0x0 << DW1000_GPIO_MODE_MSGP6_SHIFT)
#define DW1000_GPIO_MODE_MSGP6_EXTRXE	(0x1 << DW1000_GPIO_MODE_MSGP6_SHIFT)
#define DW1000_GPIO_MODE_MSGP7_SYNC		(0x0 << DW1000_GPIO_MODE_MSGP7_SHIFT)
#define DW1000_GPIO_MODE_MSGP7_GPIO7	(0x1 << DW1000_GPIO_MODE_MSGP7_SHIFT)
#define DW1000_GPIO_MODE_MSGP8_IRQ		(0x0 << DW1000_GPIO_MODE_MSGP8_SHIFT)
#define DW1000_GPIO_MODE_MSGP8_GPIO8	(0x1 << DW1000_GPIO_MODE_MSGP8_SHIFT)


/* 0x26:08 GPIO_DIR
 * Octets		Type		Mnemonic	Description
 * 4			RW			GPIO_DIR	GPIO Direction Control Register
 *
 * reg:26:08	bit:0		GDP0		Direction Selection for GPIO0
 * reg:26:08	bit:1		GDP1		Direction Selection for GPIO1
 * reg:26:08	bit:2		GDP2		Direction Selection for GPIO2
 * reg:26:08	bit:3		GDP3		Direction Selection for GPIO3
 * reg:26:08	bit:4		GDM0		Mask for setting the direction of GPIO0
 * reg:26:08	bit:5		GDM1		Mask for setting the direction of GPIO1
 * reg:26:08	bit:6		GDM2		Mask for setting the direction of GPIO2
 * reg:26:08	bit:7		GDM3		Mask for setting the direction of GPIO3
 * reg:26:08	bit:8		GDP4		Direction Selection for GPIO4
 * reg:26:08	bit:9		GDP5		Direction Selection for GPIO5
 * reg:26:08	bit:10		GDP6		Direction Selection for the GPIO6
 * reg:26:08	bit:11		GDP7		Direction Selection for the GPIO7
 * reg:26:08	bit:12		GDM4		Mask for setting the direction of GPIO4
 * reg:26:08	bit:13		GDM5		Mask for setting the direction of GPIO5
 * reg:26:08	bit:14		GDM6		Mask for setting the direction of GPIO6
 * reg:26:08	bit:15		GDM7		Mask for setting the direction of GPIO7
 * reg:26:08	bit:16		GDP8		Direction Selection for GPIO8
 * reg:26:08	bit:20		GDM8		Mask for setting the direction of GPIO8 */
#define DW1000_GPIO_DIR_GDP0_SHIFT	(0)
#define DW1000_GPIO_DIR_GDP1_SHIFT	(1)
#define DW1000_GPIO_DIR_GDP2_SHIFT	(2)
#define DW1000_GPIO_DIR_GDP3_SHIFT	(3)
#define DW1000_GPIO_DIR_GDM0_SHIFT	(4)
#define DW1000_GPIO_DIR_GDM1_SHIFT	(5)
#define DW1000_GPIO_DIR_GDM2_SHIFT	(6)
#define DW1000_GPIO_DIR_GDM3_SHIFT	(7)
#define DW1000_GPIO_DIR_GDP4_SHIFT	(8)
#define DW1000_GPIO_DIR_GDP5_SHIFT	(9)
#define DW1000_GPIO_DIR_GDP6_SHIFT	(10)
#define DW1000_GPIO_DIR_GDP7_SHIFT	(11)
#define DW1000_GPIO_DIR_GDM4_SHIFT	(12)
#define DW1000_GPIO_DIR_GDM5_SHIFT	(13)
#define DW1000_GPIO_DIR_GDM6_SHIFT	(14)
#define DW1000_GPIO_DIR_GDM7_SHIFT	(15)
#define DW1000_GPIO_DIR_GDP8_SHIFT	(16)
#define DW1000_GPIO_DIR_GDM8_SHIFT	(20)

#define DW1000_GPIO_DIR_GDP0_MASK	(0x1 << DW1000_GPIO_DIR_GDP0_SHIFT)
#define DW1000_GPIO_DIR_GDP1_MASK	(0x1 << DW1000_GPIO_DIR_GDP1_SHIFT)
#define DW1000_GPIO_DIR_GDP2_MASK	(0x1 << DW1000_GPIO_DIR_GDP2_SHIFT)
#define DW1000_GPIO_DIR_GDP3_MASK	(0x1 << DW1000_GPIO_DIR_GDP3_SHIFT)
#define DW1000_GPIO_DIR_GDM0_MASK	(0x1 << DW1000_GPIO_DIR_GDM0_SHIFT)
#define DW1000_GPIO_DIR_GDM1_MASK	(0x1 << DW1000_GPIO_DIR_GDM1_SHIFT)
#define DW1000_GPIO_DIR_GDM2_MASK	(0x1 << DW1000_GPIO_DIR_GDM2_SHIFT)
#define DW1000_GPIO_DIR_GDM3_MASK	(0x1 << DW1000_GPIO_DIR_GDM3_SHIFT)
#define DW1000_GPIO_DIR_GDP4_MASK	(0x1 << DW1000_GPIO_DIR_GDP4_SHIFT)
#define DW1000_GPIO_DIR_GDP5_MASK	(0x1 << DW1000_GPIO_DIR_GDP5_SHIFT)
#define DW1000_GPIO_DIR_GDP6_MASK	(0x1 << DW1000_GPIO_DIR_GDP6_SHIFT)
#define DW1000_GPIO_DIR_GDP7_MASK	(0x1 << DW1000_GPIO_DIR_GDP7_SHIFT)
#define DW1000_GPIO_DIR_GDM4_MASK	(0x1 << DW1000_GPIO_DIR_GDM4_SHIFT)
#define DW1000_GPIO_DIR_GDM5_MASK	(0x1 << DW1000_GPIO_DIR_GDM5_SHIFT)
#define DW1000_GPIO_DIR_GDM6_MASK	(0x1 << DW1000_GPIO_DIR_GDM6_SHIFT)
#define DW1000_GPIO_DIR_GDM7_MASK	(0x1 << DW1000_GPIO_DIR_GDM7_SHIFT)
#define DW1000_GPIO_DIR_GDP8_MASK	(0x1 << DW1000_GPIO_DIR_GDP8_SHIFT)
#define DW1000_GPIO_DIR_GDM8_MASK	(0x1 << DW1000_GPIO_DIR_GDM8_SHIFT)

#define DW1000_GPIO_DIR_GDP0_INPUT	(0x0 << DW1000_GPIO_DIR_GDP0_SHIFT)
#define DW1000_GPIO_DIR_GDP0_OUTPUT	(0x1 << DW1000_GPIO_DIR_GDP0_SHIFT)
#define DW1000_GPIO_DIR_GDP1_INPUT	(0x0 << DW1000_GPIO_DIR_GDP1_SHIFT)
#define DW1000_GPIO_DIR_GDP1_OUTPUT	(0x1 << DW1000_GPIO_DIR_GDP1_SHIFT)
#define DW1000_GPIO_DIR_GDP2_INPUT	(0x0 << DW1000_GPIO_DIR_GDP2_SHIFT)
#define DW1000_GPIO_DIR_GDP2_OUTPUT	(0x1 << DW1000_GPIO_DIR_GDP2_SHIFT)
#define DW1000_GPIO_DIR_GDP3_INPUT	(0x0 << DW1000_GPIO_DIR_GDP3_SHIFT)
#define DW1000_GPIO_DIR_GDP3_OUTPUT	(0x1 << DW1000_GPIO_DIR_GDP3_SHIFT)
#define DW1000_GPIO_DIR_GDP4_INPUT	(0x0 << DW1000_GPIO_DIR_GDP4_SHIFT)
#define DW1000_GPIO_DIR_GDP4_OUTPUT	(0x1 << DW1000_GPIO_DIR_GDP4_SHIFT)
#define DW1000_GPIO_DIR_GDP5_INPUT	(0x0 << DW1000_GPIO_DIR_GDP5_SHIFT)
#define DW1000_GPIO_DIR_GDP5_OUTPUT	(0x1 << DW1000_GPIO_DIR_GDP5_SHIFT)
#define DW1000_GPIO_DIR_GDP6_INPUT	(0x0 << DW1000_GPIO_DIR_GDP6_SHIFT)
#define DW1000_GPIO_DIR_GDP6_OUTPUT	(0x1 << DW1000_GPIO_DIR_GDP6_SHIFT)
#define DW1000_GPIO_DIR_GDP7_INPUT	(0x0 << DW1000_GPIO_DIR_GDP7_SHIFT)
#define DW1000_GPIO_DIR_GDP7_OUTPUT	(0x1 << DW1000_GPIO_DIR_GDP7_SHIFT)
#define DW1000_GPIO_DIR_GDP8_INPUT	(0x0 << DW1000_GPIO_DIR_GDP8_SHIFT)
#define DW1000_GPIO_DIR_GDP8_OUTPUT	(0x1 << DW1000_GPIO_DIR_GDP8_SHIFT)
#define DW1000_GPIO_DIR_GDM8_INPUT	(0x0 << DW1000_GPIO_DIR_GDM8_SHIFT)
#define DW1000_GPIO_DIR_GDM8_OUTPUT	(0x1 << DW1000_GPIO_DIR_GDM8_SHIFT)

#define DW1000_GPIO_DIR_GDM0 (0x1 << DW1000_GPIO_DIR_GDM0_SHIFT)
#define DW1000_GPIO_DIR_GDM1 (0x1 << DW1000_GPIO_DIR_GDM1_SHIFT)
#define DW1000_GPIO_DIR_GDM2 (0x1 << DW1000_GPIO_DIR_GDM2_SHIFT)
#define DW1000_GPIO_DIR_GDM3 (0x1 << DW1000_GPIO_DIR_GDM3_SHIFT)
#define DW1000_GPIO_DIR_GDM4 (0x1 << DW1000_GPIO_DIR_GDM4_SHIFT)
#define DW1000_GPIO_DIR_GDM5 (0x1 << DW1000_GPIO_DIR_GDM5_SHIFT)
#define DW1000_GPIO_DIR_GDM6 (0x1 << DW1000_GPIO_DIR_GDM6_SHIFT)
#define DW1000_GPIO_DIR_GDM7 (0x1 << DW1000_GPIO_DIR_GDM7_SHIFT)
#define DW1000_GPIO_DIR_GDM8 (0x1 << DW1000_GPIO_DIR_GDM8_SHIFT)


/* 0x26:0C GPIO_DOUT
 * Octets		Type		Mnemonic	Description
 * 4			RW			GPIO_DOUT	GPIO Data Output Register
 *
 * reg:26:0C	bit:0		GOP0		Output state setting for the GPIO0 output
 * reg:26:0C	bit:1		GOP1		Output state setting for GPIO1
 * reg:26:0C	bit:2		GOP2		Output state setting for GPIO2
 * reg:26:0C	bit:3		GOP3		Output state setting for GPIO3
 * reg:26:0C	bit:4		GOM0		Mask for setting GPIO0 output state
 * reg:26:0C	bit:5		GOM1		Mask for setting GPIO1 output state
 * reg:26:0C	bit:6		GOM2		Mask for setting GPIO2 output state
 * reg:26:0C	bit:7		GOM3		Mask for setting GPIO3 output state
 * reg:26:0C	bit:8		GOP4		Output state setting for GPIO4
 * reg:26:0C	bit:9		GOP5		Output state setting for GPIO5
 * reg:26:0C	bit:10		GOP6		Output state setting for GPIO6
 * reg:26:0C	bit:11		GOP7		Output state setting for GPIO7
 * reg:26:0C	bit:12		GOM4		Mask for setting the GPIO4 output state
 * reg:26:0C	bit:13		GOM5		Mask for setting the GPIO5 output state
 * reg:26:0C	bit:15		GOM6		Mask for setting the GPIO6 output state
 * reg:26:0C	bit:15		GOM7		Mask for setting the GPIO7 output state
 * reg:26:0C	bit:16		GOP8		Output state setting for GPIO8
 * reg:26:0C	bit:20		GOM8		Mask for setting the GPIO8 output state */
#define DW1000_GPIO_DOUT_GOP0_SHIFT	(0)
#define DW1000_GPIO_DOUT_GOP1_SHIFT	(1)
#define DW1000_GPIO_DOUT_GOP2_SHIFT	(2)
#define DW1000_GPIO_DOUT_GOP3_SHIFT	(3)
#define DW1000_GPIO_DOUT_GOM0_SHIFT	(4)
#define DW1000_GPIO_DOUT_GOM1_SHIFT	(5)
#define DW1000_GPIO_DOUT_GOM2_SHIFT	(6)
#define DW1000_GPIO_DOUT_GOM3_SHIFT	(7)
#define DW1000_GPIO_DOUT_GOP4_SHIFT	(8)
#define DW1000_GPIO_DOUT_GOP5_SHIFT	(9)
#define DW1000_GPIO_DOUT_GOP6_SHIFT	(10)
#define DW1000_GPIO_DOUT_GOP7_SHIFT	(11)
#define DW1000_GPIO_DOUT_GOM4_SHIFT	(12)
#define DW1000_GPIO_DOUT_GOM5_SHIFT	(13)
#define DW1000_GPIO_DOUT_GOM6_SHIFT	(15)
#define DW1000_GPIO_DOUT_GOM7_SHIFT	(15)
#define DW1000_GPIO_DOUT_GOP8_SHIFT	(16)
#define DW1000_GPIO_DOUT_GOM8_SHIFT	(20)

#define DW1000_GPIO_DOUT_GOP0_MASK	(0x1 << DW1000_GPIO_DOUT_GOP0_SHIFT)
#define DW1000_GPIO_DOUT_GOP1_MASK	(0x1 << DW1000_GPIO_DOUT_GOP1_SHIFT)
#define DW1000_GPIO_DOUT_GOP2_MASK	(0x1 << DW1000_GPIO_DOUT_GOP2_SHIFT)
#define DW1000_GPIO_DOUT_GOP3_MASK	(0x1 << DW1000_GPIO_DOUT_GOP3_SHIFT)
#define DW1000_GPIO_DOUT_GOM0_MASK	(0x1 << DW1000_GPIO_DOUT_GOM0_SHIFT)
#define DW1000_GPIO_DOUT_GOM1_MASK	(0x1 << DW1000_GPIO_DOUT_GOM1_SHIFT)
#define DW1000_GPIO_DOUT_GOM2_MASK	(0x1 << DW1000_GPIO_DOUT_GOM2_SHIFT)
#define DW1000_GPIO_DOUT_GOM3_MASK	(0x1 << DW1000_GPIO_DOUT_GOM3_SHIFT)
#define DW1000_GPIO_DOUT_GOP4_MASK	(0x1 << DW1000_GPIO_DOUT_GOP4_SHIFT)
#define DW1000_GPIO_DOUT_GOP5_MASK	(0x1 << DW1000_GPIO_DOUT_GOP5_SHIFT)
#define DW1000_GPIO_DOUT_GOP6_MASK	(0x1 << DW1000_GPIO_DOUT_GOP6_SHIFT)
#define DW1000_GPIO_DOUT_GOP7_MASK	(0x1 << DW1000_GPIO_DOUT_GOP7_SHIFT)
#define DW1000_GPIO_DOUT_GOM4_MASK	(0x1 << DW1000_GPIO_DOUT_GOM4_SHIFT)
#define DW1000_GPIO_DOUT_GOM5_MASK	(0x1 << DW1000_GPIO_DOUT_GOM5_SHIFT)
#define DW1000_GPIO_DOUT_GOM6_MASK	(0x1 << DW1000_GPIO_DOUT_GOM6_SHIFT)
#define DW1000_GPIO_DOUT_GOM7_MASK	(0x1 << DW1000_GPIO_DOUT_GOM7_SHIFT)
#define DW1000_GPIO_DOUT_GOP8_MASK	(0x1 << DW1000_GPIO_DOUT_GOP8_SHIFT)
#define DW1000_GPIO_DOUT_GOM8_MASK	(0x1 << DW1000_GPIO_DOUT_GOM8_SHIFT)

#define DW1000_GPIO_DOUT_GOP0 (0x1 << DW1000_GPIO_DOUT_GOP0_SHIFT)
#define DW1000_GPIO_DOUT_GOP1 (0x1 << DW1000_GPIO_DOUT_GOP1_SHIFT)
#define DW1000_GPIO_DOUT_GOP2 (0x1 << DW1000_GPIO_DOUT_GOP2_SHIFT)
#define DW1000_GPIO_DOUT_GOP3 (0x1 << DW1000_GPIO_DOUT_GOP3_SHIFT)
#define DW1000_GPIO_DOUT_GOM0 (0x1 << DW1000_GPIO_DOUT_GOM0_SHIFT)
#define DW1000_GPIO_DOUT_GOM1 (0x1 << DW1000_GPIO_DOUT_GOM1_SHIFT)
#define DW1000_GPIO_DOUT_GOM2 (0x1 << DW1000_GPIO_DOUT_GOM2_SHIFT)
#define DW1000_GPIO_DOUT_GOM3 (0x1 << DW1000_GPIO_DOUT_GOM3_SHIFT)
#define DW1000_GPIO_DOUT_GOP4 (0x1 << DW1000_GPIO_DOUT_GOP4_SHIFT)
#define DW1000_GPIO_DOUT_GOP5 (0x1 << DW1000_GPIO_DOUT_GOP5_SHIFT)
#define DW1000_GPIO_DOUT_GOP6 (0x1 << DW1000_GPIO_DOUT_GOP6_SHIFT)
#define DW1000_GPIO_DOUT_GOP7 (0x1 << DW1000_GPIO_DOUT_GOP7_SHIFT)
#define DW1000_GPIO_DOUT_GOM4 (0x1 << DW1000_GPIO_DOUT_GOM4_SHIFT)
#define DW1000_GPIO_DOUT_GOM5 (0x1 << DW1000_GPIO_DOUT_GOM5_SHIFT)
#define DW1000_GPIO_DOUT_GOM6 (0x1 << DW1000_GPIO_DOUT_GOM6_SHIFT)
#define DW1000_GPIO_DOUT_GOM7 (0x1 << DW1000_GPIO_DOUT_GOM7_SHIFT)
#define DW1000_GPIO_DOUT_GOP8 (0x1 << DW1000_GPIO_DOUT_GOP8_SHIFT)
#define DW1000_GPIO_DOUT_GOM8 (0x1 << DW1000_GPIO_DOUT_GOM8_SHIFT)


/* 0x26:10 GPIO_IRQE
 * Octets		Type		Mnemonic	Description
 * 4			RW			GPIO_IRQE	GPIO Interrupt Enable
 *
 * reg:26:10	bit:0		GIRQE0		GPIO IRQ Enable for GPIO0 input
 * reg:26:10	bit:1		GIRQE1		GPIO IRQ Enable for GPIO1 input
 * reg:26:10	bit:2		GIRQE2		GPIO IRQ Enable for GPIO2 input
 * reg:26:10	bit:3		GIRQE3		GPIO IRQ Enable for GPIO3 input
 * reg:26:10	bit:4		GIRQE4		GPIO IRQ Enable for GPIO4 input
 * reg:26:10	bit:5		GIRQE5		GPIO IRQ Enable for GPIO5 input
 * reg:26:10	bit:6		GIRQE6		GPIO IRQ Enable for GPIO6 input
 * reg:26:10	bit:7		GIRQE7		GPIO IRQ Enable for GPIO7 input
 * reg:26:10	bit:8		GIRQE8		GPIO IRQ Enable for GPIO8 input */
#define DW1000_GPIO_GIRQE0_SHIFT	(0)
#define DW1000_GPIO_GIRQE1_SHIFT	(1)
#define DW1000_GPIO_GIRQE2_SHIFT	(2)
#define DW1000_GPIO_GIRQE3_SHIFT	(3)
#define DW1000_GPIO_GIRQE4_SHIFT	(4)
#define DW1000_GPIO_GIRQE5_SHIFT	(5)
#define DW1000_GPIO_GIRQE6_SHIFT	(6)
#define DW1000_GPIO_GIRQE7_SHIFT	(7)
#define DW1000_GPIO_GIRQE8_SHIFT	(8)

#define DW1000_GPIO_GIRQE0_MASK		(0x1 << DW1000_GPIO_GIRQE0_SHIFT)
#define DW1000_GPIO_GIRQE1_MASK		(0x1 << DW1000_GPIO_GIRQE1_SHIFT)
#define DW1000_GPIO_GIRQE2_MASK		(0x1 << DW1000_GPIO_GIRQE2_SHIFT)
#define DW1000_GPIO_GIRQE3_MASK		(0x1 << DW1000_GPIO_GIRQE3_SHIFT)
#define DW1000_GPIO_GIRQE4_MASK		(0x1 << DW1000_GPIO_GIRQE4_SHIFT)
#define DW1000_GPIO_GIRQE5_MASK		(0x1 << DW1000_GPIO_GIRQE5_SHIFT)
#define DW1000_GPIO_GIRQE6_MASK		(0x1 << DW1000_GPIO_GIRQE6_SHIFT)
#define DW1000_GPIO_GIRQE7_MASK		(0x1 << DW1000_GPIO_GIRQE7_SHIFT)
#define DW1000_GPIO_GIRQE8_MASK		(0x1 << DW1000_GPIO_GIRQE8_SHIFT)

#define DW1000_GPIO_GIRQE0 (0x1 << DW1000_GPIO_GIRQE0_SHIFT)
#define DW1000_GPIO_GIRQE1 (0x1 << DW1000_GPIO_GIRQE1_SHIFT)
#define DW1000_GPIO_GIRQE2 (0x1 << DW1000_GPIO_GIRQE2_SHIFT)
#define DW1000_GPIO_GIRQE3 (0x1 << DW1000_GPIO_GIRQE3_SHIFT)
#define DW1000_GPIO_GIRQE4 (0x1 << DW1000_GPIO_GIRQE4_SHIFT)
#define DW1000_GPIO_GIRQE5 (0x1 << DW1000_GPIO_GIRQE5_SHIFT)
#define DW1000_GPIO_GIRQE6 (0x1 << DW1000_GPIO_GIRQE6_SHIFT)
#define DW1000_GPIO_GIRQE7 (0x1 << DW1000_GPIO_GIRQE7_SHIFT)
#define DW1000_GPIO_GIRQE8 (0x1 << DW1000_GPIO_GIRQE8_SHIFT)


/* 0x26:14 GPIO_ISEN
 * Octets		Type		Mnemonic	Description
 * 4			RW			GPIO_ISEN	GPIO Interrupt Sense Selection
 *
 * reg:26:14	bit:0		GISEN0		GPIO IRQ sense selection GPIO0 input
 * reg:26:14	bit:1		GISEN1		GPIO IRQ sense selection GPIO1 input
 * reg:26:14	bit:2		GISEN2		GPIO IRQ sense selection GPIO2 input
 * reg:26:14	bit:3		GISEN3		GPIO IRQ sense selection GPIO3 input
 * reg:26:14	bit:4		GISEN4		GPIO IRQ sense selection GPIO4 input
 * reg:26:14	bit:5		GISEN5		GPIO IRQ sense selection GPIO5 input
 * reg:26:14	bit:6		GISEN6		GPIO IRQ sense selection GPIO6 input
 * reg:26:14	bit:7		GISEN7		GPIO IRQ sense selection GPIO7 input
 * reg:26:14	bit:8		GISEN8		GPIO IRQ sense selection GPIO8 input */
#define DW1000_GPIO_GISEN0_SHIFT	(0)
#define DW1000_GPIO_GISEN1_SHIFT	(1)
#define DW1000_GPIO_GISEN2_SHIFT	(2)
#define DW1000_GPIO_GISEN3_SHIFT	(3)
#define DW1000_GPIO_GISEN4_SHIFT	(4)
#define DW1000_GPIO_GISEN5_SHIFT	(5)
#define DW1000_GPIO_GISEN6_SHIFT	(6)
#define DW1000_GPIO_GISEN7_SHIFT	(7)
#define DW1000_GPIO_GISEN8_SHIFT	(8)

#define DW1000_GPIO_GISEN0_MASK		(0x1 << DW1000_GPIO_GISEN0_SHIFT)
#define DW1000_GPIO_GISEN1_MASK		(0x1 << DW1000_GPIO_GISEN1_SHIFT)
#define DW1000_GPIO_GISEN2_MASK		(0x1 << DW1000_GPIO_GISEN2_SHIFT)
#define DW1000_GPIO_GISEN3_MASK		(0x1 << DW1000_GPIO_GISEN3_SHIFT)
#define DW1000_GPIO_GISEN4_MASK		(0x1 << DW1000_GPIO_GISEN4_SHIFT)
#define DW1000_GPIO_GISEN5_MASK		(0x1 << DW1000_GPIO_GISEN5_SHIFT)
#define DW1000_GPIO_GISEN6_MASK		(0x1 << DW1000_GPIO_GISEN6_SHIFT)
#define DW1000_GPIO_GISEN7_MASK		(0x1 << DW1000_GPIO_GISEN7_SHIFT)
#define DW1000_GPIO_GISEN8_MASK		(0x1 << DW1000_GPIO_GISEN8_SHIFT)

#define DW1000_GPIO_GISEN0_HIGH  (0x0 << DW1000_GPIO_GISEN0_SHIFT)
#define DW1000_GPIO_GISEN0_LOW   (0x1 << DW1000_GPIO_GISEN0_SHIFT)
#define DW1000_GPIO_GISEN1_HIGH  (0x0 << DW1000_GPIO_GISEN1_SHIFT)
#define DW1000_GPIO_GISEN1_LOW   (0x1 << DW1000_GPIO_GISEN1_SHIFT)
#define DW1000_GPIO_GISEN2_HIGH  (0x0 << DW1000_GPIO_GISEN2_SHIFT)
#define DW1000_GPIO_GISEN2_LOW   (0x1 << DW1000_GPIO_GISEN2_SHIFT)
#define DW1000_GPIO_GISEN3_HIGH  (0x0 << DW1000_GPIO_GISEN3_SHIFT)
#define DW1000_GPIO_GISEN3_LOW   (0x1 << DW1000_GPIO_GISEN3_SHIFT)
#define DW1000_GPIO_GISEN4_HIGH  (0x0 << DW1000_GPIO_GISEN4_SHIFT)
#define DW1000_GPIO_GISEN4_LOW   (0x1 << DW1000_GPIO_GISEN4_SHIFT)
#define DW1000_GPIO_GISEN5_HIGH  (0x0 << DW1000_GPIO_GISEN5_SHIFT)
#define DW1000_GPIO_GISEN5_LOW   (0x1 << DW1000_GPIO_GISEN5_SHIFT)
#define DW1000_GPIO_GISEN6_HIGH  (0x0 << DW1000_GPIO_GISEN6_SHIFT)
#define DW1000_GPIO_GISEN6_LOW   (0x1 << DW1000_GPIO_GISEN6_SHIFT)
#define DW1000_GPIO_GISEN7_HIGH  (0x0 << DW1000_GPIO_GISEN7_SHIFT)
#define DW1000_GPIO_GISEN7_LOW   (0x1 << DW1000_GPIO_GISEN7_SHIFT)
#define DW1000_GPIO_GISEN8_HIGH  (0x0 << DW1000_GPIO_GISEN8_SHIFT)
#define DW1000_GPIO_GISEN8_LOW   (0x1 << DW1000_GPIO_GISEN8_SHIFT)


/* 0x26:18 GPIO_IMODE
 * Octets		Type		Mnemonic	Description
 * 4			RW			GPIO_IMODE	GPIO Interrupt Mode (Level/Edge)
 *
 * reg:26:18	bit:0		GIMOD0		GPIO IRQ Mode selection for GPIO0 input
 * reg:26:18	bit:1		GIMOD1		GPIO IRQ Mode selection for GPIO1 input
 * reg:26:18	bit:2		GIMOD2		GPIO IRQ Mode selection for GPIO2 input
 * reg:26:18	bit:3		GIMOD3		GPIO IRQ Mode selection for GPIO3 input
 * reg:26:18	bit:4		GIMOD4		GPIO IRQ Mode selection for GPIO4 input
 * reg:26:18	bit:5		GIMOD5		GPIO IRQ Mode selection for GPIO5 input
 * reg:26:18	bit:6		GIMOD6		GPIO IRQ Mode selection for GPIO6 input
 * reg:26:18	bit:7		GIMOD7		GPIO IRQ Mode selection for GPIO7 input
 * reg:26:18	bit:8		GIMOD8		GPIO IRQ Mode selection for GPIO8 input */
#define DW1000_GPIO_GIMOD0_SHIFT	(0)
#define DW1000_GPIO_GIMOD1_SHIFT	(1)
#define DW1000_GPIO_GIMOD2_SHIFT	(2)
#define DW1000_GPIO_GIMOD3_SHIFT	(3)
#define DW1000_GPIO_GIMOD4_SHIFT	(4)
#define DW1000_GPIO_GIMOD5_SHIFT	(5)
#define DW1000_GPIO_GIMOD6_SHIFT	(6)
#define DW1000_GPIO_GIMOD7_SHIFT	(7)
#define DW1000_GPIO_GIMOD8_SHIFT	(8)

#define DW1000_GPIO_GIMOD0_MASK		(0x1 << DW1000_GPIO_GIMOD0_SHIFT)
#define DW1000_GPIO_GIMOD1_MASK		(0x1 << DW1000_GPIO_GIMOD1_SHIFT)
#define DW1000_GPIO_GIMOD2_MASK		(0x1 << DW1000_GPIO_GIMOD2_SHIFT)
#define DW1000_GPIO_GIMOD3_MASK		(0x1 << DW1000_GPIO_GIMOD3_SHIFT)
#define DW1000_GPIO_GIMOD4_MASK		(0x1 << DW1000_GPIO_GIMOD4_SHIFT)
#define DW1000_GPIO_GIMOD5_MASK		(0x1 << DW1000_GPIO_GIMOD5_SHIFT)
#define DW1000_GPIO_GIMOD6_MASK		(0x1 << DW1000_GPIO_GIMOD6_SHIFT)
#define DW1000_GPIO_GIMOD7_MASK		(0x1 << DW1000_GPIO_GIMOD7_SHIFT)
#define DW1000_GPIO_GIMOD8_MASK		(0x1 << DW1000_GPIO_GIMOD8_SHIFT)

#define DW1000_GPIO_GIMOD0_LEVEL (0x0 << DW1000_GPIO_GIMOD0_SHIFT)
#define DW1000_GPIO_GIMOD0_EDGE  (0x1 << DW1000_GPIO_GIMOD0_SHIFT)
#define DW1000_GPIO_GIMOD1_LEVEL (0x0 << DW1000_GPIO_GIMOD1_SHIFT)
#define DW1000_GPIO_GIMOD1_EDGE  (0x1 << DW1000_GPIO_GIMOD1_SHIFT)
#define DW1000_GPIO_GIMOD2_LEVEL (0x0 << DW1000_GPIO_GIMOD2_SHIFT)
#define DW1000_GPIO_GIMOD2_EDGE  (0x1 << DW1000_GPIO_GIMOD2_SHIFT)
#define DW1000_GPIO_GIMOD3_LEVEL (0x0 << DW1000_GPIO_GIMOD3_SHIFT)
#define DW1000_GPIO_GIMOD3_EDGE  (0x1 << DW1000_GPIO_GIMOD3_SHIFT)
#define DW1000_GPIO_GIMOD4_LEVEL (0x0 << DW1000_GPIO_GIMOD4_SHIFT)
#define DW1000_GPIO_GIMOD4_EDGE  (0x1 << DW1000_GPIO_GIMOD4_SHIFT)
#define DW1000_GPIO_GIMOD5_LEVEL (0x0 << DW1000_GPIO_GIMOD5_SHIFT)
#define DW1000_GPIO_GIMOD5_EDGE  (0x1 << DW1000_GPIO_GIMOD5_SHIFT)
#define DW1000_GPIO_GIMOD6_LEVEL (0x0 << DW1000_GPIO_GIMOD6_SHIFT)
#define DW1000_GPIO_GIMOD6_EDGE  (0x1 << DW1000_GPIO_GIMOD6_SHIFT)
#define DW1000_GPIO_GIMOD7_LEVEL (0x0 << DW1000_GPIO_GIMOD7_SHIFT)
#define DW1000_GPIO_GIMOD7_EDGE  (0x1 << DW1000_GPIO_GIMOD7_SHIFT)
#define DW1000_GPIO_GIMOD8_LEVEL (0x0 << DW1000_GPIO_GIMOD8_SHIFT)
#define DW1000_GPIO_GIMOD8_EDGE  (0x1 << DW1000_GPIO_GIMOD8_SHIFT)


/* 0x26:1C GPIO_IBES
 * Octets		Type		Mnemonic	Description
 * 4			RW			GPIO_IBES	GPIO Interrupt "Both Edge" Select
 *
 * reg:26:1C	bit:0		GIBES0		GPIO IRQ “Both Edge” selection for GPIO0 input
 * reg:26:1C	bit:1		GIBES1		GPIO IRQ “Both Edge” selection for GPIO1 input
 * reg:26:1C	bit:2		GIBES2		GPIO IRQ “Both Edge” selection for GPIO2 input
 * reg:26:1C	bit:3		GIBES3		GPIO IRQ “Both Edge” selection for GPIO3 input
 * reg:26:1C	bit:4		GIBES4		GPIO IRQ “Both Edge” selection for GPIO4 input
 * reg:26:1C	bit:5		GIBES5		GPIO IRQ “Both Edge” selection for GPIO5 input
 * reg:26:1C	bit:6		GIBES6		GPIO IRQ “Both Edge” selection for GPIO6 input
 * reg:26:1C	bit:7		GIBES7		GPIO IRQ “Both Edge” selection for GPIO7 input
 * reg:26:1C	bit:8		GIBES8		GPIO IRQ “Both Edge” selection for GPIO8 input */
#define DW1000_GPIO_GIBES0_SHIFT	(0)
#define DW1000_GPIO_GIBES1_SHIFT	(1)
#define DW1000_GPIO_GIBES2_SHIFT	(2)
#define DW1000_GPIO_GIBES3_SHIFT	(3)
#define DW1000_GPIO_GIBES4_SHIFT	(4)
#define DW1000_GPIO_GIBES5_SHIFT	(5)
#define DW1000_GPIO_GIBES6_SHIFT	(6)
#define DW1000_GPIO_GIBES7_SHIFT	(7)
#define DW1000_GPIO_GIBES8_SHIFT	(8)

#define DW1000_GPIO_GIBES0_MASK		(0x1 << DW1000_GPIO_GIBES0_SHIFT)
#define DW1000_GPIO_GIBES1_MASK		(0x1 << DW1000_GPIO_GIBES1_SHIFT)
#define DW1000_GPIO_GIBES2_MASK		(0x1 << DW1000_GPIO_GIBES2_SHIFT)
#define DW1000_GPIO_GIBES3_MASK		(0x1 << DW1000_GPIO_GIBES3_SHIFT)
#define DW1000_GPIO_GIBES4_MASK		(0x1 << DW1000_GPIO_GIBES4_SHIFT)
#define DW1000_GPIO_GIBES5_MASK		(0x1 << DW1000_GPIO_GIBES5_SHIFT)
#define DW1000_GPIO_GIBES6_MASK		(0x1 << DW1000_GPIO_GIBES6_SHIFT)
#define DW1000_GPIO_GIBES7_MASK		(0x1 << DW1000_GPIO_GIBES7_SHIFT)
#define DW1000_GPIO_GIBES8_MASK		(0x1 << DW1000_GPIO_GIBES8_SHIFT)

#define DW1000_GPIO_GIBES0_IMODE (0x0 << DW1000_GPIO_GIBES0_SHIFT)
#define DW1000_GPIO_GIBES0_BE    (0x1 << DW1000_GPIO_GIBES0_SHIFT)
#define DW1000_GPIO_GIBES1_IMODE (0x0 << DW1000_GPIO_GIBES1_SHIFT)
#define DW1000_GPIO_GIBES1_BE    (0x1 << DW1000_GPIO_GIBES1_SHIFT)
#define DW1000_GPIO_GIBES2_IMODE (0x0 << DW1000_GPIO_GIBES2_SHIFT)
#define DW1000_GPIO_GIBES2_BE    (0x1 << DW1000_GPIO_GIBES2_SHIFT)
#define DW1000_GPIO_GIBES3_IMODE (0x0 << DW1000_GPIO_GIBES3_SHIFT)
#define DW1000_GPIO_GIBES3_BE    (0x1 << DW1000_GPIO_GIBES3_SHIFT)
#define DW1000_GPIO_GIBES4_IMODE (0x0 << DW1000_GPIO_GIBES4_SHIFT)
#define DW1000_GPIO_GIBES4_BE    (0x1 << DW1000_GPIO_GIBES4_SHIFT)
#define DW1000_GPIO_GIBES5_IMODE (0x0 << DW1000_GPIO_GIBES5_SHIFT)
#define DW1000_GPIO_GIBES5_BE    (0x1 << DW1000_GPIO_GIBES5_SHIFT)
#define DW1000_GPIO_GIBES6_IMODE (0x0 << DW1000_GPIO_GIBES6_SHIFT)
#define DW1000_GPIO_GIBES6_BE    (0x1 << DW1000_GPIO_GIBES6_SHIFT)
#define DW1000_GPIO_GIBES7_IMODE (0x0 << DW1000_GPIO_GIBES7_SHIFT)
#define DW1000_GPIO_GIBES7_BE    (0x1 << DW1000_GPIO_GIBES7_SHIFT)
#define DW1000_GPIO_GIBES8_IMODE (0x0 << DW1000_GPIO_GIBES8_SHIFT)
#define DW1000_GPIO_GIBES8_BE    (0x1 << DW1000_GPIO_GIBES8_SHIFT)


/* 0x26:20 GPIO_ICLR
 * Octets		Type		Mnemonic	Description
 * 4			RW			GPIO_ICLR	GPIO Interrupt Latch Clear
 *
 * reg:26:20	bit:0		GICLR0		GPIO IRQ latch clear for GPIO0 input
 * reg:26:20	bit:1		GICLR1		GPIO IRQ latch clear for GPIO1 input
 * reg:26:20	bit:2		GICLR2		GPIO IRQ latch clear for GPIO2 input
 * reg:26:20	bit:3		GICLR3		GPIO IRQ latch clear for GPIO3 input
 * reg:26:20	bit:4		GICLR4		GPIO IRQ latch clear for GPIO4 input
 * reg:26:20	bit:5		GICLR5		GPIO IRQ latch clear for GPIO5 input
 * reg:26:20	bit:6		GICLR6		GPIO IRQ latch clear for GPIO6 input
 * reg:26:20	bit:7		GICLR7		GPIO IRQ latch clear for GPIO7 input
 * reg:26:20	bit:8		GICLR8		GPIO IRQ latch clear for GPIO8 input */
#define DW1000_GPIO_GICLR0_SHIFT	(0)
#define DW1000_GPIO_GICLR1_SHIFT	(1)
#define DW1000_GPIO_GICLR2_SHIFT	(2)
#define DW1000_GPIO_GICLR3_SHIFT	(3)
#define DW1000_GPIO_GICLR4_SHIFT	(4)
#define DW1000_GPIO_GICLR5_SHIFT	(5)
#define DW1000_GPIO_GICLR6_SHIFT	(6)
#define DW1000_GPIO_GICLR7_SHIFT	(7)
#define DW1000_GPIO_GICLR8_SHIFT	(8)

#define DW1000_GPIO_GICLR0_MASK		(0x1 << DW1000_GPIO_GICLR0_SHIFT)
#define DW1000_GPIO_GICLR1_MASK		(0x1 << DW1000_GPIO_GICLR1_SHIFT)
#define DW1000_GPIO_GICLR2_MASK		(0x1 << DW1000_GPIO_GICLR2_SHIFT)
#define DW1000_GPIO_GICLR3_MASK		(0x1 << DW1000_GPIO_GICLR3_SHIFT)
#define DW1000_GPIO_GICLR4_MASK		(0x1 << DW1000_GPIO_GICLR4_SHIFT)
#define DW1000_GPIO_GICLR5_MASK		(0x1 << DW1000_GPIO_GICLR5_SHIFT)
#define DW1000_GPIO_GICLR6_MASK		(0x1 << DW1000_GPIO_GICLR6_SHIFT)
#define DW1000_GPIO_GICLR7_MASK		(0x1 << DW1000_GPIO_GICLR7_SHIFT)
#define DW1000_GPIO_GICLR8_MASK		(0x1 << DW1000_GPIO_GICLR8_SHIFT)

#define DW1000_GPIO_GICLR0 (0x1 << DW1000_GPIO_GICLR0_SHIFT)
#define DW1000_GPIO_GICLR1 (0x1 << DW1000_GPIO_GICLR1_SHIFT)
#define DW1000_GPIO_GICLR2 (0x1 << DW1000_GPIO_GICLR2_SHIFT)
#define DW1000_GPIO_GICLR3 (0x1 << DW1000_GPIO_GICLR3_SHIFT)
#define DW1000_GPIO_GICLR4 (0x1 << DW1000_GPIO_GICLR4_SHIFT)
#define DW1000_GPIO_GICLR5 (0x1 << DW1000_GPIO_GICLR5_SHIFT)
#define DW1000_GPIO_GICLR6 (0x1 << DW1000_GPIO_GICLR6_SHIFT)
#define DW1000_GPIO_GICLR7 (0x1 << DW1000_GPIO_GICLR7_SHIFT)
#define DW1000_GPIO_GICLR8 (0x1 << DW1000_GPIO_GICLR8_SHIFT)


/* 0x26:24 GPIO_IDBE
 * Octets		Type		Mnemonic	Description
 * 4			RW			GPIO_IDBE	GPIO Interrupt De-bounce Enable
 *
 * reg:26:24	bit:0		GIDBE0		GPIO0 IRQ de-bounce enable
 * reg:26:24	bit:1		GIDBE1		GPIO1 IRQ de-bounce enable
 * reg:26:24	bit:2		GIDBE2		GPIO2 IRQ de-bounce enable
 * reg:26:24	bit:3		GIDBE3		GPIO3 IRQ de-bounce enable
 * reg:26:24	bit:4		GIDBE4		GPIO4 IRQ de-bounce enable
 * reg:26:24	bit:5		GIDBE5		GPIO5 IRQ de-bounce enable
 * reg:26:24	bit:6		GIDBE6		GPIO6 IRQ de-bounce enable
 * reg:26:24	bit:7		GIDBE7		GPIO7 IRQ de-bounce enable
 * reg:26:24	bit:8		GIDBE8		GPIO8 IRQ de-bounce enable */
#define DW1000_GPIO_GIDBE0_SHIFT	(0)
#define DW1000_GPIO_GIDBE1_SHIFT	(1)
#define DW1000_GPIO_GIDBE2_SHIFT	(2)
#define DW1000_GPIO_GIDBE3_SHIFT	(3)
#define DW1000_GPIO_GIDBE4_SHIFT	(4)
#define DW1000_GPIO_GIDBE5_SHIFT	(5)
#define DW1000_GPIO_GIDBE6_SHIFT	(6)
#define DW1000_GPIO_GIDBE7_SHIFT	(7)
#define DW1000_GPIO_GIDBE8_SHIFT	(8)

#define DW1000_GPIO_GIDBE0_MASK		(0x1 << DW1000_GPIO_GIDBE0_SHIFT)
#define DW1000_GPIO_GIDBE1_MASK		(0x1 << DW1000_GPIO_GIDBE1_SHIFT)
#define DW1000_GPIO_GIDBE2_MASK		(0x1 << DW1000_GPIO_GIDBE2_SHIFT)
#define DW1000_GPIO_GIDBE3_MASK		(0x1 << DW1000_GPIO_GIDBE3_SHIFT)
#define DW1000_GPIO_GIDBE4_MASK		(0x1 << DW1000_GPIO_GIDBE4_SHIFT)
#define DW1000_GPIO_GIDBE5_MASK		(0x1 << DW1000_GPIO_GIDBE5_SHIFT)
#define DW1000_GPIO_GIDBE6_MASK		(0x1 << DW1000_GPIO_GIDBE6_SHIFT)
#define DW1000_GPIO_GIDBE7_MASK		(0x1 << DW1000_GPIO_GIDBE7_SHIFT)
#define DW1000_GPIO_GIDBE8_MASK		(0x1 << DW1000_GPIO_GIDBE8_SHIFT)

#define DW1000_GPIO_GIDBE0_DISABLE (0x0 << DW1000_GPIO_GIDBE0_SHIFT)
#define DW1000_GPIO_GIDBE0_ENABLE  (0x1 << DW1000_GPIO_GIDBE0_SHIFT)
#define DW1000_GPIO_GIDBE1_DISABLE (0x0 << DW1000_GPIO_GIDBE1_SHIFT)
#define DW1000_GPIO_GIDBE1_ENABLE  (0x1 << DW1000_GPIO_GIDBE1_SHIFT)
#define DW1000_GPIO_GIDBE2_DISABLE (0x0 << DW1000_GPIO_GIDBE2_SHIFT)
#define DW1000_GPIO_GIDBE2_ENABLE  (0x1 << DW1000_GPIO_GIDBE2_SHIFT)
#define DW1000_GPIO_GIDBE3_DISABLE (0x0 << DW1000_GPIO_GIDBE3_SHIFT)
#define DW1000_GPIO_GIDBE3_ENABLE  (0x1 << DW1000_GPIO_GIDBE3_SHIFT)
#define DW1000_GPIO_GIDBE4_DISABLE (0x0 << DW1000_GPIO_GIDBE4_SHIFT)
#define DW1000_GPIO_GIDBE4_ENABLE  (0x1 << DW1000_GPIO_GIDBE4_SHIFT)
#define DW1000_GPIO_GIDBE5_DISABLE (0x0 << DW1000_GPIO_GIDBE5_SHIFT)
#define DW1000_GPIO_GIDBE5_ENABLE  (0x1 << DW1000_GPIO_GIDBE5_SHIFT)
#define DW1000_GPIO_GIDBE6_DISABLE (0x0 << DW1000_GPIO_GIDBE6_SHIFT)
#define DW1000_GPIO_GIDBE6_ENABLE  (0x1 << DW1000_GPIO_GIDBE6_SHIFT)
#define DW1000_GPIO_GIDBE7_DISABLE (0x0 << DW1000_GPIO_GIDBE7_SHIFT)
#define DW1000_GPIO_GIDBE7_ENABLE  (0x1 << DW1000_GPIO_GIDBE7_SHIFT)
#define DW1000_GPIO_GIDBE8_DISABLE (0x0 << DW1000_GPIO_GIDBE8_SHIFT)
#define DW1000_GPIO_GIDBE8_ENABLE  (0x1 << DW1000_GPIO_GIDBE8_SHIFT)


/* 0x26:28 GPIO_RAW
 * Octets		Type		Mnemonic	Description
 * 4			RO			GPIO_RAW	GPIO Raw State
 *
 * reg:26:28	bit:0		GRAWP0		GPIO0 raw state
 * reg:26:28	bit:1		GRAWP1		GPIO1 raw state
 * reg:26:28	bit:2		GRAWP2		GPIO2 raw state
 * reg:26:28	bit:3		GRAWP3		GPIO3 raw state
 * reg:26:28	bit:4		GRAWP4		GPIO4 raw state
 * reg:26:28	bit:5		GRAWP5		GPIO5 raw state
 * reg:26:28	bit:6		GRAWP6		GPIO6 raw state
 * reg:26:28	bit:7		GRAWP7		GPIO7 raw state
 * reg:26:28	bit:8		GRAWP8		GPIO8 raw state */
#define DW1000_GPIO_GRAWP0_SHIFT	(00
#define DW1000_GPIO_GRAWP1_SHIFT	(10
#define DW1000_GPIO_GRAWP2_SHIFT	(20
#define DW1000_GPIO_GRAWP3_SHIFT	(30
#define DW1000_GPIO_GRAWP4_SHIFT	(40
#define DW1000_GPIO_GRAWP5_SHIFT	(50
#define DW1000_GPIO_GRAWP6_SHIFT	(60
#define DW1000_GPIO_GRAWP7_SHIFT	(70
#define DW1000_GPIO_GRAWP8_SHIFT	(80

#define DW1000_GPIO_GRAWP0_MASK		(0x1 << DW1000_GPIO_GRAWP0_SHIFT)
#define DW1000_GPIO_GRAWP1_MASK		(0x1 << DW1000_GPIO_GRAWP1_SHIFT)
#define DW1000_GPIO_GRAWP2_MASK		(0x1 << DW1000_GPIO_GRAWP2_SHIFT)
#define DW1000_GPIO_GRAWP3_MASK		(0x1 << DW1000_GPIO_GRAWP3_SHIFT)
#define DW1000_GPIO_GRAWP4_MASK		(0x1 << DW1000_GPIO_GRAWP4_SHIFT)
#define DW1000_GPIO_GRAWP5_MASK		(0x1 << DW1000_GPIO_GRAWP5_SHIFT)
#define DW1000_GPIO_GRAWP6_MASK		(0x1 << DW1000_GPIO_GRAWP6_SHIFT)
#define DW1000_GPIO_GRAWP7_MASK		(0x1 << DW1000_GPIO_GRAWP7_SHIFT)
#define DW1000_GPIO_GRAWP8_MASK		(0x1 << DW1000_GPIO_GRAWP8_SHIFT)


/* 0x27 Digital Receiver Configuration
 * Octets		Type		Mnemonic	Description
 * 44			-			DX_CONF		Digital Receiver Configuration
 *
 * OFFSET		Mnemonic				Description
 * 0x02			DRX_TUNE0b				Digital Tuning Register 0b
 * 0x04			DRX_TUNE1a				Digital Tuning Register 1a
 * 0x06			DRX_TUNE1b				Digital Tuning Register 1b
 * 0x08			DRX_TUNE2				Digital Tuning Register 2
 * 0x20			DRX_SFDTOC				SFD timeout
 * 0x24			DRX_PRETOC				Preamble detection timeout
 * 0x26			DRX_TUNE4H				Digital Tuning Register 4H
 * 0x28			DRX_CAR_INT				Carrier Recovery Integrator Register
 * 0X2C			RXPACC_NOSAT			Unsaturated accumulated preamble symbols */
#define DW1000_DRX_TUNE0B_OFFSET	(0x02)
#define DW1000_DRX_TUNE1A_OFFSET	(0x04)
#define DW1000_DRX_TUNE1B_OFFSET	(0x06)
#define DW1000_DRX_TUNE2_OFFSET		(0x08)
#define DW1000_DRX_SFDTOC_OFFSET	(0x20)
#define DW1000_DRX_PRETOC_OFFSET	(0x24)
#define DW1000_DRX_TUNE4H_OFFSET	(0x26)
#define DW1000_DRX_CAR_INT_OFFSET	(0x28)
#define DW1000_RXPACC_NOSAT_OFFSET	(0X2C)


/* 0x27:02 DRX_TUNE0b
 * Octets		Type		Mnemonic	Description
 * 2			RW			DRX_TUNE0b	Digital Tuning register 0b
 *
 *
 * Data Rate	SFD configuration		DRX_TUNE0b
 * 110 kbps		Standard SFD			0x000A
 * 				Non-Standard SFD		0x0016
 * 850 kbps		Standard SFD			0x0001
 * 				Non-Standard SFD		0x0006
 * 6.8 Mbps		Standard SFD			0x0001
 * 				Non-Standard SFD		0x0002
 *
 * See table 21
 * See table 22 */
#define DW1000_DRX_TUNE0B_100_KBPS_STD		(0x000A)
#define DW1000_DRX_TUNE0B_100_KBPS_NON_STD 	(0x0016)
#define DW1000_DRX_TUNE0B_805_KBPS_STD		(0x0001)
#define DW1000_DRX_TUNE0B_850_KBPS_NON_STD	(0x0006)
#define DW1000_DRX_TUNE0B_6800_KBPS_STD		(0x0001)
#define DW1000_DRX_TUNE0B_6800_KBPS_NON_STD	(0x0002)


/* 0x27:04 DRX_TUNE1a
 * Octets		Type		Mnemonic	Description
 * 2			RW			DRX_TUNE1a	Digital Tuning register 1a
 *
 * RXPRF configuration	DRX_TUNE1a
 * (1) = 16 MHz PRF		0x0087
 * (2) = 64 MHz PRF		0x008D */
#define DW1000_DRX_TUNE1A_16_MHZ_PRF	(0x0087)
#define DW1000_DRX_TUNE1A_64_MHZ_PRF	(0x008D)


/* 0x27:06 DRX_TUNE1b
 * Octets		Type		Mnemonic	Description
 * 2			RW			DRX_TUNE1b	Digital Tuning register 1b
 *
 * Use case																		DRX_TUNE1b
 * Preamble lengths > 1024 symbols, for 110 kbps operation						0x0064
 * Preamble lengths 128 to 1024 symbols, for 850 kbps and 6.8 Mbps operation	0x0020
 * Preamble length = 64 symbols, for 6.8 Mbps operation							0x0010 */
#define DW1000_DRX_TUNE1B_PREAMBLE_GT_1024_100_KBPS					(0x0064)
#define DW1000_DRX_TUNE1B_PREAMBLE_128_TO_1024_850_KBPS_6800_KBPS	(0x0020)
#define DW1000_DRX_TUNE1B_PREAMBLE_64_6800_KBPS						(0x0010)


/* 0x27:08 DRX_TUNE2
 * Octets		Type		Mnemonic	Description
 * 4			RW			DRX_TUNE2	Digital Tuning Register 2
 *
 * PAC size		RXPRF configuration		DRX_TUNE2
 * 8			16 MHz PRF				0x311A002D
 * 				64 MHz PRF				0x313B006B
 * 16			16 MHz PRF				0x331A0052
 * 				64 MHz PRF				0x333B00BE
 * 32			16 MHz PRF				0x351A009A
 * 				64 MHz PRF				0x353B015E
 * 64			16 MHz PRF				0x371A011D
 * 				64 MHz PRF				0x373B0296 */
#define DW1000_DRX_TUNE2_PAC_8_16_MHZ_PRF	(0x311A002D)
#define DW1000_DRX_TUNE2_PAC_8_64_MHZ_PRF	(0x313B006B)
#define DW1000_DRX_TUNE2_PAC_16_16_MHZ_PRF	(0x331A0052)
#define DW1000_DRX_TUNE2_PAC_16_64_MHZ_PRF	(0x333B00BE)
#define DW1000_DRX_TUNE2_PAC_32_16_MHZ_PRF	(0x351A009A)
#define DW1000_DRX_TUNE2_PAC_32_64_MHZ_PRF	(0x353B015E)
#define DW1000_DRX_TUNE2_PAC_64_16_MHZ_PRF	(0x371A011D)
#define DW1000_DRX_TUNE2_PAC_64_64_MHZ_PRF	(0x373B0296)


/* 0x27:20 DRX_SFDTOC @TODO: see 7.2.40.7 warning
 * Octets		Type		Mnemonic	Description
 * 2			RW			DRX_SFDTOC	SFD detection timeout count */

/* 0x27:24 DRX_PRETOC
 * Octets		Type		Mnemonic	Description
 * 2			RW			DRX_PRETOC	Preamble detection timeout count */

/* 0x27:26 DRX_TUNE4H
 * Octets		Type		Mnemonic	Description
 * 2			RW			DRX_TUNE4H	Digital Tuning Register
 *
 * Expected Receive Preamble			DRX_TUNE4H
 * Length in Symbols
 * 64									0x0010
 * 128 or greater						0x0028 */
#define DW1000_DRX_TUNE4H_PREAMBLE_64		(0x0010)
#define DW1000_DRX_TUNE4H_PREAMBLE_GTE_128	(0x0028)


/* 0x27:28 DRX_CAR_INT
 * Octets		Type		Mnemonic	Description
 * 3			RO			DRX_CAR_INT	Carrier Recovery Integrator Register */
#define DW1000_DRX_FS						(998.4E6f)
#define DW1000_DRX_NSAMPLES_110KBPS			(8192)
#define DW1000_DRX_NSAMPLES_850KBPS			(1024)
#define DW1000_DRX_NSAMPLES_6800KBPS		(1024)

/* Freq_Offset = (Cint * 2^-17) / (2*NSamp/Fs)
 *            => (Cint * Fs) / (2^18 * NSamp) */
#define DW1000_DRX_FOFFSET_110KBPS			(DW1000_DRX_FS / 262144 / DW1000_DRX_NSAMPLES_110KBPS)
#define DW1000_DRX_FOFFSET_850KBPS			(DW1000_DRX_FS / 262144 / DW1000_DRX_NSAMPLES_850KBPS)
#define DW1000_DRX_FOFFSET_6800KBPS			(DW1000_DRX_FS / 262144 / DW1000_DRX_NSAMPLES_6800KBPS)

/* Clock_Offset = -Freq_Offset / Fc
 * (in ppm):    = -1E6 * Freq_Offset / FC */
#define DW1000_DRX_CLKOFFSET_CH1			(-1.0f / DW1000_FC_CH1)
#define DW1000_DRX_CLKOFFSET_CH2			(-1.0f / DW1000_FC_CH2)
#define DW1000_DRX_CLKOFFSET_CH3			(-1.0f / DW1000_FC_CH3)
#define DW1000_DRX_CLKOFFSET_CH4			(-1.0f / DW1000_FC_CH4)
#define DW1000_DRX_CLKOFFSET_CH5			(-1.0f / DW1000_FC_CH5)
#define DW1000_DRX_CLKOFFSET_CH7			(-1.0f / DW1000_FC_CH7)

// #define DW1000_DRX_FOFFSET_110KBPS_CH2		(-0.1164E-3f)
// #define DW1000_DRX_FOFFSET_110KBPS_CH3		(-0.1035E-3f)
// #define DW1000_DRX_FOFFSET_110KBPS_CH5		(-0.0716E-3f)
// #define DW1000_DRX_FOFFSET_850KBPS_CH2		(-0.9313E-3f)
// #define DW1000_DRX_FOFFSET_850KBPS_CH3		(-0.8278E-3f)
// #define DW1000_DRX_FOFFSET_850KBPS_CH5		(-0.5731E-3f)
// #define DW1000_DRX_FOFFSET_6800KBPS_CH2		(-0.9313E-3f)
// #define DW1000_DRX_FOFFSET_6800KBPS_CH3		(-0.8278E-3f)
// #define DW1000_DRX_FOFFSET_6800KBPS_CH5		(-0.5731E-3f)


/* 0x27:2C RXPACC_NOSAT
 * Octets		Type		Mnemonic		Description
 * 2			RO			RXPACC_NOSAT	Digital Debug Register */



/* 0x28 Analog RF Configuration Block
 * Octets		Type		Mnemonic	Description
 * 58			-			RF_CONF		Analog RF Configuration
 *
 * OFFSET		Mnemonic	Description
 * 0x00			RF_CONF		RF Configuration Register
 * 0x04			RF_RES1		Reserved area 1
 * 0x0B			RF_RXCTRLH	Analog RX Control Register
 * 0x0C			RF_TXCTRL	Analog TX Control Register
 * 0x10			RF_RES2		Reserved area 2
 * 0x2C			RF_STATUS	RF Status Register
 * 0x30			LDOTUNE		LDO voltage tuning */
#define DW1000_RF_CONF_OFFSET		(0x00)
#define DW1000_RF_RES1_OFFSET		(0x04)
#define DW1000_RF_RXCTRLH_OFFSET	(0x0B)
#define DW1000_RF_TXCTRL_OFFSET		(0x0C)
#define DW1000_RF_RES2_OFFSET		(0x10)
#define DW1000_RF_STATUS_OFFSET		(0x2C)
#define DW1000_LDOTUNE_OFFSET		(0x30)


/* 0x28:00 RF_CONF
 * Octets		Type		Mnemonic	Description
 * 4			RW			RF_CONF		RF Configuration Register
 *
 * reg:28:00	bits:12-8	TXFEN		Transmit block force enable
 * reg:28:00	bits:15-13	PLLFEN		PLL block force enable
 * reg:28:00	bits:20-16	LDOFEN		LDO force enable
 * reg:28:00	bits:22-21	TXRXSW		Force TX/RX switch */
#define DW1000_RF_CONF_TXFEN_SHIFT	(8)
#define DW1000_RF_CONF_PLLFEN_SHIFT	(13)
#define DW1000_RF_CONF_LDOFEN_SHIFT	(16)
#define DW1000_RF_CONF_TXRXSW_SHIFT	(21)

#define DW1000_RF_CONF_TXFEN_MASK	(0x1F << DW1000_RF_CONF_TXFEN_SHIFT)
#define DW1000_RF_CONF_PLLFEN_MASK	(0x7  << DW1000_RF_CONF_PLLFEN_SHIFT)
#define DW1000_RF_CONF_LDOFEN_MASK	(0x1F << DW1000_RF_CONF_LDOFEN_SHIFT)
#define DW1000_RF_CONF_TXRXSW_MASK	(0x3  << DW1000_RF_CONF_TXRXSW_SHIFT)


/* 0x28:0B RF_RXCTRLH
 * Octets		Type		Mnemonic	Description
 * 1			RW			RF_RXCTRLH	Analog RX Control Register
 *
 * RX Channel		RF_RXCTRLH
 * 1, 2, 3, or 5	0xD8
 * 4 or 7			0xBC */
#define DW1000_RF_RXCTRLH_CH_1 (0xD8)
#define DW1000_RF_RXCTRLH_CH_2 (0xD8)
#define DW1000_RF_RXCTRLH_CH_3 (0xD8)
#define DW1000_RF_RXCTRLH_CH_4 (0xBC)
#define DW1000_RF_RXCTRLH_CH_5 (0xD8)
#define DW1000_RF_RXCTRLH_CH_7 (0xBC)


/* 0x28:0C RF_TXCTRL
 * Octets		Type		Mnemonic	Description
 * 3			RW			RF_TXCTRL	Analog TX Control Register
 *
 * TX Channel	RF_TXCTRL
 * 1			0x00005C40
 * 2			0x00045CA0
 * 3			0x00086CC0
 * 4			0x00045C80
 * 5			0x001E3FE3
 * 7			0x001E7DE0
 *
 * reg:28:0C	bits:8:5	TXMTUNE		Transmit mixer tuning register
 * reg:28:0C	bits:11:9	TXMQ		Transmit mixer Q-factor tuning register */
#define DW1000_RF_TXCTRL_TXMTUNE_SHIFT	(5)
#define DW1000_RF_TXCTRL_TXMQ_SHIFT		(9)

#define DW1000_RF_TXCTRL_TXMTUNE_MASK	(0xF << DW1000_RF_TXCTRL_TXMTUNE_SHIFT)
#define DW1000_RF_TXCTRL_TXMQ_MASK		(0x7 << DW1000_RF_TXCTRL_TXMQ_SHIFT)

#define DW1000_RF_TXCTRL_CH_1	(0x00005C40)
#define DW1000_RF_TXCTRL_CH_2	(0x00045CA0)
#define DW1000_RF_TXCTRL_CH_3	(0x00086CC0)
#define DW1000_RF_TXCTRL_CH_4	(0x00045C80)
#define DW1000_RF_TXCTRL_CH_5	(0x001E3FE3)
#define DW1000_RF_TXCTRL_CH_7	(0x001E7DE0)


/* 0x28:2C RF_STATUS
 * Octets		Type		Mnemonic	Description
 * 4			RO			RF_STATUS	RF Status Register
 *
 * reg:28:2C	bit:0		CPLLLOCK	Clock PLL Lock status
 * reg:28:2C	bit:1		CPLLLOW		Clock PLL Low flag status bit
 * reg:28:2C	bit:2		CPLLHIGH	Clock PLL High flag status bit
 * reg:28:2C	bit:3		RFPLLLOCK	RF PLL Lock status */
#define DW1000_RF_STATUS_CPLLLOCK_SHIFT		(0)
#define DW1000_RF_STATUS_CPLLLOW_SHIFT		(1)
#define DW1000_RF_STATUS_CPLLHIGH_SHIFT		(2)
#define DW1000_RF_STATUS_RFPLLLOCK_SHIFT	(3)

#define DW1000_RF_STATUS_CPLLLOCK_MASK		(0x1 << DW1000_RF_STATUS_CPLLLOCK_SHIFT)
#define DW1000_RF_STATUS_CPLLLOW_MASK		(0x1 << DW1000_RF_STATUS_CPLLLOW_SHIFT)
#define DW1000_RF_STATUS_CPLLHIGH_MASK		(0x1 << DW1000_RF_STATUS_CPLLHIGH_SHIFT)
#define DW1000_RF_STATUS_RFPLLLOCK_MASK		(0x1 << DW1000_RF_STATUS_RFPLLLOCK_SHIFT)

#define DW1000_RF_STATUS_CPLLLOCK	(0x1 << DW1000_RF_STATUS_CPLLLOCK_SHIFT)
#define DW1000_RF_STATUS_CPLLLOW	(0x1 << DW1000_RF_STATUS_CPLLLOW_SHIFT)
#define DW1000_RF_STATUS_CPLLHIGH	(0x1 << DW1000_RF_STATUS_CPLLHIGH_SHIFT)
#define DW1000_RF_STATUS_RFPLLLOCK	(0x1 << DW1000_RF_STATUS_RFPLLLOCK_SHIFT)


/* 0x28:30 LDOTUNE
 * Octets		Type		Mnemonic	Description
 * 5			RW			LDOTUNE		Internal LDO voltage tuning parameter
 *
 * reg:28:30	bits:39:0	LDOTUNE	LDO Output voltage control */
#define DW1000_LDOTUNE_LOW_SHIFT	(0)
#define DW1000_LDOTUNE_HIGH_SHIFT	(0)

#define DW1000_LDOTUNE_LOW_MASK		(0xFFFFFFFF << DW1000_LDOTUNE_LOW_SHIFT)
#define DW1000_LDOTUNE_HIGH_MASK	(0xFF       << DW1000_LDOTUNE_HIGH_SHIFT)


/* 0x2A Transmitter Calibration Block
 * Octets		Type		Mnemonic	Description
 * 52			-			TX_CAL		Transmitter calibration block
 *
 * OFFSET		Mnemonic				Description
 * 0x00			TC_SARC					Transmitter Calibration – SAR control
 * 0x03			TC_SARL					Transmitter Calibration – Latest SAR readings
 * 0x06			TC_SARW					Transmitter Calibration – SAR readings at last Wake-Up
 * 0x08			TC_PG_CTRL				Transmitter Calibration – Pulse Generator Control
 * 0x09			TC_PG_STATUS			Transmitter Calibration – Pulse Generator Status
 * 0x0B			TC_PGDELAY				Transmitter Calibration – Pulse Generator Delay
 * 0x0C			TC_PGTEST				Transmitter Calibration – Pulse Generator Test */
#define DW1000_TC_SARC_OFFSET		(0x00)
#define DW1000_TC_SARL_OFFSET		(0x03)
#define DW1000_TC_SARW_OFFSET		(0x06)
#define DW1000_TC_PG_CTRL_OFFSET	(0x08)
#define DW1000_TC_PG_STATUS_OFFSET	(0x09)
#define DW1000_TC_PGDELAY_OFFSET	(0x0B)
#define DW1000_TC_PGTEST_OFFSET		(0x0C)


/* 0x2A:00 TC_SARC
 * Octets		Type		Mnemonic	Description
 * 2			RW			TC_SARC		Transmitter Calibration. SAR Control
 *
 * reg:2A:00	bit:0		SAR_CTRL	SAR Enable */
#define DW1000_TC_SARC_SAR_CTRL_SHIFT 	(0)

#define DW1000_TC_SARC_SAR_CTRL_MASK	(0x1 << DW1000_TC_SARC_SAR_CTRL_SHIFT)

#define DW1000_TC_SARC (0x1 << DW1000_TC_SARC_SAR_CTRL_SHIFT)


/* 0x2A:03 TC_SARL
 * Octets		Type		Mnemonic	Description
 * 3			RO			TC_SARL		Transmitter Calibration (latest SAR readings)
 *
 * reg:2A:03	bits:7–0	SAR_LVBAT	Latest SAR reading for voltage level
 * reg:2A:03	bits:15–8	SAR_LTEMP	Latest SAR reading for temperature level */
#define DW1000_TC_SARL_SAR_LVBAT_SHIFT	(0)
#define DW1000_TC_SARL_SAR_LTEMP_SHIFT	(8)

#define DW1000_TC_SARL_SAR_LVBAT_MASK	(0xFF << DW1000_TC_SARL_SAR_LVBAT_SHIFT)
#define DW1000_TC_SARL_SAR_LTEMP_MASK	(0xFF << DW1000_TC_SARL_SAR_LTEMP_SHIFT)


/* 0x2A:06 TC_SARW
 * Octets		Type		Mnemonic	Description
 * 2			RO			TC_SARW		Transmitter Calibration (SAR readings at last Wake-Up)
 *
 * reg:2A:06	bits:7–0	SAR_WBAT	SAR reading of voltage level taken at last wakeup event
 * reg:2A:06	bits:15–8	SAR_WTEMP	SAR reading of temperature level taken at last wakeup event
 */
#define DW1000_TC_SAR_WBAT_SHIFT	(0)
#define DW1000_TC_SAR_WTEMP_SHIFT	(8)

#define DW1000_TC_SAR_WBAT_MASK		(0xFF << DW1000_TC_SAR_WBAT_SHIFT)
#define DW1000_TC_SAR_WTEMP_MASK	(0xFF << DW1000_TC_SAR_WTEMP_SHIFT)


/* 0x2A:08 TC_PG_CTRL
 * Octets		Type		Mnemonic	Description
 * 1			RW			TC_PG_CTRL	Transmitter Calibration. Pulse Generator Control
 *
 * reg:2A:08	bit:0		PG_START	Pulse generator calibration start
 * reg:2A:08	bit:5-2		PG_TMEAS	Pulse generator calibration duration */
#define DW1000_TC_PG_START_SHIFT	(0)
#define DW1000_TC_PG_TMEAS_SHIFT	(2)

#define DW1000_TC_PG_START_MASK		(0x1 << DW1000_TC_PG_START_SHIFT)
#define DW1000_TC_PG_TMEAS_MASK		(0xF << DW1000_TC_PG_TMEAS_SHIFT)

#define DW1000_TC_PG_START (0x1 << DW1000_TC_PG_START_SHIFT)


/* 0x2A:09 TC_PG_STATUS
 * Octets		Type		Mnemonic		Description
 * 2			RO			TC_PG_STATUS	Transmitter Calibration. PG Status */
#define DW1000_TC_PG_STATUS_DELAY_CNT_SHIFT	(0)

#define DW1000_TC_PG_STATUS_DELAY_CNT_MASK	(0xFFF << DW1000_TC_PG_STATUS_DELAY_CNT_SHIFT)


/* 0x2A:0B TC_PGDELAY
 * Octets		Type		Mnemonic	Description
 * 1			RW			TC_PGDELAY	Transmitter Calibration. Pulse Generator Delay
 *
 * TX Channel	TC_PGDELAY
 * 1			0xC9
 * 2			0xC2
 * 3			0xC5
 * 4			0x95
 * 5			0xC0
 * 7			0x93 */
#define DW1000_TC_PGDELAY_CH_1	(0xC9)
#define DW1000_TC_PGDELAY_CH_2	(0xC2)
#define DW1000_TC_PGDELAY_CH_3	(0xC5)
#define DW1000_TC_PGDELAY_CH_4	(0x95)
#define DW1000_TC_PGDELAY_CH_5	(0xC0)
#define DW1000_TC_PGDELAY_CH_7	(0x93)


/* 0x2A:0C TC_PGTEST
 * Octets		Type		Mnemonic	Description
 * 1			RW			TC_PGTEST	Transmitter Calibration. Pulse Generator Test
 *
 * MODE									TC_PGTEST
 * Normal operation						0x00
 * Continuous Wave (CW) Test Mode		0x13 */
#define DW1000_TC_PGTEST_NORMAL			(0x00)
#define DW1000_TC_PGTEST_CW_TEST_MODE	(0x13)


/* 0x2B Frequency Synthesiser Control Block
 * Octets		Type		Mnemonic	Description
 * 21			-			FS_CTRL		Frequency synthesiser control block
 *
 * OFFSET		Mnemonic	Description
 * 0x00			FS_RES1		Frequency synthesiser – Reserved area 1
 * 0x07			FS_PLLCFG	Frequency synthesiser – PLL configuration
 * 0x0B			FS_PLLTUNE	Frequency synthesiser – PLL Tuning
 * 0x0C			FS_RES2		Frequency synthesiser – Reserved area 2
 * 0x0E			FS_XTALT	Frequency synthesiser – Crystal trim
 * 0x0F			FS_RES3		Frequency synthesiser – Reserved area 3 */
#define DW1000_FS_RES1_OFFSET		(0x00)
#define DW1000_FS_PLLCFG_OFFSET		(0x07)
#define DW1000_FS_PLLTUNE_OFFSET	(0x0B)
#define DW1000_FS_RES2_OFFSET		(0x0C)
#define DW1000_FS_XTALT_OFFSET		(0x0E)
#define DW1000_FS_RES3_OFFSET		(0x0F)


/* 0x2B:07 FS_PLLCFG
 * Octets		Type		Mnemonic	Description
 * 4			RW			FS_PLLCFG	Frequency synthesiser. PLL configuration
 *
 * Operating Channel		FS_PLLCFG
 * 1						0x09000407
 * 2,4						0x08400508
 * 3						0x08401009
 * 5, 7						0x0800041D */
#define DW1000_FS_PLLCFG_CH_1	(0x09000407)
#define DW1000_FS_PLLCFG_CH_2	(0x08400508)
#define DW1000_FS_PLLCFG_CH_4	(0x08400508)
#define DW1000_FS_PLLCFG_CH_3	(0x08401009)
#define DW1000_FS_PLLCFG_CH_5	(0x0800041D)
#define DW1000_FS_PLLCFG_CH_7	(0x0800041D)


/* 0x2B:0B FS_PLLTUNE
 * Octets		Type		Mnemonic	Description
 * 1			RW			FS_PLLTUNE	Frequency synthesiser. PLL Tuning
 *
 * Operating Channel		FS_PLLTUNE
 * 1						0x1E
 * 2,4						0x26
 * 3						0x56
 * 5,7						0xBE */
#define DW1000_FS_PLLTUNE_CH_1	(0x1E)
#define DW1000_FS_PLLTUNE_CH_2	(0x26)
#define DW1000_FS_PLLTUNE_CH_4	(0x26)
#define DW1000_FS_PLLTUNE_CH_3	(0x56)
#define DW1000_FS_PLLTUNE_CH_5	(0xBE)
#define DW1000_FS_PLLTUNE_CH_7	(0xBE)


/* 0x2B:0E FS_XTALT
 * Octets		Type		Mnemonic	Description
 * 1			RW			FS_XTALT	Frequency synthesiser. Crystal Trim.
 *
 * reg:28:2C	bit:1		XTALT		Crystal Trim
 * @TODO: This field is inconsistent in the datasheet. 7.2.44.5.
 * @TODO: Section 8.1 */


/* 0x2C Always-on System Control Interface
 * Octets		Type		Mnemonic	Description
 * 12			-			AON			Always on system control interface block
 *
 * OFFSET					Mnemonic	Description
 * 0x00						AON_WCFG	AON Wakeup Configuration Register
 * 0x02						AON_CTRL	AON Control Register
 * 0x03						AON_RDAT	AON Direct Access Read Data Result
 * 0x04						AON_ADDR	AON Direct Access Address
 * 0x05						-			reserved
 * 0x06						AON_CFG0	AON Configuration Register 0
 * 0x0A						AON_CFG1	AON Configuration Register 1 */
#define DW1000_AON_WCFG_OFFSET (0x00)
#define DW1000_AON_CTRL_OFFSET (0x02)
#define DW1000_AON_RDAT_OFFSET (0x03)
#define DW1000_AON_ADDR_OFFSET (0x04)
#define DW1000_AON_CFG0_OFFSET (0x06)
#define DW1000_AON_CFG1_OFFSET (0x0A)


/* 0x2C:00 AON_WCFG
 * Octets		Type		Mnemonic	Description
 * 2			RW			AON_WCFG	AON Wakeup Configuration Register
 *
 * reg:2C:00	bit:0		ONW_RADC	Run temperature and voltage ADC on wake up
 * reg:2C:00	bit:1		ONW_RX		Enable receiver on wake up
 * reg:2C:00	bit:3		ONW_LEUI	Load the EUI from OTP memory into Register file on wake up
 * reg:2C:00	bit:6		ONW_LDC		Load AON memory into host interface register on wake up
 * reg:2C:00	bit:7		ONW_L64P	Load Length 64 receiver operating parameters on wake up
 * reg:2C:00	bit:8		PRES_SLEEP	Preserve Sleep
 * reg:2C:00	bit:11		ONW_LLDE	Load the LDE microcode on wake up
 * reg:2C:00	bit:12		ONW_LLD0	Load the LDOTUNE value from OTP on wake up */
#define DW1000_AON_WCFG_ONW_RADC_SHIFT		(0)
#define DW1000_AON_WCFG_ONW_RX_SHIFT		(1)
#define DW1000_AON_WCFG_ONW_LEUI_SHIFT		(3)
#define DW1000_AON_WCFG_ONW_LDC_SHIFT		(6)
#define DW1000_AON_WCFG_ONW_L64P_SHIFT		(7)
#define DW1000_AON_WCFG_PRES_SLEEP_SHIFT	(8)
#define DW1000_AON_WCFG_ONW_LLDE_SHIFT		(11)
#define DW1000_AON_WCFG_ONW_LLD0_SHIFT		(12)

#define DW1000_AON_WCFG_ONW_RADC_MASK	(0x1 << DW1000_AON_WCFG_ONW_RADC_SHIFT)
#define DW1000_AON_WCFG_ONW_RX_MASK		(0x1 << DW1000_AON_WCFG_ONW_RX_SHIFT)
#define DW1000_AON_WCFG_ONW_LEUI_MASK	(0x1 << DW1000_AON_WCFG_ONW_LEUI_SHIFT)
#define DW1000_AON_WCFG_ONW_LDC_MASK	(0x1 << DW1000_AON_WCFG_ONW_LDC_SHIFT)
#define DW1000_AON_WCFG_ONW_L64P_MASK	(0x1 << DW1000_AON_WCFG_ONW_L64P_SHIFT)
#define DW1000_AON_WCFG_PRES_SLEEP_MASK	(0x1 << DW1000_AON_WCFG_PRES_SLEEP_SHIFT)
#define DW1000_AON_WCFG_ONW_LLDE_MASK	(0x1 << DW1000_AON_WCFG_ONW_LLDE_SHIFT)
#define DW1000_AON_WCFG_ONW_LLD0_MASK	(0x1 << DW1000_AON_WCFG_ONW_LLD0_SHIFT)

#define DW1000_AON_WCFG_ONW_RADC		(0x1 << DW1000_AON_WCFG_ONW_RADC_SHIFT)
#define DW1000_AON_WCFG_ONW_RX			(0x1 << DW1000_AON_WCFG_ONW_RX_SHIFT)
#define DW1000_AON_WCFG_ONW_LEUI		(0x1 << DW1000_AON_WCFG_ONW_LEUI_SHIFT)
#define DW1000_AON_WCFG_ONW_LDC			(0x1 << DW1000_AON_WCFG_ONW_LDC_SHIFT)
#define DW1000_AON_WCFG_ONW_L64P		(0x1 << DW1000_AON_WCFG_ONW_L64P_SHIFT)
#define DW1000_AON_WCFG_PRES_SLEEP		(0x1 << DW1000_AON_WCFG_PRES_SLEEP_SHIFT)
#define DW1000_AON_WCFG_ONW_LLDE		(0x1 << DW1000_AON_WCFG_ONW_LLDE_SHIFT)
#define DW1000_AON_WCFG_ONW_LLD0		(0x1 << DW1000_AON_WCFG_ONW_LLD0_SHIFT)


/* 0x2C:02 AON_CTRL
 * Octets		Type		Mnemonic	Description
 * 1			RW			AON_CTRL	AON Control Register
 *
 * reg:2C:02	bit:0		RESTORE		Copy user configurations from AON memory to host registers
 * reg:2C:02	bit:1		SAVE		Copy user configurations from host registers to AON memory
 * reg:2C:02	bit:2		UPL_CFG		Upload the AON block configurations to the AON
 * reg:2C:02	bit:3		DCA_READ	Direct AON memory access read
 * reg:2C:02	bit:7		DCA_ENAB	Direct AON memory access enable bit */
#define DW1000_AON_CTRL_RESTORE_SHIFT	(0)
#define DW1000_AON_CTRL_SAVE_SHIFT		(1)
#define DW1000_AON_CTRL_UPL_CFG_SHIFT	(2)
#define DW1000_AON_CTRL_DCA_READ_SHIFT	(3)
#define DW1000_AON_CTRL_DCA_ENAB_SHIFT	(7)

#define DW1000_AON_CTRL_RESTORE_MASK	(0x1 << DW1000_AON_CTRL_RESTORE_SHIFT)
#define DW1000_AON_CTRL_SAVE_MASK		(0x1 << DW1000_AON_CTRL_SAVE_SHIFT)
#define DW1000_AON_CTRL_UPL_CFG_MASK	(0x1 << DW1000_AON_CTRL_UPL_CFG_SHIFT)
#define DW1000_AON_CTRL_DCA_READ_MASK	(0x1 << DW1000_AON_CTRL_DCA_READ_SHIFT)
#define DW1000_AON_CTRL_DCA_ENAB_MASK	(0x1 << DW1000_AON_CTRL_DCA_ENAB_SHIFT)

#define DW1000_AON_CTRL_RESTORE 	(0x1 << DW1000_AON_CTRL_RESTORE_SHIFT)
#define DW1000_AON_CTRL_SAVE 		(0x1 << DW1000_AON_CTRL_SAVE_SHIFT)
#define DW1000_AON_CTRL_UPL_CFG 	(0x1 << DW1000_AON_CTRL_UPL_CFG_SHIFT)
#define DW1000_AON_CTRL_DCA_READ 	(0x1 << DW1000_AON_CTRL_DCA_READ_SHIFT)
#define DW1000_AON_CTRL_DCA_ENAB 	(0x1 << DW1000_AON_CTRL_DCA_ENAB_SHIFT)


/* 0x2C:03 AON_RDAT
 * Octets		Type		Mnemonic	Description
 * 1			RW			AON_RDAT	AON Direct Access Read Data Result */

/* 0x2C:04 AON_ADDR
 * Octets		Type		Mnemonic	Description
 * 1			RW			AON_ADDR	AON Direct Access Address */

/* 0x2C:06 AON_CFG0
 * Octets		Type		Mnemonic	Description
 * 4			RW			AON_CFG0	AON Configuration Register 0
 *
 * reg:2C:06	bit:0		SLEEP_EN	Sleep enable configuration bit
 * reg:2C:06	bit:1		WAKE_PIN	Wake using WAKEUP pin
 * reg:2C:06	bit:2		WAKE_SPI	Wake using SPI access
 * reg:2C:06	bit:3		WAKE_CNT	Wake when sleep counter elapses
 * reg:2C:06	bit:4		LPDIV_EN	Low power divider enable configuration
 * reg:2C:06	bits:15–5	LPCLKDIVA	LP clock frequency divider
 * reg:2C:06	bits:31–16	SLEEP_TIM	Sleep time */
#define DW1000_AON_CFG0_SLEEP_EN_SHIFT	(0)
#define DW1000_AON_CFG0_WAKE_PIN_SHIFT	(1)
#define DW1000_AON_CFG0_WAKE_SPI_SHIFT	(2)
#define DW1000_AON_CFG0_WAKE_CNT_SHIFT	(3)
#define DW1000_AON_CFG0_LPDIV_EN_SHIFT	(4)
#define DW1000_AON_CFG0_LPCLKDIVA_SHIFT	(5)
#define DW1000_AON_CFG0_SLEEP_TIM_SHIFT	(16)

#define DW1000_AON_CFG0_SLEEP_EN_MASK	(0x1    << DW1000_AON_CFG0_SLEEP_EN_SHIFT)
#define DW1000_AON_CFG0_WAKE_PIN_MASK	(0x1    << DW1000_AON_CFG0_WAKE_PIN_SHIFT)
#define DW1000_AON_CFG0_WAKE_SPI_MASK	(0x1    << DW1000_AON_CFG0_WAKE_SPI_SHIFT)
#define DW1000_AON_CFG0_WAKE_CNT_MASK	(0x1    << DW1000_AON_CFG0_WAKE_CNT_SHIFT)
#define DW1000_AON_CFG0_LPDIV_EN_MASK	(0x1    << DW1000_AON_CFG0_LPDIV_EN_SHIFT)
#define DW1000_AON_CFG0_LPCLKDIVA_MASK	(0x7FF  << DW1000_AON_CFG0_LPCLKDIVA_SHIFT)
#define DW1000_AON_CFG0_SLEEP_TIM_MASK	(0xFFFF << DW1000_AON_CFG0_SLEEP_TIM_SHIFT)

#define DW1000_AON_CFG0_SLEEP_EN (0x1 << DW1000_AON_CFG0_SLEEP_EN_SHIFT)
#define DW1000_AON_CFG0_WAKE_PIN (0x1 << DW1000_AON_CFG0_WAKE_PIN_SHIFT)
#define DW1000_AON_CFG0_WAKE_SPI (0x1 << DW1000_AON_CFG0_WAKE_SPI_SHIFT)
#define DW1000_AON_CFG0_WAKE_CNT (0x1 << DW1000_AON_CFG0_WAKE_CNT_SHIFT)
#define DW1000_AON_CFG0_LPDIV_EN (0x1 << DW1000_AON_CFG0_LPDIV_EN_SHIFT)


/* 0x2C:0A AON_CFG1
 * Octets		Type		Mnemonic	Description
 * 2			RW			AON_CFG1	AON Configuration Register 1
 *
 * reg:2C:0A	bit:0		SLEEP_CEN	Sleep counter enable
 * reg:2C:0A	bit:1		SMXX		Set to 0 when entering SLEEP state
 * reg:2C:0A	bit:2		LPOSC_CAL	Low powered oscillator calibration enable */
#define DW1000_AON_CFG1_SLEEP_CEN_SHIFT	(0)
#define DW1000_AON_CFG1_SMXX_SHIFT		(1)
#define DW1000_AON_CFG1_LPOSC_CAL_SHIFT	(2)

#define DW1000_AON_CFG1_SLEEP_CEN_MASK	(0x1 << DW1000_AON_CFG1_SLEEP_CEN_SHIFT)
#define DW1000_AON_CFG1_SMXX_MASK		(0x1 << DW1000_AON_CFG1_SMXX_SHIFT)
#define DW1000_AON_CFG1_LPOSC_CAL_MASK	(0x1 << DW1000_AON_CFG1_LPOSC_CAL_SHIFT)

#define DW1000_AON_CFG1_SLEEP_CEN	(0x1 << DW1000_AON_CFG1_SLEEP_CEN_SHIFT)
#define DW1000_AON_CFG1_SMXX		(0x1 << DW1000_AON_CFG1_SMXX_SHIFT)
#define DW1000_AON_CFG1_LPOSC_CAL	(0x1 << DW1000_AON_CFG1_LPOSC_CAL_SHIFT)


/* 0x2D OTP Memory Interface
 * Octets		Type		Mnemonic	Description
 * 18			-			OTP_IF		One Time Programmable Memory Interface
 *
 * OFFSET					Mnemonic	Description
 * 0x00						OTP_WDAT	OTP Write Data
 * 0x04						OTP_ADDR	OTP Address
 * 0x06						OTP_CTRL	OTP Control
 * 0x08						OTP_STAT	OTP Status
 * 0x0A						OTP_RDAT	OTP Read Data
 * 0x0E						OTP_SRDAT	OTP SR Read Data
 * 0x12						OTP_SF		OTP Special Function */
#define DW1000_OTP_WDAT_OFFSET	(0x00)
#define DW1000_OTP_ADDR_OFFSET	(0x04)
#define DW1000_OTP_CTRL_OFFSET	(0x06)
#define DW1000_OTP_STAT_OFFSET	(0x08)
#define DW1000_OTP_RDAT_OFFSET	(0x0A)
#define DW1000_OTP_SRDAT_OFFSET	(0x0E)
#define DW1000_OTP_SF_OFFSET	(0x12)


/* 0x2D:00 OTP_WDAT
 * Octets		Type		Mnemonic	Description
 * 4			RW			OTP_WDAT	OTP Write Data */

/* 0x2D:04 OTP_ADDR
 * Octets		Type		Mnemonic	Description
 * 2			RW			OTP_ADDR	OTP Address
 *
 * reg:2D:04	bits:10–0	OTPADDR	OTP Memory Address */
#define DW1000_OTP_ADDR_SHIFT	(0)

#define DW1000_OTP_ADDR_MASK	(0x7FF << DW1000_OTP_ADDR_SHIFT)


/* 0x2D:06 OTP_CTRL
 * Octets		Type		Mnemonic	Description
 * 2			RW			OTP_CTRL	OTP Control
 *
 * reg:2D:06	bit:0		OTPRDEN		OTP manual read mode
 * reg:2D:06	bit:1		OTPREAD		Read from OTP_ADDR
 * reg:2D:06	bit:3		OTPMRWR		OTP mode register write
 * reg:2D:06	bit:6		OTPPROG		Write OTP_WDAT to OTP_ADDR
 * reg:2D:06	bit:10-7	OTPMR		OTP Mode register
 * reg:2D:06	bit:15		LDELOAD		LDE microcode force load */
#define DW1000_OTP_CTRL_OTPRDEN_SHIFT	(0)
#define DW1000_OTP_CTRL_OTPREAD_SHIFT	(1)
#define DW1000_OTP_CTRL_OTPMRWR_SHIFT	(3)
#define DW1000_OTP_CTRL_OTPPROG_SHIFT	(6)
#define DW1000_OTP_CTRL_OTPMR_SHIFT		(7)
#define DW1000_OTP_CTRL_LDELOAD_SHIFT	(15)

#define DW1000_OTP_CTRL_OTPRDEN_MASK	(0x1 << DW1000_OTP_CTRL_OTPRDEN_SHIFT)
#define DW1000_OTP_CTRL_OTPREAD_MASK	(0x1 << DW1000_OTP_CTRL_OTPREAD_SHIFT)
#define DW1000_OTP_CTRL_OTPMRWR_MASK	(0x1 << DW1000_OTP_CTRL_OTPMRWR_SHIFT)
#define DW1000_OTP_CTRL_OTPPROG_MASK	(0x1 << DW1000_OTP_CTRL_OTPPROG_SHIFT)
#define DW1000_OTP_CTRL_OTPMR_MASK		(0xF << DW1000_OTP_CTRL_OTPMR_SHIFT)
#define DW1000_OTP_CTRL_LDELOAD_MASK	(0x1 << DW1000_OTP_CTRL_LDELOAD_SHIFT)

#define DW1000_OTP_CTRL_OTPRDEN		(0x1 << DW1000_OTP_CTRL_OTPRDEN_SHIFT)
#define DW1000_OTP_CTRL_OTPREAD		(0x1 << DW1000_OTP_CTRL_OTPREAD_SHIFT)
#define DW1000_OTP_CTRL_OTPMRWR		(0x1 << DW1000_OTP_CTRL_OTPMRWR_SHIFT)
#define DW1000_OTP_CTRL_OTPPROG		(0x1 << DW1000_OTP_CTRL_OTPPROG_SHIFT)
#define DW1000_OTP_CTRL_OTPMR		(0x1 << DW1000_OTP_CTRL_OTPMR_SHIFT)
#define DW1000_OTP_CTRL_LDELOAD		(0x1 << DW1000_OTP_CTRL_LDELOAD_SHIFT)


/* 0x2D:08 OTP_STAT
 * Octets		Type		Mnemonic	Description
 * 2			RW			OTP_STAT	OTP Status
 *
 * reg:2D:04	bit:0		OTPPRGD		OTP Programming Done
 * reg:2D:04	bit:1		OTPVPOK		OTP Programming Voltage OK */
#define DW1000_OTP_STAT_OTPPRGD_SHIFT	(0)
#define DW1000_OTP_STAT_OTPVPOK_SHIFT	(1)

#define DW1000_OTP_STAT_OTPPRGD_MASK	(0x1 << DW1000_OTP_STAT_OTPPRGD_SHIFT)
#define DW1000_OTP_STAT_OTPVPOK_MASK	(0x1 << DW1000_OTP_STAT_OTPVPOK_SHIFT)

#define DW1000_OTP_STAT_OTPPRGD	(0x1 << DW1000_OTP_STAT_OTPPRGD_SHIFT)
#define DW1000_OTP_STAT_OTPVPOK	(0x1 << DW1000_OTP_STAT_OTPVPOK_SHIFT)


/* 0x2D:0A OTP RDAT
 * Octets		Type		Mnemonic	Description
 * 4			R			OTP_RDAT	OTP Read Data */

/* 0x2D:0E OTP_SRDAT
 * Octets		Type		Mnemonic	Description
 * 4			RW			OTP_SRDAT	OTP Special Register Read Data */

/* 0x2D:12 OTP_SF
 * Octets		Type		Mnemonic	Description
 * 1			RW			OTP_SF		OTP Special Function
 *
 * reg:2D:12	bit:0		OPS_KICK	Load operating parameters selected by OPS_SEL
 * reg:2D:12	bit:1		LDO_KICK	Load LDOTUNE_CAL from OTP address 0x4.
 * reg:2D:12	bits:6-5	OPS_SEL		Operating parameter set selection */
#define DW1000_OTP_SF_OPS_KICK_SHIFT	(0)
#define DW1000_OTP_SF_LDO_KICK_SHIFT	(1)
#define DW1000_OTP_SF_OPS_SEL_SHIFT		(5)

#define DW1000_OTP_SF_OPS_KICK_MASK		(0x1 << DW1000_OTP_SF_OPS_KICK_SHIFT)
#define DW1000_OTP_SF_LDO_KICK_MASK		(0x1 << DW1000_OTP_SF_LDO_KICK_SHIFT)
#define DW1000_OTP_SF_OPS_SEL_MASK		(0x3 << DW1000_OTP_SF_OPS_SEL_SHIFT)

#define DW1000_OTP_SF_OPS_KICK 			(0x1 << DW1000_OTP_SF_OPS_KICK_SHIFT)
#define DW1000_OTP_SF_LDO_KICK 			(0x1 << DW1000_OTP_SF_LDO_KICK_SHIFT)
#define DW1000_OTP_SF_OPS_SEL_L64 		(0x0 << DW1000_OTP_SF_OPS_SEL_SHIFT)
#define DW1000_OTP_SF_OPS_SEL_TIGHT		(0x1 << DW1000_OTP_SF_OPS_SEL_SHIFT)
#define DW1000_OTP_SF_OSP_SEL_DEFAULT	(0x2 << DW1000_OTP_SF_OPS_SEL_SHIFT)


/* 0x2E Leading Edge Detection Interface
 * Octets		Type		Mnemonic	Description
 * -			-			LDE_IF		Leading Edge Detection Interface
 *
 * OFFSET					Mnemonic	Description
 * 0x0000					LDE_THRESH	LDE Threshold report
 * 0x0806					LDE_CFG1	LDE Configuration Register 1
 * 0x1000					LDE_PPINDX	LDE Peak Path Index
 * 0x1002					LDE_PPAMPL	LDE Peak Path Amplitude
 * 0x1804					LDE_RXANTD	LDE Receive Antenna Delay configuration
 * 0x1806					LDE_CFG2	LDE Configuration Register 2
 * 0x2804					LDE_REPC	LDE Replica Coefficient configuration */
#define DW1000_LDE_THRESH_OFFSET	(0x0000)
#define DW1000_LDE_CFG1_OFFSET		(0x0806)
#define DW1000_LDE_PPINDX_OFFSET	(0x1000)
#define DW1000_LDE_PPAMPL_OFFSET	(0x1002)
#define DW1000_LDE_RXANTD_OFFSET	(0x1804)
#define DW1000_LDE_CFG2_OFFSET		(0x1806)
#define DW1000_LDE_REPC_OFFSET		(0x2804)


/* 0x2E:0000 LDE_THRESH
 * Octets		Type		Mnemonic	Description
 * 2			RO			LDE_THRESH	LDE Threshold report */

/* 0x2E:0806 LDE_CFG1
 * Octets		Type		Mnemonic	Description
 * 1			RW			LDE_CFG1	LDE Configuration Register 1
 *
 * reg:2E:0806	bits:4–0	NTM			Noise Threshold Multiplier
 * reg:2E:0806	bits:7–5	PMULT		Peak Multiplier */
#define DW1000_LDE_CFG1_NTM_SHIFT	(0)
#define DW1000_LDE_CFG1_PMULT_SHIFT	(5)

#define DW1000_LDE_CFG1_NTM_MASK	(0x1F << DW1000_LDE_CFG1_NTM_SHIFT)
#define DW1000_LDE_CFG1_PMULT_MASK	(0x7  << DW1000_LDE_CFG1_PMULT_SHIFT)


/* 0x2E:1000 LDE_PPINDX
 * Octets		Type		Mnemonic	Description
 * 2			RO			LDE_PPINDX	LDE Peak Path Index */

/* 0x2E:1002 LDE_PPAMPL
 * Octets		Type		Mnemonic	Description
 * 2			RO			LDE_PPAMPL	LDE Peak Path Amplitude */

/* 0x2E:1804 LDE_RXANTD
 * Octets		Type		Mnemonic	Description
 * 2			RW			LDE_RXANTD	LDE Receive Antenna Delay configuration */

/* 0x2E:1806 LDE_CFG2
 * Octets		Type		Mnemonic	Description
 * 2			RW			LDE_CFG2	LDE Configuration Register 2
 *
 * RXPRF			LDE_CFG2
 * (1) = 16 MHz PRF	0x1607
 * (2) = 64 MHz PRF	0x0607 */
#define DW1000_LDE_CFG2_16_MHZ_PRF	(0x1607)
#define DW1000_LDE_CFG2_64_MHZ_PRF	(0x0607)


/* 0x2E:2804 LDE_REPC
 * Octets		Type		Mnemonic	Description
 * 2			RW			LDE_REPC	LDE Replica Coefficient configuration
 *
 * RX_PCODE		LDE_REPC
 * 1			0x5998
 * 2			0x5998
 * 3			0x51EA
 * 4			0x428E
 * 5			0x451E
 * 6			0x2E14
 * 7			0x8000
 * 8			0x51EA
 * 9			0x28F4
 * 10			0x3332
 * 11			0x3AE0
 * 12			0x3D70
 * 13			0x3AE0
 * 14			0x35C2
 * 15			0x2B84
 * 16			0x35C2
 * 17			0x3332
 * 18			0x35C2
 * 19			0x35C2
 * 21			0x3AE0
 * 20			0x47AE
 * 22			0x3850
 * 23			0x30A2
 * 24			0x3850 */
#define DW1000_LDE_REPC_RX_PCODE_1	(0x5998)
#define DW1000_LDE_REPC_RX_PCODE_2	(0x5998)
#define DW1000_LDE_REPC_RX_PCODE_3	(0x51EA)
#define DW1000_LDE_REPC_RX_PCODE_4	(0x428E)
#define DW1000_LDE_REPC_RX_PCODE_5	(0x451E)
#define DW1000_LDE_REPC_RX_PCODE_6	(0x2E14)
#define DW1000_LDE_REPC_RX_PCODE_7	(0x8000)
#define DW1000_LDE_REPC_RX_PCODE_8	(0x51EA)
#define DW1000_LDE_REPC_RX_PCODE_9	(0x28F4)
#define DW1000_LDE_REPC_RX_PCODE_10	(0x3332)
#define DW1000_LDE_REPC_RX_PCODE_11	(0x3AE0)
#define DW1000_LDE_REPC_RX_PCODE_12	(0x3D70)
#define DW1000_LDE_REPC_RX_PCODE_13	(0x3AE0)
#define DW1000_LDE_REPC_RX_PCODE_14	(0x35C2)
#define DW1000_LDE_REPC_RX_PCODE_15	(0x2B84)
#define DW1000_LDE_REPC_RX_PCODE_16	(0x35C2)
#define DW1000_LDE_REPC_RX_PCODE_17	(0x3332)
#define DW1000_LDE_REPC_RX_PCODE_18	(0x35C2)
#define DW1000_LDE_REPC_RX_PCODE_19	(0x35C2)
#define DW1000_LDE_REPC_RX_PCODE_21	(0x3AE0)
#define DW1000_LDE_REPC_RX_PCODE_20	(0x47AE)
#define DW1000_LDE_REPC_RX_PCODE_22	(0x3850)
#define DW1000_LDE_REPC_RX_PCODE_23	(0x30A2)
#define DW1000_LDE_REPC_RX_PCODE_24	(0x3850)


/* 0x2F Digital Diagnostics Interface
 * Octets		Type		Mnemonic	Description
 * 41			-			DIG_DIAG	Digital Diagnostics Interface
 *
 * OFFSET					Mnemonic	Description
 * 0x00						EVC_CTRL	Event Counter Control
 * 0x04						EVC_PHE		PHR Error Counter
 * 0x06						EVC_RSE		RSD Error Counter
 * 0x08						EVC_FCG		Frame Check Sequence Good Counter
 * 0x0A						EVC_FCE		Frame Check Sequence Error Counter
 * 0x0C						EVC_FFR		Frame Filter Rejection Counter
 * 0x0E						EVC_OVR		RX Overrun Error Counter
 * 0x10						EVC_STO		SFD Timeout Counter
 * 0x12						EVC_PTO		Preamble Timeout Counter
 * 0x14						EVC_FWTO	RX Frame Wait Timeout Counter
 * 0x16						EVC_TXFS	TX Frame Sent Counter
 * 0x18						EVC_HPW		Half Period Warning Counter
 * 0x1A						EVC_TPW		Transmitter Power-Up Warning Counter
 * 0x1C						EVC_RES1	Digital Diagnostics Reserved Area 1
 * 0x24						DIAG_TMC	Test Mode Control Register */
#define DW1000_EVC_CTRL_OFFSET	(0x00)
#define DW1000_EVC_PHE_OFFSET	(0x04)
#define DW1000_EVC_RSE_OFFSET	(0x06)
#define DW1000_EVC_FCG_OFFSET	(0x08)
#define DW1000_EVC_FCE_OFFSET	(0x0A)
#define DW1000_EVC_FFR_OFFSET	(0x0C)
#define DW1000_EVC_OVR_OFFSET	(0x0E)
#define DW1000_EVC_STO_OFFSET	(0x10)
#define DW1000_EVC_PTO_OFFSET	(0x12)
#define DW1000_EVC_FWTO_OFFSET	(0x14)
#define DW1000_EVC_TXFS_OFFSET	(0x16)
#define DW1000_EVC_HPW_OFFSET	(0x18)
#define DW1000_EVC_TPW_OFFSET	(0x1A)
#define DW1000_EVC_RES1_OFFSET	(0x1C)
#define DW1000_DIAG_TMC_OFFSET	(0x24)


/* 0x2F:00 Event Counter Control
 * Octets		Type		Mnemonic	Description
 * 4			SRW			EVC_CTRL	Event Counter Control
 *
 * reg:2F:00	bit:0		EVC_EN		Event Counters Enable
 * reg:2F:00	bit:1		EVC_CLR		Event Counters Clear */
#define DW1000_EVC_EN_SHIFT		(0)
#define DW1000_EVC_CLR_SHIFT	(1)

#define DW1000_EVC_EN_MASK		(0x1 << DW1000_EVC_EN_SHIFT)
#define DW1000_EVC_CLR_MASK		(0x1 << DW1000_EVC_CLR_SHIFT)

#define DW1000_EVC_EN	(0x1 << DW1000_EVC_EN_SHIFT)
#define DW1000_EVC_CLR	(0x1 << DW1000_EVC_CLR_SHIFT)


/* 0x2F:04 PHR Error Counter
 * Octets		Type		Mnemonic	Description
 * 2			RO			EVC_PHE		PHR Error Event Counter
 *
 * reg:2F:04	bits:11–0	EVC_PHE		PHR Error Event Counter */
#define DW1000_EVC_PHE_SHIFT	(0)

#define DW1000_EVC_PHE_MASK		(0xFFF << DW1000_EVC_PHE_SHIFT)


/* 0x2F:06 RSD Error Counter
 * Octets		Type		Mnemonic	Description
 * 2			RO			EVC_RSE		RSD Error Event Counter
 *
 * reg:2F:06	bits:11–0	EVC_RSE		Reed Solomon decoder (Frame Sync Loss) Error Event Counter */
#define DW1000_EVC_RSE_SHIFT	(0)

#define DW1000_EVC_RSE_MASK		(0xFFF << DW1000_EVC_RSE_SHIFT)


/* 0x2F:08 FCS Good Counter
 * Octets		Type		Mnemonic	Description
 * 2			RO			EVC_FCG		Frame Check Sequence Good Event Counter
 *
 * reg:2F:08	bits:11–0	EVC_FCG		Frame Check Sequence Good Event Counter */
#define DW1000_EVC_FCG_SHIFT	(0)

#define DW1000_EVC_FCG_MASK		(0xFFF << DW1000_EVC_FCG_SHIFT)


/* 0x2F:0A FCS Error Counter
 * Octets		Type		Mnemonic	Description
 * 2			RO			EVC_FCE		Frame Check Sequence Error Counter
 *
 * reg:2F:0A	bits:11–0	EVC_FCE		Frame Check Sequence Error Event Counter */
#define DW1000_EVC_FCE_SHIFT	(0)

#define DW1000_EVC_FCE_MASK		(0xFFF << DW1000_EVC_FCE_SHIFT)


/* 0x2F:0C Frame Filter Rejection Counter
 * Octets		Type		Mnemonic	Description
 * 2			RO			EVC_FFR		Frame Filter Rejection Counter
 *
 * reg:2F:0C	bits:11–0	EVC_FFR		Frame Filter Rejection Event Counter */
#define DW1000_EVC_FFR_SHIFT	(0)

#define DW1000_EVC_FFR_MASK		(0xFFF << DW1000_EVC_FFR_SHIFT)


/* 0x2F:0E RX Overrun Error Counter
 * Octets		Type		Mnemonic	Description
 * 2			RO			EVC_OVR		RX Overrun Error Counter
 *
 * reg:2F:0E	bits:11–0	EVC_OVR		RX Overrun Error Event Counter */
#define DW1000_EVC_OVR_SHIFT	(0)

#define DW1000_EVC_OVR_MASK		(0xFFF << DW1000_EVC_OVR_SHIFT)


/* 0x2F:10 SFD Timeout Error Counter
 * Octets		Type		Mnemonic	Description
 * 2			RO			EVC_STO		SFD Timeout Error Counter
 *
 * reg:2F:10	bits:11–0	EVC_STO		SFD timeout errors Event Counter */
#define DW1000_EVC_STO_SHIFT	(0)

#define DW1000_EVC_STO_MASK		(0xFFF << DW1000_EVC_STO_SHIFT)


/* 0x2F:12 Preamble Detection Timeout Event Counter
 * Octets		Type		Mnemonic	Description
 * 2			RO			EVC_PTO		Preamble Detection Timeout Event Counter
 *
 * reg:2F:12	bits:11–0	EVC_PTO		Preamble Detection Timeout Event Counter */
#define DW1000_EVC_PTO_SHIFT	(0)

#define DW1000_EVC_PTO_MASK		(0xFFF << DW1000_EVC_PTO_SHIFT)


/* 0x2F:14 RX Frame Wait Timeout Event Counter
 * Octets		Type		Mnemonic	Description
 * 2			RO			EVC_FWTO	RX Frame Wait Timeout Counter
 *
 * reg:2F:14	bits:11–0	EVC_FWTO	RX Frame Wait Timeout Event Counter */
#define DW1000_EVC_FWTO_SHIFT	(0)

#define DW1000_EVC_FWTO_MASK	(0xFFF << DW1000_EVC_FWTO_SHIFT)


/* 0x2F:16 TX Frame Sent Counter
 * Octets		Type		Mnemonic	Description
 * 2			RO			EVC_TXFS	TX Frame Sent Coutner
 *
 * reg:2F:16	bits:11–0	EVC_TXFS	TX Frame Sent Event Counter */
#define DW1000_EVC_TXFS_SHIFT	(0)

#define DW1000_EVC_TXFS_MASK	(0xFFF << DW1000_EVC_TXFS_SHIFT)


/* 0x2F:18 Half Period Warning Counter
 * Octets		Type		Mnemonic	Description
 * 2			RO			EVC_HPW		Half Period Warning Counter
 *
 * reg:2F:18	bits:11–0	EVC_HPW		Half Period Warning Event Counter */
#define DW1000_EVC_HPW_SHIFT	(0)

#define DW1000_EVC_HPW_MASK		(0xFFF << DW1000_EVC_HPW_SHIFT)


/* 0x2F:1A Transmitter Power-Up Warning Counter
 * Octets		Type		Mnemonic	Description
 * 2			RO			EVC_TPW		Transmitter Power-Up Warning Counter
 *
 * reg:2F:1A	bits:11–0	EVC_TPW		TX Power-Up Warning Event Counter */
#define DW1000_EVC_TPW_SHIFT	(0)

#define DW1000_EVC_TPW_MASK		(0xFFF << DW1000_EVC_TPW_SHIFT)


/* 0x2F:24 Digital Diagnostics Test Mode Control
 * Octets		Type		Mnemonic	Description
 * 2			RW			DIAG_TMC	Test Mode Control Register
 *
 * reg:2F:24	bit:4		TX_PSTM		Transmit Power Spectrum Test Mode */
#define DW1000_DIAG_TMC_TX_PSTM_SHIFT	(4)

#define DW1000_DIAG_TMC_TX_PSTM_MASK	(0x1 << DW1000_DIAG_TMC_TX_PSTM_SHIFT)

#define DW1000_DIAG_TMC_TX_PSTM	(0x1 << DW1000_DIAG_TMC_TX_PSTM_SHIFT)


/* 0x36 Power Management and System Control
 * Octets		Type		Mnemonic	Description
 * 48			-			PMSC		Power Management System Control Block
 *
 * OFFSET					Mnemonic	Description
 * 0x00						PMSC_CTRL0	PMSC Control Register 0
 * 0x04						PMSC_CTRL1	PMSC Control Register 1
 * 0x08						PMSC_RES1	PMSC reserved area 1
 * 0x0C						PMSC_SNOZT	PMSC Snooze Time Register
 * 0x10						PMSC_RES2	PMSC reserved area 2
 * 0x26						PMSC_TXFSEQ	PMSC fine grain TX sequencing control
 * 0x28						PMSC_LEDC	PMSC LED Control Register */
#define DW1000_PMSC_CTRL0_OFFSET	(0x00)
#define DW1000_PMSC_CTRL1_OFFSET	(0x04)
#define DW1000_PMSC_RES1_OFFSET		(0x08)
#define DW1000_PMSC_SNOZT_OFFSET	(0x0C)
#define DW1000_PMSC_RES2_OFFSET		(0x10)
#define DW1000_PMSC_TXFSEQ_OFFSET	(0x26)
#define DW1000_PMSC_LEDC_OFFSET		(0x28)


/* 0x36:00 PMSC_CTRL0
 * Octets		Type		Mnemonic	Description
 * 4			RW			PMSC_CTRL0	PMSC Control Register 0
 *
 * reg:36:00	bits:1-0	SYSCLKS		System Clock Selection
 * reg:36:00	bits:3-2	RXCLKS		Receiver Clock Selection
 * reg:36:00	bits:5-4	TXCLKS		Transmitter Clock Selection
 * reg:36:00	bit:6		FACE		Force Accumulator Clock Enable
 * reg:36:00	bit:10		ADCCE		Temp and voltage Analog-to-Digital Convertor Clock Enable
 * reg:36:00	bit:15		AMCE		Accumulator Memory Clock Enable
 * reg:36:00	bit:16		GPCE		GPIO clock Enable
 * reg:36:00	bit:17		GPRN		GPIO reset, active low
 * reg:36:00	bit:18		GPDCE		GPIO De-bounce Clock Enable
 * reg:36:00	bit:19		GPDRN		GPIO de-bounce reset active low
 * reg:36:00	bit:23		KHZCLKEN	Kilohertz clock Enable
 * reg:36:00	bit:24		PLL2_SEQ_EN	TX sequencing control / RX SNIFF mode control
 * reg:36:00	bits:31-28	SOFTRESET	Reset IC */
#define DW1000_PMSC_CTRL0_SYSCLKS_SHIFT		(0)
#define DW1000_PMSC_CTRL0_RXCLKS_SHIFT		(2)
#define DW1000_PMSC_CTRL0_TXCLKS_SHIFT		(4)
#define DW1000_PMSC_CTRL0_FACE_SHIFT		(6)
#define DW1000_PMSC_CTRL0_ADCCE_SHIFT		(10)
#define DW1000_PMSC_CTRL0_AMCE_SHIFT		(15)
#define DW1000_PMSC_CTRL0_GPCE_SHIFT		(16)
#define DW1000_PMSC_CTRL0_GPRN_SHIFT		(17)
#define DW1000_PMSC_CTRL0_GPDCE_SHIFT		(18)
#define DW1000_PMSC_CTRL0_GPDRN_SHIFT		(19)
#define DW1000_PMSC_CTRL0_KHZCLKEN_SHIFT	(23)
#define DW1000_PMSC_CTRL0_PLL2_SEQ_EN_SHIFT	(24)
#define DW1000_PMSC_CTRL0_SOFTRESET_SHIFT	(28)

#define DW1000_PMSC_CTRL0_SYSCLKS_MASK		(0x3 << DW1000_PMSC_CTRL0_SYSCLKS_SHIFT)
#define DW1000_PMSC_CTRL0_RXCLKS_MASK		(0x3 << DW1000_PMSC_CTRL0_RXCLKS_SHIFT)
#define DW1000_PMSC_CTRL0_TXCLKS_MASK		(0x3 << DW1000_PMSC_CTRL0_TXCLKS_SHIFT)
#define DW1000_PMSC_CTRL0_FACE_MASK			(0x1 << DW1000_PMSC_CTRL0_FACE_SHIFT)
#define DW1000_PMSC_CTRL0_ADCCE_MASK		(0x1 << DW1000_PMSC_CTRL0_ADCCE_SHIFT)
#define DW1000_PMSC_CTRL0_AMCE_MASK			(0x1 << DW1000_PMSC_CTRL0_AMCE_SHIFT)
#define DW1000_PMSC_CTRL0_GPCE_MASK			(0x1 << DW1000_PMSC_CTRL0_GPCE_SHIFT)
#define DW1000_PMSC_CTRL0_GPRN_MASK			(0x1 << DW1000_PMSC_CTRL0_GPRN_SHIFT)
#define DW1000_PMSC_CTRL0_GPDCE_MASK		(0x1 << DW1000_PMSC_CTRL0_GPDCE_SHIFT)
#define DW1000_PMSC_CTRL0_GPDRN_MASK		(0x1 << DW1000_PMSC_CTRL0_GPDRN_SHIFT)
#define DW1000_PMSC_CTRL0_KHZCLKEN_MASK		(0x1 << DW1000_PMSC_CTRL0_KHZCLKEN_SHIFT)
#define DW1000_PMSC_CTRL0_PLL2_SEQ_EN_MASK	(0x1 << DW1000_PMSC_CTRL0_PLL2_SEQ_EN_SHIFT)
#define DW1000_PMSC_CTRL0_SOFTRESET_MASK	(0xF << DW1000_PMSC_CTRL0_SOFTRESET_SHIFT)

#define DW1000_PMSC_CTRL0_SYSCLKS_AUTO		(0x0 << DW1000_PMSC_CTRL0_SYSCLKS_SHIFT)
#define DW1000_PMSC_CTRL0_SYSCLKS_19_2_MHZ	(0x1 << DW1000_PMSC_CTRL0_SYSCLKS_SHIFT)
#define DW1000_PMSC_CTRL0_SYSCLKS_125_MHZ	(0x2 << DW1000_PMSC_CTRL0_SYSCLKS_SHIFT)
#define DW1000_PMSC_CTRL0_RXCLKS_AUTO		(0x0 << DW1000_PMSC_CTRL0_RXCLKS_SHIFT)
#define DW1000_PMSC_CTRL0_RXCLKS_19_2_MHZ	(0x1 << DW1000_PMSC_CTRL0_RXCLKS_SHIFT)
#define DW1000_PMSC_CTRL0_RXCLKS_125_MHZ	(0x2 << DW1000_PMSC_CTRL0_RXCLKS_SHIFT)
#define DW1000_PMSC_CTRL0_TXCLKS_AUTO		(0x0 << DW1000_PMSC_CTRL0_TXCLKS_SHIFT)
#define DW1000_PMSC_CTRL0_TXCLKS_19_2_MHZ	(0x1 << DW1000_PMSC_CTRL0_TXCLKS_SHIFT)
#define DW1000_PMSC_CTRL0_TXCLKS_125_MHZ	(0x2 << DW1000_PMSC_CTRL0_TXCLKS_SHIFT)
#define DW1000_PMSC_CTRL0_FACE				(0x1 << DW1000_PMSC_CTRL0_FACE_SHIFT)
#define DW1000_PMSC_CTRL0_ADCCE				(0x1 << DW1000_PMSC_CTRL0_ADCCE_SHIFT)
#define DW1000_PMSC_CTRL0_AMCE				(0x1 << DW1000_PMSC_CTRL0_AMCE_SHIFT)
#define DW1000_PMSC_CTRL0_GPCE				(0x1 << DW1000_PMSC_CTRL0_GPCE_SHIFT)
#define DW1000_PMSC_CTRL0_GPRN				(0x1 << DW1000_PMSC_CTRL0_GPRN_SHIFT)
#define DW1000_PMSC_CTRL0_GPDCE				(0x1 << DW1000_PMSC_CTRL0_GPDCE_SHIFT)
#define DW1000_PMSC_CTRL0_GPDRN				(0x1 << DW1000_PMSC_CTRL0_GPDRN_SHIFT)
#define DW1000_PMSC_CTRL0_KHZCLKEN			(0x1 << DW1000_PMSC_CTRL0_KHZCLKEN_SHIFT)
#define DW1000_PMSC_CTRL0_PLL2_SEQ_EN		(0x1 << DW1000_PMSC_CTRL0_PLL2_SEQ_EN_SHIFT)
#define DW1000_PMSC_CTRL0_SOFTRESET_CLR		(0x0 << DW1000_PMSC_CTRL0_SOFTRESET_SHIFT)
#define DW1000_PMSC_CTRL0_SOFTRESET			(0xF << DW1000_PMSC_CTRL0_SOFTRESET_SHIFT)


/* 0x36:04 PMSC_CTRL1
 * Octets		Type		Mnemonic	Description
 * 4			RW			PMSC_CTRL1	PMSC Control Register 1
 *
 * reg:36:04	bit:1		ARX2INIT	Automatic transition from receive mode into the INIT state
 * reg:36:04	bits:10-3	PKTSEQ		Enable PMSC control (0xE7 should be written)
 * reg:36:04	bit:11		ATXSLP		After TX automatically Sleep
 * reg:36:04	bit:12		ARXSLP		After RX automatically Sleep
 * reg:36:04	bit:13		SNOZE		Snooze Enable
 * reg:36:04	bit:14		SNOZR		Snooze Repeat
 * reg:36:04	bit:15		PLLSYN		Enable 1 GHz clock
 * reg:36:04	bit:17		LDERUNE		LDE run enable
 * reg:36:04	bits:31–26	KHZCLKDIV	Kilohertz clock divisor */
#define DW1000_PMSC_CTRL1_ARX2INIT_SHIFT	(1)
#define DW1000_PMSC_CTRL1_PKTSEQ_SHIFT		(3)
#define DW1000_PMSC_CTRL1_ATXSLP_SHIFT		(11)
#define DW1000_PMSC_CTRL1_ARXSLP_SHIFT		(12)
#define DW1000_PMSC_CTRL1_SNOZE_SHIFT		(13)
#define DW1000_PMSC_CTRL1_SNOZR_SHIFT		(14)
#define DW1000_PMSC_CTRL1_PLLSYN_SHIFT		(15)
#define DW1000_PMSC_CTRL1_LDERUNE_SHIFT		(17)
#define DW1000_PMSC_CTRL1_KHZCLKDIV_SHIFT	(26)

#define DW1000_PMSC_CTRL1_ARX2INIT_MASK		(0x1  << DW1000_PMSC_CTRL1_ARX2INIT_SHIFT)
#define DW1000_PMSC_CTRL1_PKTSEQ_MASK		(0xFF << DW1000_PMSC_CTRL1_PKTSEQ_SHIFT)
#define DW1000_PMSC_CTRL1_ATXSLP_MASK		(0x1  << DW1000_PMSC_CTRL1_ATXSLP_SHIFT)
#define DW1000_PMSC_CTRL1_ARXSLP_MASK		(0x1  << DW1000_PMSC_CTRL1_ARXSLP_SHIFT)
#define DW1000_PMSC_CTRL1_SNOZE_MASK		(0x1  << DW1000_PMSC_CTRL1_SNOZE_SHIFT)
#define DW1000_PMSC_CTRL1_SNOZR_MASK		(0x1  << DW1000_PMSC_CTRL1_SNOZR_SHIFT)
#define DW1000_PMSC_CTRL1_PLLSYN_MASK		(0x1  << DW1000_PMSC_CTRL1_PLLSYN_SHIFT)
#define DW1000_PMSC_CTRL1_LDERUNE_MASK		(0x1  << DW1000_PMSC_CTRL1_LDERUNE_SHIFT)
#define DW1000_PMSC_CTRL1_KHZCLKDIV_MASK	(0x3F << DW1000_PMSC_CTRL1_KHZCLKDIV_SHIFT)

#define DW1000_PMSC_CTRL1_ARX2INIT	(0x1  << DW1000_PMSC_CTRL1_ARX2INIT_SHIFT)
#define DW1000_PMSC_CTRL1_PKTSEQ	(0xE7 << DW1000_PMSC_CTRL1_PKTSEQ_SHIFT)
#define DW1000_PMSC_CTRL1_ATXSLP 	(0x1  << DW1000_PMSC_CTRL1_ATXSLP_SHIFT)
#define DW1000_PMSC_CTRL1_ARXSLP 	(0x1  << DW1000_PMSC_CTRL1_ARXSLP_SHIFT)
#define DW1000_PMSC_CTRL1_SNOZE 	(0x1  << DW1000_PMSC_CTRL1_SNOZE_SHIFT)
#define DW1000_PMSC_CTRL1_SNOZR 	(0x1  << DW1000_PMSC_CTRL1_SNOZR_SHIFT)
#define DW1000_PMSC_CTRL1_PLLSYN 	(0x1  << DW1000_PMSC_CTRL1_PLLSYN_SHIFT)
#define DW1000_PMSC_CTRL1_LDERUNE 	(0x1  << DW1000_PMSC_CTRL1_LDERUNE_SHIFT)


/* 0x36:0C PMSC_SNOZT
 * Octets		Type		Mnemonic	Description
 * 1			RW			PMSC_SNOZT	PMSC Snooze Time Register
 *
 * reg:36:0C	bits:7–0	SNOZ_TIM	Snooze Time Period */
#define DW1000_PMSC_SNOZT_TIM_SHIFT	(0)

#define DW1000_PMSC_SNOZT_TIM_MASK	(0xFF << DW1000_PMSC_SNOZT_TIM_SHIFT)


/* 0x36:26 PMSC_TXFSEQ
 * Octets		Type		Mnemonic	Description
 * 2			RW			PMSC_TXFSEQ	PMSC fine grain TX sequencing control
 *
 * reg:36:26	bits:15–0	TXFINESEQ	Enable fine grain power sequencing (0x0B74) */
#define DW1000_PMSC_TXFINESEQ_SHIFT	(0)

#define DW1000_PMSC_TXFINESEQ_MASK	(0xFFFF << DW1000_PMSC_TXFINESEQ_SHIFT)

#define DW1000_PMSC_TXFINESEQ_DISABLE (0x0000 << DW1000_PMSC_TXFINESEQ_SHIFT)
#define DW1000_PMSC_TXFINESEQ_ENABLE  (0x0B74 << DW1000_PMSC_TXFINESEQ_SHIFT)


/* 0x36:28 PMSC_LEDC
 * Octets		Type		Mnemonic	Description
 * 4			RW			PMSC_LEDC	PMSC LED Control Register
 *
 * reg:36:28	bits:7–0	BLINK_TIM	Blink time count value
 * reg:36:28	bit:8		BLNKEN		Blink Enable
 * reg:36:28	bits:19-16	BLNKNOW		Manually triggers an LED blink */
#define DW1000_PMSC_LEDC_BLINK_TIM_SHIFT	(0)
#define DW1000_PMSC_LEDC_BLNKEN_SHIFT		(8)
#define DW1000_PMSC_LEDC_BLNKNOW_SHIFT		(16)

#define DW1000_PMSC_LEDC_BLINK_TIM_MASK		(0xFF << DW1000_PMSC_LEDC_BLINK_TIM_SHIFT)
#define DW1000_PMSC_LEDC_BLNKEN_MASK		(0x1  << DW1000_PMSC_LEDC_BLNKEN_SHIFT)
#define DW1000_PMSC_LEDC_BLNKNOW_MASK		(0xF  << DW1000_PMSC_LEDC_BLNKNOW_SHIFT)

#define DW1000_PMSC_LEDC_BLNKEN	(0x1 << DW1000_PMSC_LEDC_BLNKEN_SHIFT)


#define DW1000_TIME_RES		(1.0 / 499.2E6 / 128.0)	/* 15.65 ps */


typedef enum {
	DW1000_PRF_16MHZ = 0,
	DW1000_PRF_64MHZ = 1,
} DW1000_Prf;


typedef enum {
	DW1000_PAC_SIZE_8  = 0,
	DW1000_PAC_SIZE_16 = 1,
	DW1000_PAC_SIZE_32 = 2,
	DW1000_PAC_SIZE_64 = 3,
} DW1000_Pac;


typedef enum {
	DW1000_PLEN_64   = 0x1,
	DW1000_PLEN_1024 = 0x2,
	DW1000_PLEN_4096 = 0x3,
} DW1000_Pre_Length;


typedef enum {
	DW1000_DR_110KBPS  = 0,
	DW1000_DR_850KBPS  = 1,
	DW1000_DR_6800KBPS = 2,
} DW1000_Data_Rate;


typedef enum {
	DW1000_TX_SUCCESS = 1,
	DW1000_RX_SUCCESS = 2,
} DW1000_Status;


typedef struct {
	unsigned          channel;			/* Channel Number 1, 2, 3, 4, 5, 7 */
	DW1000_Data_Rate  data_rate;
	DW1000_Prf        prf;
	DW1000_Pac        pac;
	DW1000_Pre_Length preamble_length;
	unsigned          tx_code;			/* TX preamble code */
	unsigned          rx_code;			/* RX preamble code */
	uint16_t          sfd_timeout;
} DW1000_Config;


typedef struct {
	// const struct device* spi;
	// struct spi_cs_control spi_cs_ctrl;
	// struct spi_config spi_cfg;
	// struct k_sem irq_sem;
	// // atomic_t irq_flag;
	// const struct device* gpio_dev;
	// uint32_t gpio_int_pin;
	// struct gpio_callback irq_cb;
	// // struct k_sem trx_sem;

	uint32_t gpio_int_pin;

	/* Configuration */
	uint8_t  channel;			/* 1, 2, 3, 4, 5, 7 */
	uint8_t  data_rate;			/* DW1000_Data_Rate */
	uint8_t  prf;				/* DW1000_Prf */
	uint8_t  pac;				/* DW1000_Pac */
	uint8_t  preamble_length;	/* DW1000_Pre_Length */
	uint8_t  tx_code;			/* Transmitter preamble code */
	uint8_t  rx_code;			/* Receiver preamble code */
	uint8_t  xtal_trim;
	uint16_t sfd_timeout;
	uint16_t ant_delay;

	uint32_t otp_revision;
	uint32_t part_id;
	uint32_t lot_id;
	uint32_t on_wake;	/* Set: DW1000_AON_CFG_ONW_LLDE | DW1000_AON_CFG_ONW_LLD0 */
	// bool delay_start;

	/* Registers */
	uint32_t sys_cfg;		/* 0x04 – System Configuration */
	uint32_t tx_fctrl;		/* 0x08 – Transmit Frame Control */
	uint32_t sys_mask;		/* 0x0E – System Event Mask Register. @TODO: init to 0. */
	uint32_t rx_finfo;		/* 0x10 – RX Frame Information Register */
	uint32_t chan_ctrl;		/* 0x1F – Channel Control */
} DW1000;


/* Public Functions ------------------------------------------------------------------------------ */
// void dw1000_init(
// 	DW1000* dw1000,
// 	const struct device* spi,
// 	struct spi_cs_control* ctrl,
// 	const struct device* gpio_dev,
// 	uint32_t gpio_int_pin);

void dw1000_init(DW1000*, uint32_t);

void dw1000_lock             (DW1000*);
void dw1000_unlock           (DW1000*);
void dw1000_soft_reset       (DW1000*);
bool dw1000_reconfig         (DW1000*, DW1000_Config*);
void dw1000_set_tx_ant_delay (DW1000*, uint16_t);
void dw1000_set_rx_ant_delay (DW1000*, uint16_t);
// void dw1000_irq_gpio_callback(const struct device*, struct gpio_callback*, uint32_t);
// void     dw1000_set_txpwr       (DW1000*, uint32_t, uint8_t);	/* TX_POWER_ID, TC_PGDELAY_OFFSET */

inline unsigned          dw1000_channel        (const DW1000* d) { return d->channel;         }
inline DW1000_Data_Rate  dw1000_data_rate      (const DW1000* d) { return d->data_rate;       }
inline DW1000_Prf        dw1000_prf            (const DW1000* d) { return d->prf;             }
inline DW1000_Pac        dw1000_pac            (const DW1000* d) { return d->pac;             }
inline DW1000_Pre_Length dw1000_preamble_length(const DW1000* d) { return d->preamble_length; }
inline unsigned          dw1000_tx_code        (const DW1000* d) { return d->tx_code;         }
inline unsigned          dw1000_rx_code        (const DW1000* d) { return d->rx_code;         }
inline uint16_t          dw1000_sfd_timeout    (const DW1000* d) { return d->sfd_timeout;     }
inline uint16_t          dw1000_ant_delay      (const DW1000* d) { return d->ant_delay;       }
       uint32_t          dw1000_read_dev_id    (DW1000*);

/* DW1000 time conversion.
 * DW1000 system clock is 499.2 MHz * 128 = 63.8976 GHz = ~15.65 ps
 * 1 dw1000 ns = 2^6  / (63.8976 GHz) = ~1.0016 ns
 * 1 dw1000 us = 2^16 / (63.8976 GHz) = ~1.0256 us = 1024 dw1000 ns
 * 1 dw1000 ms = 2^26 / (63.8976 GHz) = ~1.0503 ms = 1024 dw1000 us */
inline uint64_t          dw1000_ns_to_ticks    (uint64_t ns) { return ns * 64;       }
inline uint64_t          dw1000_us_to_ticks    (uint64_t us) { return us * 65536;    }
inline uint64_t          dw1000_ms_to_ticks    (uint64_t ms) { return ms * 67108864; }

int      dw1000_wait_for_irq        (DW1000*, uint32_t);
int32_t  dw1000_set_trx_tstamp      (DW1000*, uint64_t);
bool     dw1000_write_tx            (DW1000*, const void*, unsigned, unsigned);
bool     dw1000_write_tx_fctrl      (DW1000*, unsigned, unsigned);
bool     dw1000_start_tx            (DW1000*, bool);
bool     dw1000_start_delayed_tx    (DW1000*, bool);
void     dw1000_set_wait_for_rx     (DW1000*, uint32_t);

void     dw1000_set_drxb            (DW1000*, bool);
bool     dw1000_get_drxb            (DW1000*);
void     dw1000_sync_drxb           (DW1000*, uint32_t);
void     dw1000_hrbpt               (DW1000*);

void     dw1000_set_preamble_timeout(DW1000*, uint16_t);
void     dw1000_set_rx_timeout      (DW1000*, uint16_t);
bool     dw1000_start_rx            (DW1000*);
bool     dw1000_start_delayed_rx    (DW1000*);
bool     dw1000_read_rx             (DW1000*, void*, unsigned, unsigned);
uint32_t dw1000_read_status         (DW1000*);
uint32_t dw1000_read_rx_finfo       (DW1000*);
uint64_t dw1000_read_sys_tstamp     (DW1000*);
uint64_t dw1000_read_tx_tstamp      (DW1000*);
uint64_t dw1000_read_rx_tstamp      (DW1000*);
float    dw1000_rx_clk_offset       (DW1000*);
void     dw1000_sleep_after_tx      (DW1000*, bool);
void     dw1000_sleep_after_rx      (DW1000*, bool);

void     dw1000_int_enable          (DW1000*, uint32_t);
void     dw1000_int_clear           (DW1000*, uint32_t);
void     dw1000_int_disable         (DW1000*, uint32_t);
uint32_t dw1000_handle_irq          (DW1000*);

void     dw1000_force_trx_off       (DW1000*, uint32_t);
void     dw1000_rx_reset            (DW1000*);

void     dw1000_config_sleep        (DW1000*, uint16_t, uint8_t);
void     dw1000_enter_sleep         (DW1000*);
void     dw1000_wakeup_by_cs        (DW1000*);
// void     dw1000_wakeup_by_pin       (DW1000*);

// void     dw1000_start_sniff         (DW1000*, uint8_t, uint8_t);
// void     dw1000_start_lp_listen     (DW1000*);


/* see https://github.com/Decawave/mynewt-dw1000-core/blob/master/hw/drivers/dw1000/src/dw1000_dev.c */
/* see https://tools.ietf.org/html/rfc2292#section-1 */


// void dw1000_sleep    (DW1000*);
// void dw1000_deepsleep(DW1000*);


#ifdef __cplusplus
}
#endif

#endif // DW1000_H
/******************************************* END OF FILE *******************************************/
