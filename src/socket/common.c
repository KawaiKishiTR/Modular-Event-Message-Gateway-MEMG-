#include "socket/common.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

static buffer_t buffer_pool[BUFFER_COUNT];
static buffer_t* buffer_head = NULL;

static pthread_mutex_t pool_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  pool_cond = PTHREAD_COND_INITIALIZER;
static bool is_pool_init = false;

int safe_close(int fd) {
    if (fd >= 0) {
        fprintf(stderr, "Closing Socket: (%d)\n", fd);
        close(fd);
    }
    return -1;
}

void buffer_pool_init(void) {
    pthread_mutex_lock(&pool_lock);
    if (!is_pool_init) {
        for (size_t i = 0; i < BUFFER_COUNT - 1; i++) {
            buffer_pool[i].next = &buffer_pool[i + 1];
        }
        buffer_pool[BUFFER_COUNT - 1].next = NULL;
        buffer_head = &buffer_pool[0];
        is_pool_init = true;
    }
    pthread_mutex_unlock(&pool_lock);
}

char* buffer_alloc(void) {
    pthread_mutex_lock(&pool_lock);
    
    // Lazy init kontrolü kilit içinde yapılır (Thread-safe)
    if (!is_pool_init) {
        for (size_t i = 0; i < BUFFER_COUNT - 1; i++) {
            buffer_pool[i].next = &buffer_pool[i + 1];
        }
        buffer_pool[BUFFER_COUNT - 1].next = NULL;
        buffer_head = &buffer_pool[0];
        is_pool_init = true;
    }

    while (buffer_head == NULL) {
        pthread_cond_wait(&pool_cond, &pool_lock);
    }

    buffer_t* selected = buffer_head;
    buffer_head = buffer_head->next;

    pthread_mutex_unlock(&pool_lock);
    return (char*)selected;
}

void buffer_free(char* buf) {
    if (buf == NULL) return;

    pthread_mutex_lock(&pool_lock);

    buffer_t* block = (buffer_t*)buf;
    block->next = buffer_head;
    buffer_head = block;

    pthread_cond_signal(&pool_cond);

    pthread_mutex_unlock(&pool_lock);
}