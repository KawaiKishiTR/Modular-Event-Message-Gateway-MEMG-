#include "socket/socket.h"



int _safe_close(int fd) {
    fprintf(stderr, "Closing Socket: (%d)\n", fd);
    close(fd);
    return -1;
}

int create_socket(const char* ip_str, int port) {
    int server_fd;
    int sockopt = 1;
    SA address;
    
    // socket oluşturma
    server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }
    
    // SO_REUSEADDR ile anlık program kapanıp açılmasında program çökmesi önlenir
    // program kapandığında çekirdek soketi bir süre daha rezerve eder bu sebeple reuse kullanarak bekleme faslını geçiyoruz
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt)) < 0) {
        perror("setsockopt");
        return _safe_close(server_fd);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family  = AF_INET;
    address.sin_port    = htons(port); // Parametre port

    // IP yapılandırması: NULL veya "0.0.0.0" gelirse tüm arayüzleri dinler
    if (ip_str == NULL || strcmp(ip_str, "0.0.0.0") == 0) {
        address.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, ip_str, &address.sin_addr) <= 0) {
            fprintf(stderr, "Gecersiz IP adresi formati: %s\n", ip_str);
            return _safe_close(server_fd);
        }
    }

    if (bind(server_fd, (SA*)&address, sizeof(address)) < 0) {
        perror("bind");
        return _safe_close(server_fd);
    }

    // Soketi pasif dinleyici durumuna getir (Backlog: 128 bağlantı kuyruğu)
    if (listen(server_fd, 128) < 0) {
        perror("listen");
        return _safe_close(server_fd);
    }

    return server_fd;
}

int accept_conneciton(int server_fd) {
    SA client_addr;
    socklen_t addrlen = sizeof(client_addr);

    // accepting client
    int client_fd = accept4(server_fd, (struct sockaddr*)&client_addr, &addrlen, SOCK_NONBLOCK);
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("accept4 hatasi");
        }
        return -1;
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
    printf("[+] Yeni Baglanti: %s:%d (fd: %d)\n", ip_str, ntohs(client_addr.sin_port), client_fd);

    return client_fd;
}



void init_buffers() {
    for (int c = 0; c < BUFFER_COUNT - 1; c++) {
        buffers[c].next = &buffers[c + 1];
    }
    buffers[BUFFER_COUNT - 1].next = NULL;
    buffer_head = &buffers[0];
    is_buffers_init = true;
}

char* buffer_alloc() {
    if (!is_buffers_init) {
        init_buffers();
    }

    pthread_mutex_lock(&pool_lock); // lock.acquire()

    // Havuzda boş blok yoksa, biri buffer_free yapana kadar CPU harcamadan UYU
    while (buffer_head == NULL) {
        pthread_cond_wait(&pool_cond, &pool_lock);
        // pthread_cond_wait kilidi geçici olarak serbest bırakır ve thread'i uyutur.
        // Uyandığında kilidi otomatik olarak tekrar alır.
    }

    buffer_t* toReturn = buffer_head;
    buffer_head = buffer_head->next;

    pthread_mutex_unlock(&pool_lock); // lock.release()
    return (char*)toReturn;
}

void buffer_free(char* buffer) {
    if (buffer == NULL) return;
    if (!is_buffers_init) {
        init_buffers();
    }

    pthread_mutex_lock(&pool_lock); // lock.acquire()

    buffer_t* buf = (buffer_t*)buffer;
    buf->next = buffer_head;
    buffer_head = buf;

    // Uyuyan bir worker thread varsa onu uyandır (Signal)
    pthread_cond_signal(&pool_cond);

    pthread_mutex_unlock(&pool_lock); // lock.release()
}

int register_client_to_epoll(int epoll_fd, int client_fd) {
    struct epoll_event ev;
    // EPOLLIN: Okunacak veri var
    // EPOLLET: Edge-Triggered mod (yüksek performans için)
    // EPOLLRDHUP: İstemci bağlantıyı kapattığında algıla
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.data.fd = client_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
        perror("epollctl: client_fd eklenemedi");
        return _safe_close(client_fd);
    }
    return 0;
}

void dummy_reader(int current_fd, int epoll_fd) {
    ssize_t res;
    char* buf = buffer_alloc();

    while (1) {
        res = read(current_fd, buf, BUFFER_SIZE);
        if (res < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("error while read");
            buffer_free(buf); // hata durumunda kaynağı iade at
            return;
        } else if (res == 0) {
            // İstemci bağlantıyı kapattı (EOF)
            printf("[-] Istemci cikis yapti (fd: %d)\n", current_fd);
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
            _safe_close(current_fd);
            buffer_free(buf); // istemci çıkınca kaynağı iade et
            return;
        }
        printf("Readed %zd bytes from (fd: %d)\n", res, current_fd);
    }

    // Okuma ve paket işleme tamamlandığında bloğu havuza geri ver
    buffer_free(buf);
}

void run_eventloop(int server_fd) {
    // çekirdek epoll instance oluştur
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        return;
    }

    // server_fd yi dinleyici olarak epoll a kaydet
    struct epoll_event ev;
    ev.events = EPOLLIN; // yeni bağlantı geldiğinde çalışır
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
        perror("epoll_ctl: server_fd eklenemedi");
        close(epoll_fd);
        return;
    }

    struct epoll_event events[MAX_EVENTS];
    printf("[*] Olay Dongusu (Event Loop) Baslatildi...\n");

    while (1) {
        // çekirdek olay bildirene kadar uyu
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue; // bir sinyal ile kesildiyse devam et
            perror("epoll_wait");
            break;
        }

        for (int c = 0; c < nfds; c++) {
            int current_fd = events[c].data.fd;
            uint32_t current_events = events[c].events;

            //DURUM 1: server_fd üzerinde olay var -> yeni istemci bağlandı!
            if (current_fd == server_fd) {
                while (1) {
                    // kabul kuyruğundaki bağlantıyı al
                    int client_fd = accept_conneciton(server_fd);
                    if (client_fd < 0) {
                        // liste boşaldı yada hata oldu, çık
                        break;
                    }
                    // yeni istemciyi epoll izleme listesine ekle
                    register_client_to_epoll(epoll_fd, client_fd);
                }
            }

            //DURUM 2: Bağlantı koptu yada hata oluştu
            else if (current_events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                printf("[-] Baglanti koptu (fd: %d)\n", current_fd);
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                _safe_close(current_fd);
            }

            //DURUM 3: mevcut bir istemciden veri geldi (EPOLLIN)
            else if (current_events & EPOLLIN) {
                printf("Istemciden veri geldi (fd: %d)\n", current_fd);
                // burada soketten json verisi okunacak
                dummy_reader(current_fd, epoll_fd);

                /* #PAYATTENTION:
                DURUM 3'e geçtiğinde tek bir read() çağrısı yaparsan ve gelen veri tamponundan (buffer) büyükse, 
                geriye kalan veriyi okumak için epoll_wait seni tekrar uyandırmaz. Soket kilitlenir (deadlock).

                Kural: EPOLLET kullanıyorsan, read() çağrısını bir döngüde errno == EAGAIN veya errno == EWOULDBLOCK 
                hatası alana kadar art arda çağırmak zorundasın.
                */
            }
        }
    }
    _safe_close(epoll_fd);
}

