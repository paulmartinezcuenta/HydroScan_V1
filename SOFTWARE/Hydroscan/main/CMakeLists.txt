idf_component_register(
    SRCS
        "main.c"
        "tds.c"
        "ds18b20.c"
        "telemetry.c"
        "mpu6050_sensor.c"
        "attitude.c"
        "wave_processing.c"
        "spectrum.c"
        "wave_task.c"
        "modem.c"
        "gps.c"
        "firebase.c"

    INCLUDE_DIRS "."
)