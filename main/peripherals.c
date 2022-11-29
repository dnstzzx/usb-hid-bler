#include "driver/gpio.h"
#include "driver/adc.h"
#include "soc/adc_channel.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include "board.h"

// led
volatile uint8_t blink = 0;
void set_blink(uint8_t enable)
{
    if(enable){
        printf("blink started\n");
    }else{
        printf("blink stopped\n");
    }
    blink = enable;
}


#ifdef BOARD_HAS_LED

void blink_idle(void *param)
{
    uint8_t state = 0;
    while(1){
        if(blink){
            state = !state;
            gpio_set_level(BOARD_PIN_LED, state);
        }else{
            if(state == 0){
                state = 1;
                gpio_set_level(BOARD_PIN_LED, 1);
            }
        }
        vTaskDelay(1500 / portTICK_PERIOD_MS);
    }
}
#endif

// battery
int  battery_voltage;
#ifdef BOARD_HAS_BATTARY

// esp32c3 only
#define ADC_CHANNEL BOARD_PIN_BATTARY
#define MULTISAMLE_COUNT 16
#define MEASURE_INTERVEL_MS 1000 

esp_adc_cal_characteristics_t adc_char;
bool battery_adc_calibration_enable = false;

void read_battery_adc(void *param){
    while(1){
        int sum = 0;
        uint8_t success = 1;
        for(int i=0;i<MULTISAMLE_COUNT;i++){
            int input = adc1_get_raw(ADC_CHANNEL);
            if(input == -1){
                printf("battery votage measure failed\n");
                success = 0;
            }else{
                sum += input;
            }
        }
        if(!success){
            continue;
        }
        sum /= MULTISAMLE_COUNT;
        battery_voltage = esp_adc_cal_raw_to_voltage(sum, &adc_char) * BOARD_BATTARY_VOLTAGE_SCALE;
        vTaskDelay(MEASURE_INTERVEL_MS / portTICK_PERIOD_MS);
    }
}

#endif

void peripherals_init()
{
    
    // led
    #ifdef BOARD_HAS_LED
    #define LED_PIN_BIT_MASK (1ULL << BOARD_PIN_LED)
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = LED_PIN_BIT_MASK,
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);
    xTaskCreatePinnedToCore(blink_idle, "blink", 512, NULL, 1, NULL, 0);
    #endif

    // battery
    #ifdef BOARD_HAS_BATTARY
    adc1_config_width(ADC_WIDTH_12Bit);
    adc_channel_t channel = ADC_CHANNEL;
    adc1_config_channel_atten(channel, ADC_ATTEN_11db);
    
    esp_err_t ret;
    const char *TAG = "battery adc calibration";
    ret = esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP);
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "Calibration scheme not supported, skip software calibration");
    } else if (ret == ESP_ERR_INVALID_VERSION) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else if (ret == ESP_OK) {
        battery_adc_calibration_enable = true;
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_11db, ADC_WIDTH_12Bit, 0, &adc_char);
    } else {
        ESP_LOGE(TAG, "Invalid arg");
    }
    xTaskCreatePinnedToCore(read_battery_adc, "battery", 2048, NULL, 1, NULL, 0);

    #endif
}