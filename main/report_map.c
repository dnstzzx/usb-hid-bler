#include "report_map.h"
#include "nvs_flash.h"
#include <string.h>
#include "esp_random.h"
#include "bridge.h"

const char *map_index_partition = "report_maps";
const char *map_index_ns = "maps";
const char *map_content_partition = "report_maps";
const char *map_content_ns = "maps";

Map_Index map_index;
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
    size_t read_length = sizeof(map_index);

    esp_err_t err = nvs_get_blob(handle, "index", &map_index, &read_length);
    nvs_close(handle);
    
    if(err == ESP_ERR_NVS_NOT_FOUND){
        printf("No report map index found, creating a new one\n");
        map_index.map_count = 0;
        save_map_index();
    }else{
        ESP_ERROR_CHECK(err);
        printf("map index loaded,now load %d report maps\n", map_index.map_count);
    }
    char map_id[16];
    for(int i=0;i<map_index.map_count;i++){
        sprintf(map_id, "map%d", i);
        load_map(map_id, saved_maps[i]);
        printf("map %d loaded", i);
        print_hex_dump("", saved_maps[i], map_index.indexes[i].length);
    }
    printf("map index and saved report maps load completed\n");
}

void save_map_index(){
    nvs_handle_t handle;
    Map_Index saving_index;
    memcpy(&saving_index, &map_index, sizeof(Map_Index));
    nvs_open_from_partition(map_index_partition, map_index_ns, NVS_READWRITE, &handle);
    nvs_set_blob(handle, "index", (const char *)&saving_index, sizeof(Map_Index));
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
    ESP_ERROR_CHECK(nvs_set_blob(handle, (const char *)map_id, saved_maps[index], map_index.indexes[index].length));
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
    for(int i=0; i<map_index.map_count; i++){
        if(map_index.indexes[i].length == length && map_index.indexes[i].sum == sum){
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

static void random_id(char *id, size_t length){
    for(int i=0;i<length;i++){
        uint32_t rand = esp_random();
        if(rand % 4 == 0){
            rand = rand >> 2;
            id[i] = '0' + rand % 10;
        }else{
            rand = rand >> 2;
            id[i] = 'a' + rand % 26;
        }
    }
    id[length] = '\0';
    
}


int add_report_map(uint8_t *map, size_t length){
    if(map_index.map_count == STORE_MAP_COUNT - 1){
        return -1;
    }

    int index = map_index.map_count;
    Map_Index_Item *index_item = &map_index.indexes[index];
    uint32_t sum = calc_report_map_sum(map, length);
    index_item->length = length;
    index_item->sum = sum;
    index_item->process_mode = PASSTHROUGH;
    
    memcpy(saved_maps[index], map, length);
    
    map_index.map_count ++;
    return index;
}
