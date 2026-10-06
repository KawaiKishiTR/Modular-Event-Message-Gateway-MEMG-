#include "memg_ipc/config.hpp"
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>

namespace memg {

inline std::string  ENV_FILE;
inline std::string  CACHE_FILE;
inline std::string  ROOT_ENV_FILE = "/etc/memg/endpoints.env";

bool Registry::ensure_directory(const std::string& file_path) {
    size_t last_slash = file_path.find_last_of('/');
    if (last_slash == std::string::npos) return false;
    
    std::string dir = file_path.substr(0, last_slash);
    struct stat st{};
    if (stat(dir.c_str(), &st) != 0) {
        // Dizin yoksa 0777 ile aç (UDS izinlerini sonra chmod ile sıkılaştıracağız)
        return mkdir(dir.c_str(), 0777) == 0;
    }
    return true;
}

bool parse_env_file(const std::string& token, std::ifstream& file, std::string& out_value) {
    std::string line;
    std::string toReturn;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue; // boşsa geç
        size_t sep = line.find('=');
        if (sep == std::string::npos) continue; // '=' sembolü yoksa geç

        std::string key     = line.substr(0, sep);
        std::string remain  = line.substr(sep + 1);
        if (key != token) continue; // key aradığımız ile eşleşmiyorsa geç

        sep = remain.find('#');
        if (sep == std::string::npos) {toReturn.assign(remain); break;} // yorum yoksa sonucu dön
        std::string val = remain.substr(0, sep); // yorumu temizle
        toReturn.assign(val);
        break;
    }

    if (toReturn.empty()) return false;
    out_value.assign(toReturn);
    return true;
}

bool is_have_home() {
    char* home = std::getenv("HOME");
    return (home && home[0] != '\0');
}

std::string build_envkey(std::string token, std::string suffix) {
    return ("MEMG_" + token + suffix);
}

std::string sanitize_token(std::string token) {
    std::string result(token);
    std::replace(result.begin(), result.end(), '.', '_');
    return result;
}

std::string Registry::get_env_file() {
    if (!ENV_FILE.empty()) return ENV_FILE;
    std::string toReturn;

    do {
    char* result;
    result = std::getenv("MEMG_ENV_FILE");
    if (result && result[0] != '\0') {
        toReturn.assign(result);
        break;
    }

    result = std::getenv("HOME");
    if (result && result[0] != '\0') {
        toReturn.assign(std::string(result) + "/.config/memg/endpoints.env");
        break;
    }

    toReturn.assign(ROOT_ENV_FILE);
    } while(0);

    ENV_FILE.assign(toReturn);
    return ENV_FILE;
}

std::string Registry::get_cache_file(const std::string& my_token) {
    if (!CACHE_FILE.empty()) return CACHE_FILE;
    std::string sanitized_token(sanitize_token(my_token));
    std::string ENV_KEY(build_envkey(sanitized_token, "_CACHE_FILE"));

    std::string toReturn;

    do {
    char* result;
    result = std::getenv(ENV_KEY.c_str());
    if (result && result[0] != '\0') {
        toReturn.assign(result);
        break;
    }

    result = std::getenv("HOME");
    if (result && result[0] != '\0') {
        toReturn.assign(std::string(result) + "/.cache/memg/" + sanitized_token + "/dump.cache");
        break;
    }

    if (geteuid() == 0) {
        toReturn.assign("/tmp/memg/" + sanitized_token + "/dump.cache");
        break;
    }
    } while(0);

    if (toReturn.empty()) {
        throw std::runtime_error("cache_file location cant resolved");
    }

    CACHE_FILE.assign(toReturn);
    return CACHE_FILE;
}

std::string Registry::get_socket_file(const std::string& token) {
    std::string sanitized_token(sanitize_token(token));
    std::string ENV_KEY(build_envkey(sanitized_token, "_SOCK"));

    std::string toReturn;

    do {
    char* result;
    result = std::getenv(ENV_KEY.c_str());
    if (result && result[0] != '\0') {
        toReturn.assign(result);
        break;
    }

    std::ifstream env_file;
    env_file.open(get_env_file());
    if (parse_env_file(ENV_KEY, env_file, toReturn)) {
        break;
    }

    env_file.close();
    env_file.open(ROOT_ENV_FILE);
    if (parse_env_file(ENV_KEY, env_file, toReturn)) {
        break;
    }
    } while(0);

    if (toReturn.empty()) {
        throw std::runtime_error("sock_file location cant resolved: " + sanitized_token + "(" + token + ")");
    }

    return toReturn;
}

} // namespace memg