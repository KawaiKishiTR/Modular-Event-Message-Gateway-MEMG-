#ifndef MEMG_SOCKET_H
#define MEMG_SOCKET_H

#define MAX_EVENTS 64

int create_server_socket(const char* ip_str, int port);
void run_eventloop(int server_fd);

#endif // MEMG_SOCKET_H