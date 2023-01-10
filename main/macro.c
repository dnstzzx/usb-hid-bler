#include "stdlib.h"
#include "string.h"
#include "cJSON.h"
#include "macro.h"
#include "utils.h"
#include "storage.h"
#include "ble_device.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/timer.h"
#include "esp_timer.h"


#define MOUSE_MACRO_NS "macros"
#define MACRO_RUN_INTERVAL_MS (1)
#define TIMER_GROUP TIMER_GROUP_1
#define TIMER_ID    TIMER_0
#define TIMER_DIVIDER 16
#define TIMER_SCALE   (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds

saved_list_t *macros;
static volatile size_t current_tick = 0;
static volatile bool macro_enabled = true;

static QueueHandle_t out_mouse_report_queue;
struct send_report_from_queue_task_param{
    QueueHandle_t queue;
    size_t msg_length;
    size_t report_map_id;
    size_t report_id;
};

static saved_list_head_t *_alloc_loaded_macro(size_t l);
static void run_macros(void *);
static void send_out_from_queue(struct send_report_from_queue_task_param *param);

void macro_init(){
    macros = (saved_list_t *)malloc(sizeof(saved_list_t));
    saved_list_create(macros, MOUSE_MACRO_NS, offsetof(macro_t, version), offsetof(macro_t, context) - offsetof(macro_t, version));
    saved_list_load(macros, _alloc_loaded_macro);

    out_mouse_report_queue = xQueueCreate(64, sizeof(standard_mouse_report_t) - 1);     // report id is excluded
    struct send_report_from_queue_task_param *param = malloc(sizeof(struct send_report_from_queue_task_param));
    param->queue            =   out_mouse_report_queue;
    param->msg_length       =   sizeof(standard_mouse_report_t) - 1;    // report id is excluded
    param->report_map_id    =   0;
    param->report_id        =   1;
    xTaskCreate((void (*)(void *))send_out_from_queue, "macro_out", 2048, param, 0, NULL);

    timer_config_t config = {
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = true,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .divider = TIMER_DIVIDER,
        .intr_type = TIMER_INTR_LEVEL,
    };
    ESP_ERROR_CHECK(timer_init(TIMER_GROUP, TIMER_ID, &config));
    ESP_ERROR_CHECK(timer_set_alarm_value(TIMER_GROUP, TIMER_ID, MACRO_RUN_INTERVAL_MS * TIMER_SCALE / 1000));
    ESP_ERROR_CHECK(timer_enable_intr(TIMER_GROUP, TIMER_ID));
    ESP_ERROR_CHECK(timer_isr_register(TIMER_GROUP, TIMER_ID, run_macros, NULL, 0, NULL)); 
    enable_macros();
}

/* --------------------  macro management ------------------------- */

static saved_list_head_t *_alloc_loaded_macro(size_t l){
    if(l != offsetof(macro_t, context) - offsetof(macro_t, version)){
        printf("warning: loaded a macro with incorrect length %d\n", l);
    }
    macro_t *macro = malloc(sizeof(macro_t));
    memset(&macro->context, 0, sizeof(macro->context));
    return &macro->head;
}

cJSON *macro_to_json(macro_t *macro){
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "name", macro->head.name);
    cJSON_AddNumberToObject(json, "version", macro->version);
    cJSON_AddNumberToObject(json, "trigger_buttons_mask", macro->trigger_buttons_mask);
    cJSON_AddBoolToObject(json, "cancel_input_report", macro->cancel_input_report);
    cJSON_AddNumberToObject(json, "output_model", macro->output_model);
    cJSON_AddNumberToObject(json, "action_type", macro->action_type);
    cJSON_AddNumberToObject(json, "action_delay", macro->action_delay);
    cJSON_AddNumberToObject(json, "report_duration", macro->report_duration);
    if(macro->output_model == MACRO_MODEL_MOUSE){
        char buff[sizeof(standard_mouse_report_t) * 2 + 1];
        bin2hex(buff, (uint8_t *)&macro->mouse_output_report, sizeof(standard_mouse_report_t));
        cJSON_AddStringToObject(json, "mouse_output_report", buff);
    }
    return json;
}

macro_t * macro_from_json(cJSON *json_obj, macro_t *macro_out){
    char *name = cJSON_GetObjectItem(json_obj, "name")->valuestring;
    memcpy(&macro_out->head.name, name, sizeof(macro_out->head.name));
    macro_out->head.name[sizeof(macro_out->head.name) - 1] = '\0';
    macro_out->version = cJSON_GetObjectItem(json_obj, "version")->valueint;
    macro_out->trigger_buttons_mask = cJSON_GetObjectItem(json_obj, "trigger_buttons_mask")->valueint;
    macro_out->cancel_input_report = cJSON_IsTrue(cJSON_GetObjectItem(json_obj, "cancel_input_report"));
    macro_out->output_model = cJSON_GetObjectItem(json_obj, "output_model")->valueint;
    macro_out->action_type = cJSON_GetObjectItem(json_obj, "action_type")->valueint;
    macro_out->action_delay = cJSON_GetObjectItem(json_obj, "action_delay")->valueint;
    macro_out->report_duration = cJSON_GetObjectItem(json_obj, "report_duration")->valueint;
    char *report = cJSON_GetObjectItem(json_obj, "mouse_output_report")->valuestring;
    if(strlen(report) != sizeof(standard_mouse_report_t) * 2){
        return NULL;
    }
    if(!hex2bin(report, (uint8_t *)&macro_out->mouse_output_report)){
        return NULL;
    }
    return macro_out;
}

