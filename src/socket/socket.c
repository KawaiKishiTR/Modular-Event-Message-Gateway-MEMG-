#define _GNU_SOURCE
#include "socket/socket.h"
#include "socket/common.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int accept_connection(int server_fd)
{
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

    int client_fd = accept4(server_fd, (struct sockaddr *)&client_addr, &addrlen, SOCK_NONBLOCK);
    if (client_fd < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            perror("accept4");
        }
        return -1;
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
    printf("[+] Yeni Baglanti: %s:%d (fd: %d)\n", ip_str, ntohs(client_addr.sin_port), client_fd);

    return client_fd;
}

static int register_client_to_epoll(int epoll_fd, int client_fd)
{
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.data.fd = client_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0)
    {
        perror("epoll_ctl: client_fd eklenemedi");
        return safe_close(client_fd);
    }
    return 0;
}

static void handle_client_read(int current_fd, int epoll_fd)
{
    char *buf = buffer_alloc();

    while (1)
    {
        ssize_t res = read(current_fd, buf, BUFFER_SIZE);
        if (res < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            perror("read hatasi");
            buffer_free(buf);
            return;
        }
        else if (res == 0)
        {
            printf("[-] Istemci cikis yapti (fd: %d)\n", current_fd);
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
            safe_close(current_fd);
            buffer_free(buf);
            return;
        }

        printf("Readed %zd bytes from (fd: %d)\n", res, current_fd);
        // Burada ileride JSON framing & parsing mantığı çalışacak
    }

    buffer_free(buf);
}

int create_server_socket(const char *ip_str, int port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return -1;
    }

    int sockopt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt)) < 0)
    {
        perror("setsockopt");
        return safe_close(server_fd);
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (ip_str == NULL || strcmp(ip_str, "0.0.0.0") == 0)
    {
        address.sin_addr.s_addr = INADDR_ANY;
    }
    else
    {
        if (inet_pton(AF_INET, ip_str, &address.sin_addr) <= 0)
        {
            fprintf(stderr, "Gecersiz IP adresi: %s\n", ip_str);
            return safe_close(server_fd);
        }
    }

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind");
        return safe_close(server_fd);
    }

    if (listen(server_fd, 128) < 0)
    {
        perror("listen");
        return safe_close(server_fd);
    }

    return server_fd;
}

void run_eventloop(int server_fd)
{
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0)
    {
        perror("epoll_create1");
        return;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0)
    {
        perror("epoll_ctl: server_fd");
        close(epoll_fd);
        return;
    }

    struct epoll_event events[MAX_EVENTS];
    printf("[*] Olay Dongusu (Event Loop) Baslatildi...\n");

    while (1)
    {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0)
        {
            if (errno == EINTR)
                continue;
            perror("epoll_wait");
            break;
        }

        for (int c = 0; c < nfds; c++)
        {
            int current_fd = events[c].data.fd;
            uint32_t current_events = events[c].events;

            if (current_fd == server_fd)
            {
                while (1)
                {
                    int client_fd = accept_connection(server_fd);
                    if (client_fd < 0)
                        break;
                    register_client_to_epoll(epoll_fd, client_fd);
                }
            }
            else if (current_events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                printf("[-] Baglanti koptu (fd: %d)\n", current_fd);
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                safe_close(current_fd);
            }
            else if (current_events & EPOLLIN)
            {
                printf("Istemciden veri geldi (fd: %d)\n", current_fd);
                handle_client_read(current_fd, epoll_fd);
            }
        }
    }

    safe_close(epoll_fd);
}