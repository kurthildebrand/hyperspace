/************************************************************************************************//**
 * @file		nrf52832_spim.c
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
#include <nrfx/hal/nrf_gpio.h>
#include <nrfx/hal/nrf_spim.h>
#include <stdatomic.h>
#include <stdint.h>
// #include <sys/atomic.h>
#include <zephyr.h>	/* Zephyr */

#include "config.h"
#include "spim.h"


/* Private Macros -------------------------------------------------------------------------------- */
// #define SPIM            RPI_SPI_DEVICE
// #define SCK             RPI_SPI_SCLK_PIN
// #define MOSI            RPI_SPI_MOSI_PIN
// #define MISO            RPI_SPI_MISO_PIN
// #define CS              RPI_SPI_CS1_PIN
// #define SPIM_IRQ_PRIO   1


#define SPIM            NRF_SPIM1
#define SCK             (DW1000_SPI_CLK_PIN)
#define MOSI            (DW1000_SPI_MOSI_PIN)
#define MISO            (DW1000_SPI_MISO_PIN)
#define CS              (DW1000_SPI_CS_PIN)
#define SPIM_IRQ_PRIO   (DW1000_IRQ_PRIO)


/* Private Types --------------------------------------------------------------------------------- */
typedef struct {
	const Spibuf* txbufs;
	unsigned      txcount;
	const Spibuf* rxbufs;
	unsigned      rxcount;
} Spi_Transaction;


/* Private Functions ----------------------------------------------------------------------------- */
// static inline nrf_spim_mode_t      get_nrf_spim_mode     (uint16_t operation);
// static inline nrf_spim_bit_order_t get_nrf_spim_bit_order(uint16_t operation);
static inline nrf_spim_frequency_t get_nrf_spim_frequency(uint32_t frequency);
static inline void spim_trx_prime_load(void);
static inline bool spim_trx_prime_next(void);
void SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQHandler(void);


/* Private Variables ----------------------------------------------------------------------------- */
// static struct k_mutex spim_mutex;
// static struct k_sem   spim_sem;
// static atomic_t        spim_done;
static atomic_flag     spim_done;
static uint32_t        spim_frequency;
static Spi_Transaction spim_transaction;


// void SPIM2_SPIS2_SPI2_IRQHandler(void)
// void SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQHandler(void)	/* Non-Zephyr */
void spim_irq_handler(void)	/* Zephyr */
{
	if(SPIM->EVENTS_STARTED)
	{
		SPIM->EVENTS_STARTED = 0;
		spim_trx_prime_load();
	}

	if(SPIM->EVENTS_END)
	{
		SPIM->EVENTS_END = 0;
		if(!spim_trx_prime_next()) {
			atomic_flag_clear(&spim_done);
		} else {
			nrf_spim_task_trigger(SPIM, NRF_SPIM_TASK_START);
		}
	}
}


/* spim_init ************************************************************************************//**
 * @brief		*/
void spim_init(void)
{
	// k_mutex_init(&spim_mutex);
	// k_sem_init(&spim_sem, 0, 1);

	nrf_spim_disable(SPIM);
	nrf_spim_pins_set(SPIM, SCK, MOSI, MISO);

	nrf_spim_rx_list_disable(SPIM);
	nrf_spim_tx_list_disable(SPIM);
	spim_set_freq           (8000000);
	// nrf_spim_frequency_set  (SPIM, NRF_SPIM_FREQ_8M);
	nrf_spim_configure      (SPIM, NRF_SPIM_MODE_0, NRF_SPIM_BIT_ORDER_MSB_FIRST);
	nrf_spim_orc_set        (SPIM, 0x00);
	nrf_spim_enable         (SPIM);

	/* Configure GPIO CS pin as output */
	nrf_gpio_cfg_output(CS);
	spim_cs(0);

	// /* Non-Zephyr */
	// NVIC_SetPriority(NRFX_IRQ_NUMBER_GET(SPIM), 1);
	// NVIC_EnableIRQ(NRFX_IRQ_NUMBER_GET(SPIM));

	/* Zephyr */
	IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(SPIM), 1, spim_irq_handler, 0, 0);
	irq_enable(NRFX_IRQ_NUMBER_GET(SPIM));
}


/* spim_power_up ********************************************************************************//**
 * @brief		*/
void spim_power_up(void)
{
	nrf_spim_enable(SPIM);
	nrf_spim_pins_set(SPIM, SCK, MOSI, MISO);
}


