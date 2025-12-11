# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS 
        "main.c"
        "sensor_task/sensor.c"
        "sensor_task/sensor.h"
        "emergency_state/emergency.c"
        "emergency_state/emergency.h"
        "door_task/door.c"
        "door_task/door.h"
        "buzzer_task/buzzer.c"
        "buzzer_task/buzzer.h"
        "agent_task.c"
        "communication/wifi_manager.c"
        "communication/mqtt_handler.c"
        "ring_buffer.c"

    INCLUDE_DIRS
        "../include"
        "."
)
