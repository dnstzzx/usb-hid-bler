#include "storage.h"
#include "string.h"

void storage_init(){
    ESP_ERROR_CHECK(nvs_flash_init_partition(STORAGE_NVS_PARTITION_NAME));
}

nvs_handle_t storage_open(const char *ns){
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open_from_partition(STORAGE_NVS_PARTITION_NAME, ns, NVS_READWRITE, &handle));
    return handle;
}


#define data_of(head,offset) (((void *)head) + offset)

static inline void __list_add(saved_list_head_t *new, saved_list_head_t *prev, saved_list_head_t *next){
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

size_t _saved_list_load(saved_list_head_t *list_out, int save_offset, nvs_handle_t handle,\
                         const char *ns, saved_list_head_t *(*alloc_new_item)(size_t)){
    nvs_iterator_t iter = nvs_entry_find(STORAGE_NVS_PARTITION_NAME, ns, NVS_TYPE_BLOB);
    size_t item_count = 0;
    while(iter != NULL){
        nvs_entry_info_t info;
        nvs_entry_info(iter, &info);
        size_t length;
        nvs_get_blob(handle, info.key, NULL, &length);
        saved_list_head_t *item_head;
        if(alloc_new_item == NULL){
            item_head = (saved_list_head_t *)malloc(save_offset + length);
        }else{
            item_head = alloc_new_item(length);
        }
        nvs_get_blob(handle, info.key, data_of(item_head, save_offset), &length);
        strcpy(item_head->name, info.key);
        __list_add(item_head, list_out->prev, list_out);
        item_count ++;
        iter = nvs_entry_next(iter);
    }
    return item_count;
}

void _saved_list_add(saved_list_head_t *node, saved_list_head_t* list, int save_offset, size_t save_length, nvs_handle_t handle){
    __list_add(node, list, list->next);
    _saved_list_save_item(node, list, save_offset, save_length, handle);
}
void _saved_list_add_tail(saved_list_head_t *node, saved_list_head_t* list, int save_offset, size_t save_length, nvs_handle_t handle){
    __list_add(node, list->prev, list);
    _saved_list_save_item(node, list, save_offset, save_length, handle);
}
void _saved_list_del(saved_list_head_t *node, saved_list_head_t* list, nvs_handle_t handle){
    node->next->prev = node->prev;
    node->prev->next = node->next;
    node->prev = node->next = (saved_list_head_t *)0;
    nvs_erase_key(handle, node->name);
}

