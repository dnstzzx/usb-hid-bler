set(include_dirs ".")

idf_component_register(SRCS "esp_hid_gap.c" "usb_host.c" "peripherals.c" "macro.c" "report_map.c" "usb_main.c" "ble_device.c" "utils.c" "main.c" "manager.c" "storage.c"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES esp_hid
                       REQUIRES nvs_flash
                       REQUIRES hid_translater
                       REQUIRES json
                       REQUIRES esp_adc_cal)
