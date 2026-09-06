#define _GNU_SOURCE
#include "socket/common.h"
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>      // fctnl fonksiyon kütüphanesi
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <arpa/inet.h>  // htons(), inet_pton() gibi dönüşüm fonksiyonları
#include <sys/epoll.h>
#include <sys/socket.h> // temel çekirdek socket(), bind() fonksiyonları
#include <netinet/in.h> // struct sockaddr_in, INADDR_ANY, IPPROTO_TCP

#define MAX_EVENTS 64
#define SA struct sockaddr_in
#define BUFFER_SIZE 1024
#define BUFFER_COUNT 16

typedef union buffer {
    union buffer* next;
    char value[BUFFER_SIZE];
} buffer_t;

static buffer_t buffers[BUFFER_COUNT];
static buffer_t* buffer_head = NULL;
static bool is_buffers_init = false;

// pythondaki threading.Lock()
static pthread_mutex_t pool_lock = PTHREAD_MUTEX_INITIALIZER;

// bellek boşaldığında bekleyen iş parçacıklarını uyandırmak için (Condition variable)
static pthread_cond_t pool_cond = PTHREAD_COND_INITIALIZER;

// function for closing socket
int _safe_close(int fd);

// cnonnection functions
int create_socket(const char* ip_str, int port);
int accept_conneciton(int server_fd);

// thread safe buffer functions
void init_buffers();
char* buffer_alloc();
void buffer_free(char* buffer);

// nonblocking epoll function
int register_client_to_epoll(int epoll_fd, int client_fd);
void dummy_reader(int current_fd, int epoll_fd);
void run_eventloop(int server_fd);
