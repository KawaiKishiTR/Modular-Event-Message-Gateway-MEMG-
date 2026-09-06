#ifndef MEMG_COMMON_H
#define MEMG_COMMON_H

#include <stddef.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024
#define BUFFER_COUNT 16

typedef union buffer {
    union buffer* next;
    char value[BUFFER_SIZE];
} buffer_t;

// Soket kapatma yardımcısı
int safe_close(int fd);

// Bellek havuzu yönetimi (Memory Pool API)
void buffer_pool_init(void);
char* buffer_alloc(void);
void buffer_free(char* buf);

#endif // MEMG_COMMON_H