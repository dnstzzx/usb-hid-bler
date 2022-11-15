#include "report_map.h"
#include "nvs_flash.h"
#include <string.h>

#include "utils.h"

const char *map_index_partition = "report_maps";
const char *map_index_ns = "maps";
const char *map_content_partition = "report_maps";
const char *map_content_ns = "maps";

Map_Info_Table map_info_table;
Report_Map_BUFF saved_maps[STORE_MAP_COUNT];

void load_report_maps();
static size_t load_map(char *map_id, Report_Map_BUFF buff);

void init_map(){
    ESP_ERROR_CHECK(nvs_flash_init_partition(map_index_partition));
    /*
    if (!strcmp(map_index_partition, map_content_partition)){
        ESP_ERROR_CHECK(nvs_flash_init_partition(map_content_partition));
    }*/
    load_report_maps();

}

void load_report_maps(){
    nvs_handle_t handle;

    
    ESP_ERROR_CHECK(nvs_open_from_partition(map_index_partition, map_index_ns, NVS_READWRITE, &handle));
    size_t read_length = sizeof(map_info_table);

    esp_err_t err = nvs_get_blob(handle, "index", &map_info_table, &read_length);
    nvs_close(handle);
    
    if(err == ESP_ERR_NVS_NOT_FOUND){
        printf("No report map index found, creating a new one\n");
        map_info_table.map_count = 0;
        save_map_info_table();
    }else{
        ESP_ERROR_CHECK(err);
        printf("map index loaded,now load %d report maps\n", map_info_table.map_count);
    }
    char map_id[16];
    for(int i=0;i<map_info_table.map_count;i++){
        sprintf(map_id, "map%d", i);
        load_map(map_id, saved_maps[i]);
        printf("map %d loaded", i);
        print_hex_dump("", saved_maps[i], map_info_table.indexes[i].length);
    }
    printf("map index and saved report maps load completed\n");
}

void save_map_info_table(){
    nvs_handle_t handle;
    Map_Info_Table saving_table;
    memcpy(&saving_table, &map_info_table, sizeof(Map_Info_Table));
    nvs_open_from_partition(map_index_partition, map_index_ns, NVS_READWRITE, &handle);
    nvs_set_blob(handle, "index", (const char *)&saving_table, sizeof(Map_Info_Table));
    nvs_commit(handle);
    nvs_close(handle);
}

static size_t load_map(char *map_id, Report_Map_BUFF buff){
    nvs_handle_t handle;
    size_t length = sizeof(Report_Map_BUFF);

    ESP_ERROR_CHECK(nvs_open_from_partition(map_content_partition, map_content_ns, NVS_READONLY, &handle));
    ESP_ERROR_CHECK(nvs_get_blob(handle, (const char *)map_id, buff, &length));
    nvs_close(handle);
    return length;
}

size_t get_report_map(char *map_id, Report_Map_BUFF buff){
    return load_map(map_id, buff);
}

void save_report_map(int index){
    char map_id[16];
    sprintf(map_id, "map%d", index);

    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open_from_partition(map_content_partition, map_content_ns, NVS_READWRITE, &handle));
    ESP_ERROR_CHECK(nvs_set_blob(handle, (const char *)map_id, saved_maps[index], map_info_table.indexes[index].length));
    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
}


static uint32_t calc_report_map_sum(Report_Map_BUFF map, size_t map_length){
    uint32_t sum = 0;
    for(int i=0;i<map_length;i++){
        sum += map[i];
    }
    return sum;
}

/* 
    check whether a report map stored in nvs
    returns: report index if found else -1
*/
int check_report_map(Report_Map_BUFF map, size_t length){
    uint32_t sum = calc_report_map_sum(map, length);
    int report_index = -1;
    for(int i=0; i<map_info_table.map_count; i++){
        if(map_info_table.indexes[i].length == length && map_info_table.indexes[i].sum == sum){
            char same = 1;
            uint8_t *m = saved_maps[i];
            for(int j=0;j<length;j++){
                if(m[j] != map[j]){
                    same = 0;
                    break;
                }
            }
            if(same){
                report_index = i;
                break;
            }
        }
    }
    return report_index;
}


int add_report_map(uint8_t *map, size_t length){
    if(map_info_table.map_count == STORE_MAP_COUNT - 1){
        return -1;
    }

    int index = map_info_table.map_count;
    Map_Info_Item *info_item = &map_info_table.indexes[index];
    uint32_t sum = calc_report_map_sum(map, length);
    info_item->length = length;
    info_item->sum = sum;
    info_item->process_mode = PROCESS_MODE_NONE;
    
    memcpy(saved_maps[index], map, length);
    
    map_info_table.map_count ++;
    return index;
}
