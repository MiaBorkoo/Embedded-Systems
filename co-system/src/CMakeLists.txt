# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS
        "main.c"
        "sensor_task/sensor.c"
        "emergency_state/emergency.c"
        "door_task/door.c"
        "buzzer_task/buzzer.c"
        "communication/wifi_manager.c"
        "communication/mqtt_handler.c"
        "communication/agent_task.c"
        "communication/ring_buffer.c"
        "communication/crc8.c"
        "communication/protocol.c"
        "communication/protocol_types.h"
        "fsm/fsm.c"
        "stats/stats.c"


    INCLUDE_DIRS
        "../include"
        "../config"
        "."
        "communication"
        "sensor_task"
        "emergency_state"
        "door_task"
        "buzzer_task"
        "fsm"
)
