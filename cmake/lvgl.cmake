set(LVGL_PATH ${PROJECT_ROOT}/external/lvgl)
set(LV_CONF_INCLUDE_SIMPLE ${CMAKE_SOURCE_DIR}/source/board/inc)

file(GLOB_RECURSE LVGL_SOURCES
    ${LVGL_PATH}/lvgl/src/core/*.c
    ${LVGL_PATH}/lvgl/src/misc/*.c
    ${LVGL_PATH}/lvgl/src/hal/*.c
    ${LVGL_PATH}/lvgl/src/draw/*.c
    ${LVGL_PATH}/lvgl/src/widgets/*.c
    ${LVGL_PATH}/lvgl/src/font/*.c
    ${LVGL_PATH}/lvgl/src/extra/*.c
    ${LVGL_PATH}/lvgl/src/extra/**/*.c
    ${LVGL_PATH}/lvgl/examples/get_started/lv_example_get_started_2.c
)

add_library(lvgl STATIC ${LVGL_SOURCES})

target_include_directories(lvgl PUBLIC
    ${LVGL_PATH}/lvgl
    ${LVGL_PATH}/lvgl/src
    ${LVGL_PATH}/lvgl/src/core
    ${LVGL_PATH}/lvgl/src/misc
    ${LVGL_PATH}/lvgl/src/hal
    ${LVGL_PATH}/lvgl/src/draw
    ${LVGL_PATH}/lvgl/src/widgets
    ${LVGL_PATH}/lvgl/src/font
    ${LVGL_PATH}/lvgl/src/extra
    ${LVGL_PATH}/lvgl/examples/get_started
    ${LV_CONF_INCLUDE_SIMPLE}
)

target_compile_definitions(lvgl PUBLIC LV_LVGL_H_INCLUDE_SIMPLE)
