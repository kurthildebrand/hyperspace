/************************************************************************************************//**
 * @file		config.h
 * @brief
 *
 * @copyright	Copyright Kurt Hildebrand.
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
#ifndef CONFIG_H
#define CONFIG_H

#if __STDC_VERSION__ < 199901L
#error Compile with C99 or higher!
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* Public Macros --------------------------------------------------------------------------------- */
// #define TX_ANT_DELAY        (16300)
#define TX_ANT_DELAY        (16456)
#define RX_ANT_DELAY        (16456)


/* RPI Header Pin-out -------------------------------------------------------------------------------
 * 
 * 		NRF        | DWM   | RPI Function     | RPI   | RPI Function    | DWM   | NRF       
 * 		-----------|-------|------------------|-------|-----------------|-------|-----------
 * 		           |       |           +3.3v  |  1 2  | +5v             |       |           
 * 		P0.15      | J1.23 | 12C1 SDA  GPIO2  |  3 4  | +5v             |       |           
 * 		P0.08      | J1.25 | 12C1 SCL  GPIO3  |  5 6  | GND             |       |           
 * 		           |       |           GPIO4  |  7 8  | GPIO14          | J1.18 | P0.11     
 * 		           |       |           GND    |  9 10 | GPIO15          | J1.20 | P0.05/AIN3
 * 		           |       |           GPIO17 | 11 12 | GPIO18 RESET    | J1.33 | P0.21     
 * 		           |       |           GPIO27 | 13 14 | GND             |       |           
 * 		P0.26      | J1.19 |           GPIO22 | 15 16 | GPIO23          |       |           
 * 		           |       |           +3.3v  | 17 18 | GPIO24          |       |           
 * 		P0.06      | J1.27 | SPI0 MOSI GPIO10 | 19 20 | GND             |       |           
 * 		P0.07      | J1.26 | SPI0 MISO GPIO9  | 21 22 | GPIO25 SPI0 CS0 |       |           
 * 		P0.04/AIN2 | J1.28 | SPI0 SCLK GPIO11 | 23 24 | GPIO8  SPI0 CS1 | J1.29 | P0.03/AIN1
 * 		           |       |           GND    | 25 26 | GPIO7           |       |           
 * 
 * J7 Header Pin-out --------------------------------------------------------------------------------
 * 
 * 		NRF    | DWM   | J7 Header   | DWM   | NRF   
 * 		-------|-------|-------------|-------|-------
 * 		DW_GP0 | J1.21 |    1   2    | J1.22 | DW_GP0
 * 		P0.12  | J1.6  |    3   4    | J1.13 | P0.27
 * 		P0.29  | J1.14 |    5   6    | J1.15 | P0.28
 * 		P0.23  | J1.16 |    7   8    | J1.17 | P0.13
 * 
 * NRF Pin-out --------------------------------------------------------------------------------------
 * 
 * 		P0.00/XL1       |           |   32.768 kHz  |
 * 		P0.01/XL2       |           |   32.768 kHz  |
 * 		P0.02/AIN0      |   J1.32   |   BT_WAKE_UP  |
 * 		P0.03/AIN1      |   J1.29   |   CS_RPI      |   RPi Chip Select
 * 		P0.04/AIN2      |   J1.28   |   SPI1_CLK    |   RPi SPI CLK
 * 		P0.05/AIN3      |   J1.20   |   RXD         |   UART TXD
 * 		P0.06           |   J1.27   |   SPI1_MOSI   |   RPi SPI MOSI
 * 		P0.07           |   J1.26   |   SPI1_MISO   |   RPi SPI MISO
 * 		P0.08           |   J1.25   |   SCL_RPI     |   RPi I2C SCL
 * 		P0.09           |   J1.5    |               |
 * 		P0.10           |   J1.4    |               |
 * 		P0.11           |   J1.18   |   TXD         |   UART RXD
 * 		P0.12           |   J1.6    |   M_PIN6      |
 * 		P0.13           |   J1.17   |   M_PIN17     |
 * 		P0.14           |   J1.7    |   RED LED     |
 * 		P0.15           |   J1.23   |   SDA_RPI     |   RPi I2C SDA
 * 		P0.16           |           |   SPI1_CLK    |   DW1000 SPI CLK
 * 		P0.17           |           |   DW_CS       |   DW1000 Chip Select
 * 		P0.18           |           |   SPI1_MISO   |   DW1000 SPI MISO
 * 		P0.19           |           |   DW_IRQ      |
 * 		P0.20           |           |   SPI1_MOSI   |   DW1000 SPI MOSI
 * 		P0.21/RESET     |   J1.33   |               |
 * 		P0.22           |   J1.8    |   RED LED     |
 * 		P0.23           |   J1.16   |               |   M_PIN16
 * 		P0.24           |           |   DW_RST      |
 * 		P0.25           |           |   IRQ_ACC     |
 * 		P0.26           |   J1.19   |   READY       |   GPIO_RPI
 * 		P0.27           |   J1.13   |               |   M_PIN13
 * 		P0.28/AIN4      |   J1.15   |   I2C_SCL     |   M_PIN15
 * 		P0.29/AIN5      |   J1.14   |   I2C_SDA     |   M_PIN14
 * 		P0.30/AIN6      |   J1.10   |   GREEN LED   |
 * 		P0.31/AIN7      |   J1.9    |   BLUE LED    | 
 */
#define RPI_I2C_DEVICE		(I2C1)
#define RPI_I2C_SDA_PIN		(15)
#define RPI_I2C_SCL_PIN		(8)

#define RPI_SPI_DEVICE		(NRF_SPIM2)
// #define RPI_SPI_DEVICE		(NRF_SPIS2)
#define RPI_SPI_MOSI_PIN	(6)
#define RPI_SPI_MISO_PIN	(7)
#define RPI_SPI_SCLK_PIN	(4)
#define RPI_SPI_CS1_PIN		(3)
#define RPI_IRQ_PIN			(26)

#define RPI_GPIO14_PIN		(11)
#define RPI_GPIO15_PIN		(5)
#define RPI_GPIO18_PIN		(21)

#define DW1000_SPI_DEVICE	(NRF_SPIM1)
#define DW1000_SPI_CLK_PIN	(16)
#define DW1000_SPI_CS_PIN	(17)
#define DW1000_SPI_MOSI_PIN	(20)
#define DW1000_SPI_MISO_PIN	(18)
#define DW1000_IRQ_PIN		(19)
#define DW1000_IRQ_PRIO		(1)

#define I2C_SCL_PIN			(28)
#define I2C_SDA_PIN			(29)
#define ACC_IRQ_PIN			(25)

#define J7_3_PIN			(12)
#define J7_4_PIN			(27)
#define J7_5_PIN			(29)
#define J7_6_PIN			(28)
#define J7_7_PIN			(23)
#define J7_8_PIN			(13)

#define UART_TXD_PIN		(5)
#define UART_RXD_PIN		(11)

#define RED_LED1_PIN		(14)
#define RED_LED2_PIN		(22)
#define GREEN_LED_PIN		(30)
#define BLUE_LED_PIN		(31)


#ifdef __cplusplus
}
#endif

#endif // CONFIG_H
/******************************************* END OF FILE *******************************************/
