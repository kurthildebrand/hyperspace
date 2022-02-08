# https://stackoverflow.com/questions/17511496/how-to-create-a-shared-library-with-cmake
cmake_minimum_required(VERSION 3.13.1)

set(CMAKE_TOOLCHAIN_FILE ../../../toolchain.rpi.cmake)

project(hyperopt VERSION 1.0.0)

enable_language(C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Release)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

target_compile_options(${PROJECT_NAME} PRIVATE
	-Oz
	-nostdlib
	-Wall
	-Wextra
)

target_link_options(${PROJECT_NAME} PRIVATE
	-Oz
	-nostdlib
	-fuse-ld=lld
	-B../../../gcc-arm-10.3-2021.07-mingw-w64-i686-arm-none-linux-gnueabihf/lib/gcc/arm-none-linux-gnueabihf/10.3.1/
	-L../../../gcc-arm-10.3-2021.07-mingw-w64-i686-arm-none-linux-gnueabihf/lib/gcc/arm-none-linux-gnueabihf/10.3.1/
)

target_include_directories(${PROJECT_NAME} PRIVATE
	./
	../../mistlib/algorithms/
	../../mistlib/net/
	../../mistlib/net/ip/
	../../mistlib/types/
)

target_sources(${PROJECT_NAME} PRIVATE
	hyperopt.c
	../../../mistlib/algorithms/byteorder.c
	../../../mistlib/algorithms/calc.c
	../../../mistlib/types/buffer.c
	../../../mistlib/types/key.c
	../../../mistlib/types/linked.c
	../../../mistlib/net/ip/ipv6.c
)

# target_link_libraries(${PROJECT_NAME} PRIVATE mistlib)

# add_subdirectory(../../../mistlib)

# target_sources(${PROJECT_NAME} PRIVATE
# 	buffer.c
# 	byteorder.c
# 	ipv6.c
# 	key.c
# 	hyperopt.c
# )
