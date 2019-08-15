idf_component_register(SRCS "main.c"
                            "openlux/web.c"
                            "openlux/common.c"
                            "openlux/sensors.c"
                            INCLUDE_DIRS "openlux")

# Create a spiffs image containing the html files and flash it to the ESP
spiffs_create_partition_image(webdata ../web FLASH_IN_PROJECT)
