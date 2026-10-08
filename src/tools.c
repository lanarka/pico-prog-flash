#include <stdio.h>
#include "pico/stdlib.h"

void print_span() {
    for (int i=0;i<45;++i) {
        printf(".");
        sleep_ms(25);
    }
    printf("\n");
}

void print_buf(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        printf("%02x", buf[i]);
        if (i % 16 == 15)
            printf("\n");
        else
            printf(" ");
    }
    printf("\n");
}
