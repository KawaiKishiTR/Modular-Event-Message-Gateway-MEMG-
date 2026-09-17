cmake_minimum_required(VERSION 3.16)
project(memg_ipc VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Kütüphane kaynakları
add_library(memg_ipc SHARED
    src/memg_ipc/config.cpp
    src/memg_ipc/epoll.cpp
    src/memg_ipc/socket.cpp
    src/memg_ipc/node.cpp
)

target_include_directories(memg_ipc PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>
    $<INSTALL_INTERFACE:include>
)

# 2. Test Executable'ları
add_executable(test_server src/test_server.cpp)
target_link_libraries(test_server PRIVATE memg_ipc)

add_executable(test_client src/test_client.cpp)
target_link_libraries(test_client PRIVATE memg_ipc)

# Kurulum hedefleri (sudo make install için)
install(TARGETS memg_ipc LIBRARY DESTINATION /usr/local/lib)
install(DIRECTORY src/include/memg_ipc DESTINATION /usr/local/include)