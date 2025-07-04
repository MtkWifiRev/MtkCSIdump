cmake_minimum_required(VERSION 3.1)
project(motion-detector)

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

add_executable(motion-detector ${SOURCES} main.cpp)
add_library(motiondetector STATIC ${SOURCES})

set(LIBNL_TINY_PATH /data/mediatek/leaks/meta-gl-motion-detection/recipes-ccsp/hal/files/libnl-tiny_bin/usr/local/lib/)

find_library(LIBNL_TINY nl-tiny PATHS ${LIBNL_TINY_PATH})

include_directories(/data/mediatek/leaks/meta-gl-motion-detection/recipes-ccsp/hal/files/libnl-tiny_bin/usr/local/include/libnl-tiny/)

add_compile_options(-Wall -Wextra)
find_package(Threads REQUIRED)
target_link_libraries(motion-detector PRIVATE Threads::Threads ${LIBNL_TINY})

set(CMAKE_INSTALL_PREFIX /usr)
install(TARGETS motion-detector RUNTIME DESTINATION z)
