#pragma once

#include <string>
#include <cstdint>

namespace memg {

struct SocketConfig {
    bool        is_server{true};
    bool        set_nonblocking{true};
    mode_t      permissions{0666};
    std::string token;
};

struct EpollConfig {
    int         timeout_ms{16};         // -1: Sonsuz uyku, >0: Periyodik tick (animasyon/timer)
    uint32_t    max_events{64};
};

struct ServiceConfig {
    EpollConfig*    epoll;
    SocketConfig*   socket;
};

class Registry {
public:

    // Servis soketinin oluşturulacağı ana dizini (/tmp/memg veya $XDG_RUNTIME_DIR/memg) garanti eder
    static bool ensure_directory(const std::string& file_path);

    static std::string get_socket_file(const std::string& token);
    static std::string get_cache_file (const std::string& token);
    static std::string get_env_file   ();
};

} // namespace memg