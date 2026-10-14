#include "memg_ipc/epoll.hpp"
#include <unistd.h>
#include <cerrno>
#include <system_error>

namespace memg {

Epoll::Epoll(EpollConfig* cfg) 
    :   _cfg(cfg), 
        _events(cfg->max_events) {
    _epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (_epoll_fd < 0) {
        throw std::system_error(errno, std::generic_category(), "[Epoll/ERROR] [Constructor] epoll_create1 başarısız");
    }
}

Epoll::~Epoll() {
    if (_epoll_fd >= 0) close(_epoll_fd);
}

Epoll::Epoll(Epoll&& other) noexcept 
    : _epoll_fd(other._epoll_fd), _events(std::move(other._events)), _cfg(other._cfg) {
    other._epoll_fd = -1;
}

Epoll& Epoll::operator=(Epoll&& other) noexcept {
    if (this != &other) {
        if (_epoll_fd >= 0) close(_epoll_fd);
        _epoll_fd = other._epoll_fd;
        _events = std::move(other._events);
        _cfg = other._cfg;
        other._epoll_fd = -1;
    }
    return *this;
}

void Epoll::add(int fd, uint32_t events) {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        throw std::system_error(errno, std::generic_category(), "[Epoll/ERROR] [add] epoll add hatası");
    }
}

void Epoll::modify(int fd, uint32_t events) {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        throw std::system_error(errno, std::generic_category(), "[Epoll/ERROR] [modify] epoll mod hatası");
    }
}

void Epoll::remove(int fd) {
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        if (errno != ENOENT && errno != EBADF) {
            throw std::system_error(errno, std::generic_category(), "[Epoll/ERROR] [remove] epoll del hatası");
        }
    }
}

int Epoll::wait() {
    while (true) {
        int nfds = epoll_wait(_epoll_fd, _events.data(), (int)_events.size(), _cfg->timeout_ms);
        if (nfds >= 0) return nfds; // Tetiklenen olay sayısı
        if (errno == EINTR) continue; // Kesme sinyali gelirse tekrar bekle
        throw std::system_error(errno, std::generic_category(), "[Epoll/ERROR] [wait] epoll_wait hatası");
    }
}

} // namespace memg