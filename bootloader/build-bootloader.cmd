@REM ARG1: Workspace root filepath

@REM Setup environment
CALL %~f1\zephyrproject\zephyr\zephyr-env.cmd

@REM Build the bootloader
west build -b mdek1001 -s ../zephyrproject/bootloader/mcuboot/boot/zephyr -- -DBOARD_ROOT=../../../../../ -DCONFIG_BOOT_SIGNATURE_TYPE_ED25519=y
