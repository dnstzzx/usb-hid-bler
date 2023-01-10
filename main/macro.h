#pragma once
#include <stdint.h>
#include "cJSON.h"
#include "translate.h"
#include "storage.h"

enum{
    MACRO_MODEL_MOUSE
};
typedef uint8_t macro_model_t;

enum{
    MACRO_ACTION_TYPE_ONCE,     // action only once for a series of trigger
    MACRO_ACTION_TYPE_KEEP,     // action as long as trigged
    MACRO_ACRION_TYPE_TOGGLE    // toggle a report timer
};
typedef uint8_t macro_action_type_t;

typedef struct{
    bool triggered;
    union{
        struct{
            bool last_triggered;
        } once;
        struct{
            size_t last_out_tick;
        } keep;
        struct{
            unsigned on: 1;
            unsigned last_triggered: 1;
            size_t last_out_tick;
        } toggle;
    };  
    
}macro_context_t;

typedef struct{
    saved_list_head_t head;
    // save zone start
    uint8_t            version;

    //trigger
    bool               cancel_input_report;
    macro_model_t      input_model;
    union{
        struct{ // for mouse
            uint8_t     trigger_buttons_mask;
        };
    };

    // action
    macro_model_t           output_model;
    macro_action_type_t     action_type;
    uint16_t                action_delay;       // in ms
    uint16_t                report_duration;    // in ms
    union{
        standard_mouse_report_t mouse_output_report;
    };

    // save zone end
    macro_context_t context;
}macro_t;
extern saved_list_t *macros;
void macro_init();
void enable_macros();
void disable_macros();

inline macro_t *get_macro_by_name(const char *name){
    return (macro_t *)saved_list_search(macros, name);
}

cJSON *macro_to_json(macro_t *macro);
macro_t * macro_from_json(cJSON *json_obj, macro_t *macro_out);
void set_macro(macro_t *macro);
bool delete_macro(const char *macro_name);

/*
  returns: whether cancel the input
*/
bool macro_handle_mouse_input(standard_mouse_report_t *report);