/* spim_power_down ******************************************************************************//**
 * @brief		*/
void spim_power_down(void)
{
	nrf_spim_disable(SPIM);
	// nrf_gpio_cfg_default(nrf_spim_sck_pin_get(SPIM));
	// nrf_gpio_cfg_default(nrf_spim_miso_pin_get(SPIM));
	// nrf_gpio_cfg_default(nrf_spim_mosi_pin_get(SPIM));

	/* SPIM 1 Errata 89 Workaround */
	*(volatile uint32_t *)0x40004FFC = 0;
	*(volatile uint32_t *)0x40004FFC;
	*(volatile uint32_t *)0x40004FFC = 1;
}


// /* get_nrf_spim_mode ****************************************************************************//**
//  * @brief		*/
// static inline nrf_spim_mode_t get_nrf_spim_mode(uint16_t operation)
// {
// 	if (SPI_MODE_GET(operation) & SPI_MODE_CPOL) {
// 		if (SPI_MODE_GET(operation) & SPI_MODE_CPHA) {
// 			return NRF_SPIM_MODE_3;
// 		} else {
// 			return NRF_SPIM_MODE_2;
// 		}
// 	} else {
// 		if (SPI_MODE_GET(operation) & SPI_MODE_CPHA) {
// 			return NRF_SPIM_MODE_1;
// 		} else {
// 			return NRF_SPIM_MODE_0;
// 		}
// 	}
// }


// /* get_nrf_spim_bit_order ***********************************************************************//**
//  * @brief		*/
// static inline nrf_spim_bit_order_t get_nrf_spim_bit_order(uint16_t operation)
// {
// 	if (operation & SPI_TRANSFER_LSB) {
// 		return NRF_SPIM_BIT_ORDER_LSB_FIRST;
// 	} else {
// 		return NRF_SPIM_BIT_ORDER_MSB_FIRST;
// 	}
// }


/* get_nrf_pim_frequency ************************************************************************//**
 * @brief		*/
static inline nrf_spim_frequency_t get_nrf_spim_frequency(uint32_t frequency)
{
	/* Get the highest supported frequency not exceeding the requested one. */
	if (frequency < 250000) {
		return NRF_SPIM_FREQ_125K;
	} else if (frequency < 500000) {
		return NRF_SPIM_FREQ_250K;
	} else if (frequency < 1000000) {
		return NRF_SPIM_FREQ_500K;
	} else if (frequency < 2000000) {
		return NRF_SPIM_FREQ_1M;
	} else if (frequency < 4000000) {
		return NRF_SPIM_FREQ_2M;
	} else if (frequency < 8000000) {
		return NRF_SPIM_FREQ_4M;
#if defined(CONFIG_SOC_NRF52833) || defined(CONFIG_SOC_NRF52840)
	} else if (frequency < 16000000) {
		return NRF_SPIM_FREQ_8M;
	} else if (frequency < 32000000) {
		return NRF_SPIM_FREQ_16M;
	} else {
		return NRF_SPIM_FREQ_32M;
#else
	} else {
		return NRF_SPIM_FREQ_8M;
#endif
	}
}


/* spim_cs **************************************************************************************//**
 * @brief		*/
void spim_cs(bool on)
{
	// if(config && config->cs && config->cs->gpio_dev) {
	// 	if(on) {
	// 		if(config->operation & SPI_CS_ACTIVE_HIGH) {
	// 			// gpio_pin_write(config->cs->gpio_dev, config->cs->gpio_pin, 1);
	// 			nrf_gpio_pin_write(config->cs->gpio_pin, 1);
	// 		} else {
	// 			// gpio_pin_write(config->cs->gpio_dev, config->cs->gpio_pin, 0);
	// 			nrf_gpio_pin_write(config->cs->gpio_pin, 0);
	// 		}
	// 		// k_busy_wait(config->cs->delay);
	// 	} else {
	// 		// k_busy_wait(config->cs->delay);
	// 		if(config->operation & SPI_CS_ACTIVE_HIGH) {
	// 			// gpio_pin_write(config->cs->gpio_dev, config->cs->gpio_pin, 0);
	// 			nrf_gpio_pin_write(config->cs->gpio_pin, 0);
	// 		} else {
	// 			// gpio_pin_write(config->cs->gpio_dev, config->cs->gpio_pin, 1);
	// 			nrf_gpio_pin_write(config->cs->gpio_pin, 1);
	// 		}
	// 	}
	// }

	// if(config && config->cs && config->cs->gpio_dev) {
	// 	if(on) {
	// 		if(config->operation & SPI_CS_ACTIVE_HIGH) {
	// 			gpio_pin_write(config->cs->gpio_dev, config->cs->gpio_pin, 1);
	// 		} else {
	// 			gpio_pin_write(config->cs->gpio_dev, config->cs->gpio_pin, 0);
	// 		}
	// 		k_busy_wait(config->cs->delay);
	// 	} else {
	// 		k_busy_wait(config->cs->delay);
	// 		if(config->operation & SPI_CS_ACTIVE_HIGH) {
	// 			gpio_pin_write(config->cs->gpio_dev, config->cs->gpio_pin, 0);
	// 		} else {
	// 			gpio_pin_write(config->cs->gpio_dev, config->cs->gpio_pin, 1);
	// 		}
	// 	}
	// }

	/* CS is active low */
	if(on) {
		nrf_gpio_pin_write(CS, 0);
	} else {
		nrf_gpio_pin_write(CS, 1);
	}
}


