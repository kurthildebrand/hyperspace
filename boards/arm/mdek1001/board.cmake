board_runner_args(jlink "--device=nrf52" "--speed=4000")

include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
