cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 1.5.1)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(assistente_pessoal C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(assistente_pessoal
    main.c
)

target_link_libraries(assistente_pessoal pico_stdlib hardware_adc hardware_pwm pico_cyw43_arch_lwip_threadsafe_background)

# enable usb output, disable uart output
pico_enable_stdio_usb(assistente_pessoal 1)
pico_enable_stdio_uart(assistente_pessoal 0)

# Add the standard include files to the build
target_include_directories(assistente_pessoal PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_options(assistente_pessoal PRIVATE -Os)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(assistente_pessoal)