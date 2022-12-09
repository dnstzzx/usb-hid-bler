#pragma once

void print_hex_dump(const char *name,uint8_t *buffer, int len);


// for little endian
inline void bin2hex(char *dest, uint8_t *src, size_t length){
    const uint8_t table[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    for(int i=0;i<length;i++){
        *(uint16_t *)(dest + (i << 1)) = (table[(src[i] & 0xF0) >> 4] ) | (table[src[i] & 0x0F] << 8);
    }
    dest[length * 2] = '\0';
}

int hex2bin(char* input, uint8_t *output);