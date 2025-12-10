# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS 
        "main.c"
        "sensor_task.c"
        "alarm_task.c"
        "agent_task.c"
        "wifi_manager.c"
        "mqtt_handler.c"
        "ring_buffer.c"
    INCLUDE_DIRS 
        "../include"
)
