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
        "fsm/fsm.c"

    INCLUDE_DIRS
        "../include"
        "."
        "communication"
        "sensor_task"
        "emergency_state"
        "door_task"
        "buzzer_task"
        "fsm"
)
