cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(dejavu_payload C ASM)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O0 -mcpu=cortex-a9 -mthumb-interwork -fno-builtin -Wno-main")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T ${CMAKE_CURRENT_SOURCE_DIR}/linker.x -nostartfiles -nodefaultlibs -nostdlib")

include_directories(
  ./
  msif
  FatFs
)

add_executable(dejavu_payload
  start.S
  main.c
  state.c
  gpio.c
  pervasive.c
  spi.c
  syscon.c
  libc/qsort.c
  libc/string.c
  msif/ms.c
  msif/msif.c
  msif/msif_auth.c
  FatFs/diskio.c
  FatFs/ff.c
  FatFs/ffsystem.c
  FatFs/ffunicode.c
)

target_link_libraries(dejavu_payload
  gcc
)

add_custom_command(TARGET dejavu_payload
  COMMAND ${CMAKE_OBJCOPY} -S -O binary dejavu_payload dejavu_payload.bin
  DEPENDS dejavu_payload
)

add_custom_target(dejavu_payload_binary DEPENDS dejavu_payload)
