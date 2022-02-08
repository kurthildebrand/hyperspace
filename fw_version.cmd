@echo off
set fw_manuf=
set fw_board=
set fw_version=

for /f "tokens=*" %%i in ('FINDSTR /rc:"\<FW_MANUF\>" fw_version.h') do (
	for /f usebackq^ tokens^=2^ delims^=^" %%a in ('%%i') do set fw_manuf=%%a
)

for /f "tokens=*" %%i in ('FINDSTR /rc:"\<FW_BOARD\>" fw_version.h') do (
	for /f usebackq^ tokens^=2^ delims^=^" %%a in ('%%i') do set fw_board=%%a
)
for /f "tokens=*" %%i in ('FINDSTR /rc:"\<FW_VERSION\>" fw_version.h') do (
	for /f usebackq^ tokens^=2^ delims^=^" %%a in ('%%i') do set fw_version=%%a
)
