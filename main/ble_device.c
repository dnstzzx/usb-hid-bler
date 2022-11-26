/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_hidd.h"
#include "esp_hid_gap.h"
#include "blink.h"
#include "report_map.h"
#include "manager.h"

static const char *TAG = "HID_DEV_DEMO";
volatile uint8_t ble_connected = 0;

typedef struct
{
    TaskHandle_t task_hdl;
    esp_hidd_dev_t *hid_dev;
    uint8_t protocol_mode;
    uint8_t *buffer;
} local_param_t;

static local_param_t s_ble_hid_param = {0};

void ble_send(size_t mapid, size_t report_id, uint8_t *data, uint8_t length)
{
    if(ble_connected){
        esp_err_t err = esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, mapid, report_id, data, length);
        if(err != ESP_OK)   printf("ble send failed, error code:%d\n", err);
    }else{
        printf("ignore data because ble is not connected\n");
    }
}

static void ble_hidd_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;
    static const char *TAG = "HID_BLE";

    switch (event) {
    case ESP_HIDD_START_EVENT: {
        ESP_LOGI(TAG, "START");
        esp_hid_ble_gap_adv_start();
        set_blink(1);
        break;
    }
    case ESP_HIDD_CONNECT_EVENT: {
        ESP_LOGI(TAG, "CONNECT");
        ble_connected = 1;
        set_blink(0);
        //ble_hid_task_start_up();//todo: this should be on auth_complete (in GAP)

        break;
    }
    case ESP_HIDD_PROTOCOL_MODE_EVENT: {
        ESP_LOGI(TAG, "PROTOCOL MODE[%u]: %s", param->protocol_mode.map_index, param->protocol_mode.protocol_mode ? "REPORT" : "BOOT");
        break;
    }
    case ESP_HIDD_CONTROL_EVENT: {
        ESP_LOGI(TAG, "CONTROL[%u]: %sSUSPEND", param->control.map_index, param->control.control ? "EXIT_" : "");
        break;
    }
    case ESP_HIDD_OUTPUT_EVENT: {
        ESP_LOGI(TAG, "OUTPUT[%u]: %8s ID: %2u, Len: %d, Data:", param->output.map_index, esp_hid_usage_str(param->output.usage), param->output.report_id, param->output.length);
        ESP_LOG_BUFFER_HEX(TAG, param->output.data, param->output.length);
        if(param->output.map_index == 0 && param->output.report_id == MANAGER_OUTPUT_REPORT_ID){
            on_manager_output(param->output.data, param->output.length);
        }
        break;
    }
    case ESP_HIDD_FEATURE_EVENT: {
        ESP_LOGI(TAG, "FEATURE[%u]: %8s ID: %2u, Len: %d, Data:", param->feature.map_index, esp_hid_usage_str(param->feature.usage), param->feature.report_id, param->feature.length);
        ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
        break;
    }
    case ESP_HIDD_DISCONNECT_EVENT: {
        ESP_LOGI(TAG, "DISCONNECT: %s", esp_hid_disconnect_reason_str(esp_hidd_dev_transport_get(param->disconnect.dev), param->disconnect.reason));
        //ble_hid_task_shut_down();
        ble_connected = 0;
        esp_hid_ble_gap_adv_start();
        set_blink(1);
        break;
    }
    case ESP_HIDD_STOP_EVENT: {
        ESP_LOGI(TAG, "STOP");
        break;
    }
    default:
        break;
    }
    return;
}

static esp_hid_raw_report_map_t ble_report_maps[STORE_MAP_COUNT + PREDEFINED_MAP_COUNT];
static esp_hid_device_config_t ble_hid_config = {
    .vendor_id          = 0x6666,
    .product_id         = 0x6666,
    .version            = 0x6666,
    .device_name        = "USB HID BLER",
    .manufacturer_name  = "Dnstzzx",
    .serial_number      = "666666666",
    .report_maps        = ble_report_maps,
    .report_maps_len    = 0
};

void ble_main(void)
{
    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );


    ble_hid_config.report_maps_len = map_info_table.map_count + PREDEFINED_MAP_COUNT;
    printf("registered %d maps for ble\n", ble_hid_config.report_maps_len);
    ble_report_maps[0].data = manager_report_desc;
    ble_report_maps[0].len = manager_report_desc_length;
    ble_report_maps[1].data = standard_mouse_report_desc;
    ble_report_maps[1].len = standard_mouse_report_desc_length;

    for(int i=0; i<map_info_table.map_count;i++){
        esp_hid_raw_report_map_t *ble_map = &ble_report_maps[i + PREDEFINED_MAP_COUNT];
        ble_map->len = map_info_table.indexes[i].length;
        ble_map->data = saved_maps[i];
    }

    ESP_LOGI(TAG, "setting hid gap, mode:%d", HID_DEV_MODE);
    ret = esp_hid_gap_init(HID_DEV_MODE);
    ESP_ERROR_CHECK( ret );
    ret = esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_GENERIC, ble_hid_config.device_name);
    ESP_ERROR_CHECK( ret );

    if ((ret = esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler)) != ESP_OK) {
        ESP_LOGE(TAG, "GATTS register callback failed: %d", ret);
        return;
    }
    ESP_LOGI(TAG, "setting ble device");
    ESP_ERROR_CHECK(
        esp_hidd_dev_init(&ble_hid_config, ESP_HID_TRANSPORT_BLE, ble_hidd_event_callback, &s_ble_hid_param.hid_dev));

}
