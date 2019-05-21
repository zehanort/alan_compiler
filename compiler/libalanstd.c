/**************************************************
 * C implementation of ALAN std library functions *
***************************************************/

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

void writeInteger(int32_t num) {
    printf("%" PRId32, num);
}

void writeByte(uint8_t byte) {
    printf("%" PRIu8, byte);
}

void writeChar(uint8_t ch) {
    printf("%c", ch);
}

void writeString(uint8_t *str) {
    printf("%s", str);
}
