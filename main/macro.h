#pragma once
#include <stdint.h>
#include "cJSON.h"
#include "translate.h"
#include "storage.h"

#define MAX_MOUSE_MACRO_COUNT 16
#define MAX_KEYBOARD_MACRO_COUNT 16

typedef enum{
    MACRO_MODEL_MOUSE
}macro_output_model_t;

// action only once for a series of trigger
#define MACRO_ACTION_TYPE_ONCE   (0)
// action as long as trigged
#define MACRO_ACTION_TYPE_KEEP   (1) 
// toggle a report timer
#define MACRO_ACRION_TYPE_TOGGLE (2)

typedef struct{
    saved_list_head_t head;
    uint8_t     version;
    uint8_t     trigger_buttons_mask;
    uint8_t    cancel_input_report;
    macro_output_model_t    output_model;
    uint8_t     action_type;
    uint16_t    action_delay;       // in ms
    uint16_t    report_duration;    // for toggle type anction, in ms

    union{
        standard_mouse_report_t mouse_output_report;
    };
    
}mouse_macro_t;

extern saved_list_t *mouse_macros;
void macro_init();

inline mouse_macro_t *get_macro_by_name(const char *name){
    return (mouse_macro_t *)saved_list_search(mouse_macros, name);
}

cJSON *mouse_macro_to_json(mouse_macro_t *macro);
mouse_macro_t * mouse_macro_from_json(cJSON *json_obj, mouse_macro_t *macro_out);
void set_mouse_macro(mouse_macro_t *macro);
bool delete_mouse_macro(const char *macro_name);