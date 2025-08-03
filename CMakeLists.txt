cmake_minimum_required(VERSION 3.1)
project(CSIdump)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_CXX_COMPILER /home/daniel/openwrt-sdk_24-10-01/openwrt-sdk-24.10.1-mediatek-filogic_gcc-13.3.0_musl.Linux-x86_64/staging_dir/toolchain-aarch64_cortex-a53_gcc-13.3.0_musl/bin/aarch64-openwrt-linux-g++)

set(SOURCES
  parsers/parser_mt76.cpp
  wifi_drv_api/mt76_api.cpp
  motion_detector.cpp
  md.cpp
)

include_directories(${CMAKE_SOURCE_DIR})

add_executable(CSIdump ${SOURCES} main.cpp)

set(LIBNL_TINY_PATH /data/mediatek/leaks/meta-gl-motion-detection/recipes-ccsp/hal/files/libnl-tiny_bin/usr/local/lib/)

find_library(LIBNL_TINY nl-tiny PATHS ${LIBNL_TINY_PATH})

include_directories(/data/mediatek/leaks/meta-gl-motion-detection/recipes-ccsp/hal/files/libnl-tiny_bin/usr/local/include/libnl-tiny/)

add_compile_options(-Wall -Wextra)

# Static linking configuration
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

find_package(Threads REQUIRED)
target_link_libraries(CSIdump PRIVATE Threads::Threads ${LIBNL_TINY} -lgcc_eh -lgcc -static-libgcc -static-libstdc++)
