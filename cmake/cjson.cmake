
set(CJSON_PATH ${PROJECT_ROOT}/external/cJSON)


set(CJSON_INCLUDES
    ${CJSON_PATH}
)

set(CJON_SOURCE "${CJSON_PATH}/cJSON.c")

add_library(cjson STATIC)
target_include_directories(cjson PUBLIC ${CJSON_INCLUDES})
target_sources(cjson PRIVATE ${CJON_SOURCE})
