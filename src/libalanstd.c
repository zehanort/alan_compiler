/* ---------------------------------------------------------------------
   ---------- C implementation of ALAN std library functions -----------
   --------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

// write functions
void writeInteger(int32_t n) {
    printf("%" PRId32, n);
}

void writeByte(uint8_t b) {
    printf("%" PRIu8, b);
}

void writeChar(uint8_t b) {
    printf("%c", b);
}

void writeString(uint8_t *s) {
    printf("%s", s);
}

// read functions
int32_t readInteger() {
    int32_t n;
    if (!scanf(" %" SCNd32, &n)) {
        fprintf(stderr, "readInteger: failed to read integer\n");
        exit(1);
    }
    return n;
}

uint8_t readByte() {
    uint8_t b;
    if (!scanf(" %" SCNu8, &b)) {
        fprintf(stderr, "readByte: failed to read byte\n");
        exit(1);
    }
    return b;
}

uint8_t readChar() {
    uint8_t b;
    if (!scanf(" %c", &b)) {
        fprintf(stderr, "readChar: failed to read char\n");
        exit(1);
    }
    return b;
}

void readString(int32_t n, uint8_t *s) {
    scanf(" ");
    int i;
    uint8_t c;
    for (i = 0; i < n; i++) {
        c = (uint8_t)getchar();
        if (c == '\n' || c == EOF) {
            *s = '\0';
            return;
        }
        *s++ = c;
    }
    *s = '\0';
    return;
}

// type casting functions
int32_t extend(uint8_t b) {
    return (int32_t)b;
}

uint8_t shrink(int32_t i) {
    return (uint8_t)(i & 0xFF);
}

// string manipulation functions
int32_t strlen(uint8_t *s) {
    int32_t len = 0;
    while (*s++ != '\0') len++;
    return len;
}

int32_t strcmp(uint8_t *s1, uint8_t *s2) {
    int32_t diff;
    while (*s1 != '\0' && *s2 != '\0') {
        diff = (*s1++ - *s2++);
        if (diff != 0) return diff;
    }
    if (*s1 == '\0' && *s2 == '\0') return 0;
    return ((*s1 == '\0') ? -(int32_t)*s2 : (int32_t)*s1);
}

void strcpy(uint8_t *trg, uint8_t *src) {
    // NOTE: if trg is not big enough -> undefined behavior
    while (*src != '\0') *trg++ = *src++;
    *trg = '\0';
    return;
}

void strcat(uint8_t *trg, uint8_t *src) {
  while (*trg++ != '\0');
  trg--;
  while (*src != '\0') *trg++ = *src++;
  *trg = '\0';
  return;
}
