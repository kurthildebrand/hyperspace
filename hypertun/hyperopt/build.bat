@REM Builds the hyperspace tun project. This batch file is expected to be called in hyperspace-tun/.
@echo off

@REM Configure the cmake build system if necessary.
if not exist build/ (
	mkdir build\
	cmake -S.\ -B.\build -G Ninja
)

@REM Build Hyperopt.
cmake --build ./build
