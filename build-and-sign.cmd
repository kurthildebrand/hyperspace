@REM ARG 1: Workspace root filepath

@REM Setup environment
CALL %~f1\zephyrproject\zephyr\zephyr-env.cmd

@REM Build in current directory
west build

@REM Get firmware version
CALL %~f1\fw_version.cmd
@echo Firmware Manuf: %fw_manuf%
@echo Firmware Board: %fw_board%
@echo Firmware Version: %fw_version%

@REM Sign
west sign -t imgtool -p %~f1\zephyrproject\bootloader\mcuboot\scripts\imgtool.py -- --key %~f1\zephyrproject\bootloader\mcuboot\root-ed25519.pem --version %fw_version% --custom-tlv 0xA0 %fw_manuf%,%fw_board%,%fw_version%
