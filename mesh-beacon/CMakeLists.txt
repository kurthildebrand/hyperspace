cmake_minimum_required(VERSION 3.13.1)

set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/../)
set(BOARD mdek1001)

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)

project(mesh-beacon)

target_include_directories(app PRIVATE
	# CMSIS
	../zephyrproject/modules/hal/cmsis/CMSIS/Core/Include/

	# NRFX
	../zephyrproject/modules/hal/nordic/
	../zephyrproject/modules/hal/nordic/nrfx/
	../zephyrproject/modules/hal/nordic/nrfx/drivers/
	../zephyrproject/modules/hal/nordic/nrfx/drivers/include/
	../zephyrproject/modules/hal/nordic/nrfx/hal/
	../zephyrproject/modules/hal/nordic/nrfx/helpers/
	../zephyrproject/modules/hal/nordic/nrfx/mdk/
	../zephyrproject/modules/hal/nordic/nrfx/templates/

	# Mistlib
	../mistlib/algorithms/
	../mistlib/net/
	../mistlib/types/

	# Application
	./
	../
	../common/
)

target_include_directories(app PRIVATE ${ZEPHYR_BASE}/subsys/net/ip)

target_sources(app PRIVATE
	# NRFX
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_adc.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_clock.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_comp.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_dppi.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_egu.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_gpiote.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_i2s.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_ipc.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_lpcomp.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_nfct.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_nvmc.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_pdm.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_power.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_ppi.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_pwm.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_qdec.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_qspi.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_rng.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_rtc.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_saadc.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_spi.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_spim.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_spis.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_systick.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_temp.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_timer.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_twi_twim.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_twi.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_twim.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_twis.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_uart.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_uarte.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_usbd.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_usbreg.c
	../zephyrproject/modules/hal/nordic/nrfx/drivers/src/nrfx_wdt.c

	# Mistlib
	../mistlib/algorithms/byteorder.c
	../mistlib/algorithms/calc.c
	../mistlib/algorithms/insertsort.c
	../mistlib/algorithms/matrix.c
	../mistlib/algorithms/order.c
	../mistlib/algorithms/search.c
	../mistlib/algorithms/selsort.c
	# ../mistlib/net/ieee_802_15_4.c
	../mistlib/types/array.c
	../mistlib/types/bits.c
	../mistlib/types/buffer.c
	../mistlib/types/compare.c
	../mistlib/types/entry.c
	../mistlib/types/heap.c
	../mistlib/types/key.c
	../mistlib/types/linked.c
	../mistlib/types/list.c
	../mistlib/types/map.c
	../mistlib/types/pool.c
	../mistlib/types/range.c
	../mistlib/types/ringbuffer.c
	../mistlib/types/stack.c

	# Application
	main.c
	coap_server.c
	# ../common/coap_test.c
	../common/backoff.c
	../common/bayesian.c
	../common/dw1000.c
	../common/ieee_802_15_4.c
	../common/iir.c
	../common/location.c
	../common/lowpan.c
	../common/spim_nrf52832.c
	../common/spis_if.c
	../common/timeslot.c
	../common/tsch.c
)
