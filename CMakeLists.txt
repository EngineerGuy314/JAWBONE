# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_BUILD_TYPE "Release") 

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
# We also need PICO EXTRAS
include(pico_extras_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(JAWBONE C CXX ASM)
add_definitions(-DPICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)  #fixes problem with slow XOSC startup causing failure to boot- Thanks Kazu!
pico_sdk_init()
add_executable(JAWBONE)
pico_generate_pio_header(JAWBONE ${CMAKE_CURRENT_LIST_DIR}/timer_PIO.pio)# These Lines are what invoke the assembler that compiles the .pio programs
pico_generate_pio_header(JAWBONE ${CMAKE_CURRENT_LIST_DIR}/onewire/onewire_library.pio) 
target_sources(JAWBONE PUBLIC
               ${CMAKE_CURRENT_LIST_DIR}/pico_fractional_pll.c
               ${CMAKE_CURRENT_LIST_DIR}/GPStime.c
               ${CMAKE_CURRENT_LIST_DIR}/TxChannel.c
               ${CMAKE_CURRENT_LIST_DIR}/WSPRutility.c
               ${CMAKE_CURRENT_LIST_DIR}/nhash.c
               ${CMAKE_CURRENT_LIST_DIR}/maidenhead.c
               ${CMAKE_CURRENT_LIST_DIR}/WSPRbeacon.c
               ${CMAKE_CURRENT_LIST_DIR}/logutils.c
               ${CMAKE_CURRENT_LIST_DIR}/onewire/onewire_library.c  
               ${CMAKE_CURRENT_LIST_DIR}/utilities.c
               ${CMAKE_CURRENT_LIST_DIR}/main.c
              )

pico_set_program_name(JAWBONE "JAWBONE")
pico_set_program_version(JAWBONE "0.5")

pico_enable_stdio_uart(JAWBONE 0) # disable stdio to UART
pico_enable_stdio_usb(JAWBONE 1)

# Add the standard include files to the build
target_include_directories(JAWBONE PRIVATE
                           ${CMAKE_CURRENT_LIST_DIR}
                           ${CMAKE_CURRENT_LIST_DIR}/..
                          )

target_link_libraries(JAWBONE
                      pico_stdlib
                      pico_sync
                      pico_multicore
                      hardware_timer
                      hardware_clocks
                      hardware_pio
					  hardware_adc
					  hardware_flash
					  hardware_sync
					  hardware_i2c
					  hardware_sleep
				      hardware_exception
					  hardware_pwm
					  hardware_irq
                     )

pico_add_extra_outputs(JAWBONE)
