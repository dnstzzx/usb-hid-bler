set(srcs
         "esp_hid_gap.c"
         "usb_host.c"
         "blink.c"
         "report_map.c")
set(include_dirs ".")

idf_component_register(SRCS "usb_main.c" "ble_device.c" "utils.c" "main.c" "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES esp_hid
                       REQUIRES nvs_flash
                       REQUIRES hid_translater)
