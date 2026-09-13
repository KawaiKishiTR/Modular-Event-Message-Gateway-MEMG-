#pragma once

#include <sys/epoll.h>
#include <vector>

namespace memg {


class Epoll {
public:
    explicit Epoll(int max_events = 64);
    ~Epoll();

    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;
    Epoll(Epoll&& other) noexcept;
    Epoll& operator=(Epoll&& other) noexcept;

    void add(int fd, uint32_t events);
    void modify(int fd, uint32_t events);
    void remove(int fd);

    // Kaç adet event geldiğini döner (nfds). Hata olursa sistem hatası fırlatır.
    int wait(int timeout_ms = -1);

    // Gelen event'lere erişim:
    const epoll_event& get_event(int index) const { return _events[index]; }
    int get_fd() const noexcept { return _epoll_fd; }
private:
    int _epoll_fd{-1};
    std::vector<epoll_event> _events;
};

} // namespace memg