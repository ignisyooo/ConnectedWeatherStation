target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_sources(${PROJECT_NAME} PUBLIC 
                "${CMAKE_CURRENT_SOURCE_DIR}/httpSessionMgr.c"
                "${CMAKE_CURRENT_SOURCE_DIR}/httpClient.c"
                )

