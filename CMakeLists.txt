cmake_minimum_required(VERSION 3.16)
project(MEMG C)

# C standardını belirleme
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Kaynak kodları (.c) derleyerek bir statik kütüphane (static library) haline getirme
add_library(memg_socket STATIC
    src/socket/common.c
    src/socket/socket.c
)

add_executable(memg_main 
    src/main.c
)

# Header (.h) arama yollarını kütüphaneye bağlama (include directories)
target_include_directories(memg_socket PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
)

target_link_libraries(memg_main memg_socket)

# pthread kütüphanesini bağlama (thread kullanacaksanız)
find_package(Threads REQUIRED)
target_link_libraries(memg_socket PUBLIC Threads::Threads)