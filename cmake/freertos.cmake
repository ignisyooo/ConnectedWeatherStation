
set(FREERTOS_PATH ${PROJECT_ROOT}/external/FreeRTOS/Source)


set(FREERTOS_INCLUDES 
    ${FREERTOS_PATH}/CMSIS_RTOS_V2
    ${FREERTOS_PATH}/include
    ${FREERTOS_PATH}/portable/GCC/ARM_CM7/r0p1
)

set(FREERTOS_SOURCES
    ${FREERTOS_PATH}/CMSIS_RTOS_V2/cmsis_os2.c
    ${FREERTOS_PATH}/portable/GCC/ARM_CM7/r0p1/port.c
    ${FREERTOS_PATH}/portable/MemMang/heap_4.c
    ${FREERTOS_PATH}/croutine.c
    ${FREERTOS_PATH}/event_groups.c
    ${FREERTOS_PATH}/list.c
    ${FREERTOS_PATH}/queue.c
    ${FREERTOS_PATH}/stream_buffer.c
    ${FREERTOS_PATH}/tasks.c
    ${FREERTOS_PATH}/timers.c
)

add_library(freertos STATIC)
target_include_directories(freertos PUBLIC ${FREERTOS_INCLUDES})
target_sources(freertos PRIVATE ${FREERTOS_SOURCES})
target_compile_definitions(freertos PRIVATE
    FREERTOS
)

target_link_libraries(freertos PRIVATE sdk)
