target_sources(${PROJECT_NAME} PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/clock.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gpio.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/stm32f7xx_hal_msp.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/usart.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/error.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/system_stm32f7xx.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/startup/startup_stm32f767zitx.s"
)

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/inc")
