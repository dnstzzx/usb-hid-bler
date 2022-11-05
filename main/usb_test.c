#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "hal/timer_ll.h"


#include "freertos/queue.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "driver/gpio.h"
#include "soc/soc.h"
#include "soc/periph_defs.h"
#include "esp_log.h"

#include "usb_host.h"
#include "bridge.h"

#define xQueueHandle QueueHandle_t


struct USBMessage
{
	uint8_t src;
	uint8_t len;
	uint8_t  data[0x8];
};

struct new_usb_report_map_info{
    sUsbContStruct *usb;
    uint8_t hidid;
};

struct install_status_t{
    enum Process_Mode mode;
    int mapid;
};

struct install_status_t install_statuses[NUM_USB][4];  // install_statuses[usbid][hidid]

static xQueueHandle usb_mess_Que = NULL;
static xQueueHandle new_device_Que = NULL;

void IRAM_ATTR timer_group0_isr(void *para)
{
	timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);
	//taskENTER_CRITICAL();
	usb_process();
	//taskEXIT_CRITICAL();
	timer_group_enable_alarm_in_isr(TIMER_GROUP_0, TIMER_0);
}

void usbMess(uint8_t src,uint8_t len,uint8_t *data)
{
	struct  USBMessage msg;
	msg.src = src;
	msg.len = len<0x8?len:0x8;
	for(int k=0;k<msg.len;k++)
	{
		msg.data[k] = data[k];
	}
	xQueueSend(usb_mess_Que, ( void * ) &msg,(TickType_t)0);
}

void on_device_in(sUsbContStruct *usb){
    xQueueSendFromISR(new_device_Que, &usb, 0);
}

void stop_usb_timer();

void handle_new_device(void *vparam){
    sUsbContStruct *usb;
    while (1){
        if(xQueueReceive(new_device_Que, &usb, 500 / portTICK_PERIOD_MS)){
            printf("new device %4x:%4x on port %d,%d hid report maps detected\n",usb->desc.idVendor, usb->desc.idProduct, usb->selfNum, usb->hid_report_desc_count);
            
            int saving_indexes[4];
            size_t saving_count = 0;
            for(int i=0; i<usb->hid_report_desc_count; i++){
                uint8_t *report_map = usb->hid_report_desc_buffer[i];
                size_t length = usb->hid[i].wDescriptorLength;

                int index = check_report_map(report_map, length);
                if(index == -1){
                    index = add_report_map(report_map, length);
                    printf("found new report map on usb%d hid%d ", usb->selfNum, i);
                    print_hex_dump("", report_map, length);
                    if(index == -1){
                        printf("report map table is full, aborted\n");
                        break;
                    }
                    printf("now saved at index %d\n", index);
                    saving_indexes[saving_count] = index;
                    saving_count ++;
                }else{
                    install_statuses[usb->selfNum][i].mapid = index;
                    install_statuses[usb->selfNum][i].mode = PASSTHROUGH;

                    printf("found new report map on usb%d hid%d ", usb->selfNum, i);
                    print_hex_dump("", report_map, length);
                    printf("matched index %d\n", index);
                }
            }

            if(saving_count != 0){
                stop_usb_timer();
                for(int i=0;i<saving_count;i++){
                    save_report_map(saving_indexes[i]);
                }
                save_map_index();
                printf("new report map and index saved, restarting...\n");
                esp_restart();
            }
        }
    }
}

void usb_recv()
{
	struct USBMessage msg;
    static uint8_t msg_data[8];
    if(xQueueReceive(usb_mess_Que, &msg, 0)){
        uint8_t hidid = msg.src % 4;
        uint8_t usbid = msg.src >> 2;
        uint8_t length = msg.len;
        struct install_status_t *status = &install_statuses[usbid][hidid];
        if(status->mode == NONE){
            return;
        }
        memcpy(msg_data, msg.data, length);
        
        //data_transform(msg.data, msg.len);
        ble_send(status->mapid, msg_data, length);
        printf("msg from usb%d/hid%d", usbid, hidid);
        print_hex_dump(" ", msg_data, length);

	    //printf("\nfrom: %02x len: %02x data:",msg.src,msg.len);
	    //int unum= msg.src / 4;
        /*
        for(int k=0;k<msg.len;k++){
		   printf("%02x ",msg.data[k]);
	    }*/
	}
}
void test_delay1() {__asm__ (" nop");  }
void test_delay2() {__asm__ (" nop"); __asm__ (" nop"); }
void test_delay3() {__asm__ (" nop"); __asm__ (" nop"); __asm__ (" nop"); }
void stest()
{
    const char *test = "I am some test content to put in the heap";
    char buf[64];
    memset(buf, 0xEE, 64);
    strlcpy(buf, test, 64);

    char *a = malloc(64);
    memcpy(a, buf, 64);
    // move data from 'a' to IRAM
    char *b = heap_caps_realloc(a, 64, MALLOC_CAP_EXEC);
    printf("b=%p\n",b);
    printf("a=%p\n",a);	
    //~ assert((a-b)!=0);
    //~ assert(heap_caps_check_integrity(MALLOC_CAP_INVALID, true));
    //TEST_ASSERT_EQUAL_HEX32_ARRAY(buf, b, 64 / sizeof(uint32_t));

    // Move data back to DRAM
    char *c = heap_caps_realloc(b, 48, MALLOC_CAP_8BIT);
    printf("c=%p\n",c);	
    //~ TEST_ASSERT_NOT_NULL(c);
    //~ TEST_ASSERT_NOT_EQUAL(b, c);
    //~ TEST_ASSERT(heap_caps_check_integrity(MALLOC_CAP_INVALID, true));
    //~ TEST_ASSERT_EQUAL_HEX8_ARRAY(buf, c, 48);

    free(c);
}


#define DP_P   3
#define DM_P  4
#define DP_P1  5
#define DM_P1  6
#define DP_P2  -1
#define DM_P2  -1
#define DP_P3  -1
#define DM_P3  -1


int64_t get_system_time_us() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000000LL + (tv.tv_usec));
}


void setDelay(uint8_t ticks);

void start_usb_timer(){
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = 1,
    }; // default clock source is APB
    setDelay(4);
    stest();
    usb_mess_Que  = xQueueCreate(10,sizeof(struct USBMessage));
    initStates(DP_P,DM_P,DP_P1,DM_P1,DP_P2,DM_P2,DP_P3,DM_P3);
    //  initStates(DP_P,DM_P,-1,-1,-1,-1,-1,-1);

    

    int timer_idx = TIMER_0;
    double timer_interval_sec = TIMER_INTERVAL0_SEC;

    timer_init(TIMER_GROUP_0, timer_idx, &config);
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    //timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr,(void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr,(void *) timer_idx, 0, NULL);  
    timer_start(TIMER_GROUP_0, timer_idx);
}

void stop_usb_timer(){
    timer_pause(TIMER_GROUP_0, TIMER_0);
}

void usb_task(void *pvParameter)
{
    start_usb_timer();
    while(1)
    {
	    usb_recv();
	    printState();   
	    vTaskDelay(10 / portTICK_PERIOD_MS);
    };
}




void usb_main()
{   
    new_device_Que = xQueueCreate(10, sizeof(sUsbContStruct *));

    xTaskCreate(&handle_new_device, "install_device", 4096, NULL, 0, NULL);
    xTaskCreatePinnedToCore(&usb_task, "usb", 4096, NULL, 5, NULL, 0);
    
}
