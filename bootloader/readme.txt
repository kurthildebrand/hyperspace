Bootloader
----------
Todo: DMCUBOOT_DOWNGRADE_PREVENTION=y

west build -b mdek1001 -s ../zephyrproject/bootloader/mcuboot/boot/zephyr -- -DBOARD_ROOT=../../../../../ -DCONFIG_BOOT_SIGNATURE_TYPE_ED25519=y
west flash

Application
-----------
west sign -t imgtool -p ../zephyrproject/bootloader/mcuboot/scripts/imgtool.py -- --key ../zephyrproject/bootloader/mcuboot/root-ed25519.pem
west flash --hex-file ./build/zephyr/zephyr.signed.hex
