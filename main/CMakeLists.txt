set(srcs "esp_hid_device_main.c"
         "esp_hid_gap.c"
         "usb_host.c"
         "usb_test.c"
         "bridge.c"
         "blink.c"
         "report_map.c")
set(include_dirs ".")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES esp_hid
                       REQUIRES nvs_flash)
