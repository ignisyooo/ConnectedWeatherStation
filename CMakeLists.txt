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

add_executable(${MAIN_TARGET})

target_compile_definitions(${MAIN_TARGET} PUBLIC
    FREERTOS
    USE_HAL_DRIVER
    STM32F767xx
)

include(cmake/sdk.cmake)
include(cmake/freertos.cmake)
include(cmake/lwip.cmake)
include(cmake/cjson.cmake)
include(cmake/lvgl.cmake)

add_subdirectory(source)

target_link_libraries(${MAIN_TARGET} PUBLIC sdk freertos lwip cjson lvgl)

add_custom_command(TARGET ${MAIN_TARGET}
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${MAIN_TARGET}.elf ${MAIN_TARGET}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${MAIN_TARGET}.elf ${MAIN_TARGET}.bin
)
