#include "stdlib.h"
#include "string.h"
#include "cJSON.h"
#include "macro.h"
#include "utils.h"
#include "storage.h"

#define MOUSE_MACRO_NS "mouse_macro"

saved_list_t *mouse_macros;

void macro_init(){
    mouse_macros = (saved_list_t *)malloc(sizeof(saved_list_t));
    saved_list_create(mouse_macros, MOUSE_MACRO_NS, offsetof(mouse_macro_t, version), sizeof(mouse_macro_t) - offsetof(mouse_macro_t, version));
    saved_list_load(mouse_macros, NULL);
}

/*
typedef struct{
    saved_list_head_t head;
    uint8_t     version;
    uint8_t     trigger_buttons_mask;
    uint8_t    cancel_input_report;
    uint8_t    output_model;
    uint8_t     action_type;
    uint16_t    action_delay;       // in ms
    uint16_t    report_duration;    // for toggle type anction, in ms

    union{
        standard_mouse_report_t mouse_output_report;
    };
}mouse_macro_t;
*/

cJSON *mouse_macro_to_json(mouse_macro_t *macro){
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

mouse_macro_t * mouse_macro_from_json(cJSON *json_obj, mouse_macro_t *macro_out){
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

void set_mouse_macro(mouse_macro_t *macro){
    mouse_macro_t *target = get_macro_by_name(macro->head.name);
    if(target != NULL){     // 同名鼠标宏已经存在
        macro->head.prev = target->head.prev;
        macro->head.next = target->head.next;
        memcpy(target, macro, sizeof(mouse_macro_t));
        saved_list_item_changed(&target->head, mouse_macros);
    }else{
        mouse_macro_t* new_macro = malloc(sizeof(mouse_macro_t));
        memcpy(new_macro, macro, sizeof(mouse_macro_t));
        saved_list_add_tail(&new_macro->head, mouse_macros);
    }
    saved_list_commit(mouse_macros);
    
}

bool delete_mouse_macro(const char *macro_name){
    mouse_macro_t *target = get_macro_by_name(macro_name);
    if(target == NULL)  return false;
    saved_list_delete_item(&target->head, mouse_macros);
    saved_list_commit(mouse_macros);
    return true;
}
