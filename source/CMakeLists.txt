target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/main.c")


add_subdirectory(board)
add_subdirectory(app/logger)
add_subdirectory(app/mqttClient)
add_subdirectory(app/dns)