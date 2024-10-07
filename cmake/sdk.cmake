
set(SDK_PATH        ${PROJECT_ROOT}/external/SDK)
set(HAL_PATH        ${SDK_PATH}/STM32F7xx_HAL_Driver)
set(CMSIS_PATH      ${SDK_PATH}/CMSIS)
set(HAL_CONFIG_PATH ${PROJECT_ROOT}/source/board/inc)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMSIS_INCLUDES
    ${CMSIS_PATH}/Device/ST/STM32F7xx/Include
    ${CMSIS_PATH}/Include
)

set(HAL_INCLUDES
    ${HAL_PATH}/Inc
    ${HAL_PATH}/Inc/Legacy
    ${HAL_CONFIG_PATH}
)

set(SDK_INCLUDES
    ${CMSIS_INCLUDES}
    ${HAL_INCLUDES}
)

file(GLOB HAL_SOURCES CONFIGURE_DEPENDS ${HAL_PATH}/Src/*.c)

add_library(sdk STATIC)
target_include_directories(sdk PUBLIC ${SDK_INCLUDES})
target_sources(sdk PRIVATE ${HAL_SOURCES})
target_compile_definitions(sdk PRIVATE
    -DUSE_HAL_DRIVER
    -DSTM32F767xx
)
