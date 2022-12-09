#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int hex2bin(char* input, uint8_t *output) {
    if (!input || !output) return 0;

    size_t input_len = strlen(input);
    if (input_len % 2 != 0) return 0;

    char* p = input;
    size_t output_len = 0;

    while (*p && *(p + 1)) {
        char high = *p;
        char low = *(p + 1);

        int high_value = 0;
        if (high >= '0' && high <= '9') high_value = high - '0';
        else if (high >= 'a' && high <= 'f') high_value = high - 'a' + 10;
        else if (high >= 'A' && high <= 'F') high_value = high - 'A' + 10;
        else return 0;

        int low_value = 0;
        if (low >= '0' && low <= '9') low_value = low - '0';
        else if (low >= 'a' && low <= 'f') low_value = low - 'a' + 10;
        else if (low >= 'A' && low <= 'F') low_value = low - 'A' + 10;
        else return 0;

        output[output_len++] = high_value << 4 | low_value;
        p += 2;
    }

    return output_len;
}
