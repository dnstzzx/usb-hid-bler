#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>


#define LED_PIN 7
#define LED_PIN_BIT_MASK (1ULL << LED_PIN)

uint8_t blink = 0;

void set_blink(uint8_t enable)
{
    if(enable){
        printf("blink started\n");
    }else{
        printf("blink stopped\n");
    }
    
}

void blink_idle(void *param)
{
    uint8_t state = 0;
    while(1){
        if(blink){
            state = !state;
            gpio_set_level(LED_PIN, state);
        }else{
            if(state == 0){
                state = 1;
                gpio_set_level(LED_PIN, 1);
            }
        }
        vTaskDelay(1500 / portTICK_PERIOD_MS);
    }
}

void blink_init()
{

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = LED_PIN_BIT_MASK,
        .pull_down_en = 0,
        .pull_up_en = 0
    };

    gpio_config(&io_conf);

    xTaskCreatePinnedToCore(blink_idle, "blink", 512, NULL, 1, NULL, 0);
}