void set_macro(macro_t *macro){
    macro_enabled = false;
    macro_t *target = get_macro_by_name(macro->head.name);
    if(target != NULL){     // 同名宏已经存在
        macro->head.prev = target->head.prev;
        macro->head.next = target->head.next;
        memcpy(target, macro, sizeof(macro_t));
        saved_list_item_changed(&target->head, macros);
    }else{
        macro_t* new_macro = malloc(sizeof(macro_t));
        memcpy(new_macro, macro, sizeof(macro_t));
        saved_list_add_tail(&new_macro->head, macros);
    }
    saved_list_commit(macros);
    macro_enabled = true;
}

bool delete_macro(const char *macro_name){
    macro_enabled = false;
    macro_t *target = get_macro_by_name(macro_name);
    if(target == NULL)  return false;
    saved_list_delete_item(&target->head, macros);
    saved_list_commit(macros);
    free(target);
    macro_enabled = true;
    return true;
}

/* --------------------  report ouput queue ------------------------- */

static void send_out_from_queue(struct send_report_from_queue_task_param *param){
    uint8_t msg[param->msg_length];
    while(1){
        if(xQueueReceive(param->queue, &msg, 500 / portTICK_PERIOD_MS)){
            ble_send(param->report_map_id, param->report_id, (uint8_t *)&msg, param->msg_length);
            print_hex_dump("sending macro report", msg, sizeof(msg));
        }
    }
}

static void send_out_to_queue(macro_t *macro){
    if(macro->output_model == MACRO_MODEL_MOUSE){
        xQueueSendFromISR(out_mouse_report_queue, ((uint8_t *)&macro->mouse_output_report) + 1, false);
    }
}

/* --------------------  macro executing ------------------------- */

static inline void perform_macro(macro_t *macro){
    // 暂未加入触发延迟支持
    macro_context_t *context = &macro->context;
    switch (macro->action_type){
        case MACRO_ACTION_TYPE_ONCE:
            if(context->triggered && !context->once.last_triggered){  // 相当于triggered上升沿触发
                send_out_to_queue(macro);
            }
            context->once.last_triggered = context->triggered;
            break;
        case MACRO_ACTION_TYPE_KEEP:
            if(context->triggered){
                if(current_tick - context->keep.last_out_tick >= macro->report_duration){
                    send_out_to_queue(macro);
                    context->keep.last_out_tick = current_tick;
                }
            }
            break;
        case MACRO_ACRION_TYPE_TOGGLE:
            if(context->triggered && !context->toggle.last_triggered){  // 相当于triggered上升沿触发
                context->toggle.on = !context->toggle.on;
            }
            if(context->toggle.on && current_tick - context->toggle.last_out_tick >= macro->report_duration){
                send_out_to_queue(macro);
                context->toggle.last_out_tick = current_tick;
            }
            context->toggle.last_triggered = context->triggered;
            break;
        default:
            break;
    }

}

static void run_macros(void *param){
    timer_group_clr_intr_status_in_isr(TIMER_GROUP, TIMER_ID);
    current_tick += MACRO_RUN_INTERVAL_MS;
    if(macro_enabled){
        macro_t *macro;
        list_for_each_entry(macro, (saved_list_head_t *)macros, head){
            perform_macro(macro);
        }
    }
    timer_group_enable_alarm_in_isr(TIMER_GROUP, TIMER_ID);
}

void enable_macros(){
    current_tick = 0;
    ESP_ERROR_CHECK(timer_start(TIMER_GROUP, TIMER_ID));
}

void disable_macros(){
    ESP_ERROR_CHECK(timer_pause(TIMER_GROUP, TIMER_ID));
}

/* --------------------  macro input ------------------------- */

/*
  returns: whether cancel the input
*/
bool macro_handle_mouse_input(standard_mouse_report_t *report){
    macro_t *macro;
    bool cancel = false;
    list_for_each_entry(macro, (saved_list_head_t *)macros, head){
        if(macro->input_model != MACRO_MODEL_MOUSE) continue;
        macro->context.triggered = (macro->trigger_buttons_mask & report->buttons) == macro->trigger_buttons_mask;
        cancel |= (macro->context.triggered && macro->cancel_input_report);
    }
    return cancel;
}
