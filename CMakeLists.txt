cmake_minimum_required(VERSION 3.16)
project(memg_apis VERSION 1.0.0 LANGUAGES CXX)

# Header-only arayüz kütüphanesi (Interface Library)
add_library(memg_apis INTERFACE)

# Geliştirme esnasında ve kurulduktan sonra include yollarını ayarla
target_include_directories(memg_apis INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>
    $<INSTALL_INTERFACE:include>
)

# Sistem dizinine (/usr/local/include/memg_apis) kurulum kuralı
install(
    DIRECTORY src/include/memg_apis
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

# CMake entegrasyonu için hedefi dışa aktar (Export target)
install(
    TARGETS memg_apis
    EXPORT memg_apis-targets
)

install(
    EXPORT memg_apis-targets
    FILE memg_apis-config.cmake
    NAMESPACE memg::
    DESTINATION lib/cmake/memg_apis
)