#include "memg_ipc/config.hpp"
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>

namespace memg {

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

//resolve helper functions
bool resolve_enviroment     (const std::string& token, std::string& out_value);
bool resolve_config_file    (const std::string& token, std::string& out_value);
bool resolve_XDG_RUNTIME_DIR(const std::string& token, std::string& out_value);

// actual resolve function
std::string Registry::resolve(const std::string& token) {
    std::string toReturn;
    // 1. Ortam Değişkeni Kontrolü (Örn: MEMG_TOKEN_IO_MEMG_RGB=/custom/path.sock)
    if (resolve_enviroment(token, toReturn)) {
        return toReturn;
    }

    // 2. Config Dosyası Kontrolü (~/.config/memg/endpoints.conf)
    if (resolve_config_file(token, toReturn)) {
        return toReturn;
    }

    // 3. Fallback: XDG_RUNTIME_DIR veya /tmp/memg/
    if (resolve_XDG_RUNTIME_DIR(token, toReturn)) {
        return toReturn;
    }
    throw std::system_error(errno, std::generic_category(), "No matching resolve function can resolve the token: " + token);
}

bool resolve_enviroment(const std::string& token, std::string& out_value) {
    std::string env_var = "MEMG_TOKEN_" + token;
    std::replace(env_var.begin(), env_var.end(), '.', '_');
    const char* env_val = std::getenv(env_var.c_str());
    if (env_val && env_val[0] != '\0') {
        out_value.assign(env_val);
        return true;
    }
    return false;
}

bool resolve_config_file(const std::string& token, std::string& out_value) {
    const char* home = std::getenv("HOME");
    if (!home) return false;

    std::string toReturn;
    std::ifstream file(std::string(home) + "/.config/memg/endpoints.conf");
    
    if (!file.is_open()) return false;
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue; //boşsa yada tamamen yorumsa geç

        size_t sep = line.find('=');
        if (sep == std::string::npos) continue; // '=' sembolü yoksa geç

        std::string key = line.substr(0, sep);
        std::string remain = line.substr(sep + 1);
        if (key != token) continue; // key aradığımız key değilse geç

        size_t cmd = remain.find('#');
        if (cmd == std::string::npos) {toReturn.assign(remain); break;} // yoruma dair başka birşey yoksa sonucu dön
        std::string val = remain.substr(0, cmd); // yorumu temizle
        toReturn.assign(val);
        break; // sonucu dön
    }

    if (toReturn.empty()) return false;
    out_value.assign(toReturn);
    return true;
}

bool resolve_XDG_RUNTIME_DIR(const std::string& token, std::string& out_value) {
    // 3. Fallback: XDG_RUNTIME_DIR veya /tmp/memg/
    const char* runtime_dir = std::getenv("XDG_RUNTIME_DIR");
    std::string base_dir = (runtime_dir && runtime_dir[0] != '\0')
                           ? std::string(runtime_dir) + "/memg" 
                           : "/tmp/memg";

    // Token içindeki '.' karakterlerini '_' yapıp soket ismi üret
    std::string sanitized_token = token;
    std::replace(sanitized_token.begin(), sanitized_token.end(), '.', '_');

    out_value.assign(base_dir + "/" + sanitized_token + ".sock");
    return true;
}


} // namespace memg