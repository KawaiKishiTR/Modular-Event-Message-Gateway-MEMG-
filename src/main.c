#include "socket/socket.h"

int main() {

    int fd = create_socket("0.0.0.0", 8000);
    run_eventloop(fd);

    return 0;
}