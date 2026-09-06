#include "socket/socket.h"

int main() {

    int fd = create_server_socket("0.0.0.0", 8000);
    run_eventloop(fd);

    return 0;
}