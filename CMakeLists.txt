cmake_minimum_required(VERSION 3.22)
# ##############################################################################

project(ConnectedWeatherStation LANGUAGES C ASM)

set(CMAKE_EXECUTABLE_SUFFIX .elf)


set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(MAIN_TARGET ${PROJECT_NAME})

set(CPU_PARAMETERS
    -mthumb
    -mcpu=cortex-m7
    -mfpu=fpv5-d16
    -mfloat-abi=hard
)

add_executable(${MAIN_TARGET})

target_compile_options(${MAIN_TARGET} PUBLIC
    ${CPU_PARAMETERS}
    -g
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    $<$<CONFIG:Debug>:-Og>
)

target_link_options(${MAIN_TARGET} PUBLIC
    -T${CMAKE_CURRENT_SOURCE_DIR}/board_config/STM32F767ZITX_FLASH.ld
    ${CPU_PARAMETERS}
    -Wl,-Map=${MAIN_TARGET}.map
    --specs=nosys.specs
    -u _printf_float                # STDIO float formatting support
    -Wl,--start-group
    -lc
    -lm
    -lstdc++
    -lsupc++
    -Wl,--end-group
    -Wl,--print-memory-usage
)

target_compile_definitions(${MAIN_TARGET} PUBLIC
    FREERTOS
    USE_HAL_DRIVER
    STM32F767xx
)


include(cmake/sdk.cmake)

add_subdirectory(source)

target_link_libraries(${MAIN_TARGET} PUBLIC sdk)

add_custom_command(TARGET ${MAIN_TARGET}
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${MAIN_TARGET}.elf ${MAIN_TARGET}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${MAIN_TARGET}.elf ${MAIN_TARGET}.bin
)
