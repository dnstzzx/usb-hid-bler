#include <stdio.h>
#include "esp_random.h"

void print_hex_dump(const char *name,uint8_t *buffer,int len)
{
    if (buffer == NULL || len <= 0)
    {
        return;
    }
    printf("%s: [", name);
    for (size_t i = 0; i < len; i++)
    {
        printf("%.2X ", *(buffer + i));
    }
    printf("]\n");
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