/* spim_lock ************************************************************************************//**
 * @brief		*/
void spim_lock(void)
{
	// k_mutex_lock(&spim_mutex, K_FOREVER);
}


/* spim_unlock **********************************************************************************//**
 * @brief		*/
void spim_unlock(void)
{
	// k_mutex_unlock(&spim_mutex);
}


/* spim_set_freq ********************************************************************************//**
 * @brief		*/
void spim_set_freq(uint32_t freq)
{
	spim_frequency = get_nrf_spim_frequency(freq);
}


/* spim_trx *************************************************************************************//**
 * @brief		*/
void spim_trx(const Spibuf* txbufs, unsigned txcount, const Spibuf* rxbufs, unsigned rxcount)
{
	if(!txbufs && !rxbufs)
	{
		return;
	}
	else if(txcount == 0 && rxcount == 0)
	{
		return;
	}

	unsigned i;
	for(i = 0; i < txcount && i < rxcount; i++) {
		if(rxbufs[i].ptr && rxbufs[i].len == 1 &&
		   txbufs[i].ptr && rxbufs[i].len <= 1)
		{
			/* Transaction aborted since it would trigger nRF52832 PAN 58 */
			return;
		}
	}

	nrf_spim_frequency_set(SPIM, spim_frequency);

	/* Setup transaction */
	spim_transaction.txbufs  = txbufs;
	spim_transaction.txcount = txcount;
	spim_transaction.rxbufs  = rxbufs;
	spim_transaction.rxcount = rxcount;

	spim_trx_prime_load();
	spim_trx_prime_next();

	atomic_flag_test_and_set(&spim_done);
	nrf_spim_int_enable(SPIM, NRF_SPIM_INT_END_MASK | NRF_SPIM_INT_STARTED_MASK);

	/* Start the transaction */
	spim_cs(1);
	nrf_spim_task_trigger(SPIM, NRF_SPIM_TASK_START);

	/* Spin and wait for transaction to complete */
	while(atomic_flag_test_and_set(&spim_done)) { }

	spim_cs(0);
}


static inline void spim_trx_prime_load(void)
{
	if(spim_transaction.txcount && spim_transaction.txbufs->ptr) {
		nrf_spim_tx_buffer_set(SPIM, spim_transaction.txbufs->ptr, spim_transaction.txbufs->len);
	} else {
		nrf_spim_tx_buffer_set(SPIM, 0, 0);
	}

	if(spim_transaction.rxcount && spim_transaction.rxbufs->ptr) {
		nrf_spim_rx_buffer_set(SPIM, spim_transaction.rxbufs->ptr, spim_transaction.rxbufs->len);
	} else {
		nrf_spim_rx_buffer_set(SPIM, 0, 0);
	}
}


static inline bool spim_trx_prime_next(void)
{
	if(spim_transaction.txcount == 0 && spim_transaction.rxcount == 0) {
		return false;
	}

	/* Move to next tx buffer */
	if(spim_transaction.txcount) {
		spim_transaction.txcount--;
		spim_transaction.txbufs++;
	}

	/* Move to next rx buffer */
	if(spim_transaction.rxcount) {
		spim_transaction.rxcount--;
		spim_transaction.rxbufs++;
	}

	return true;
}


/******************************************* END OF FILE *******************************************/
