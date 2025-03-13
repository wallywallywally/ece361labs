#include <stdio.h>
#include <sys/socket.h>
#include <pthread.h>

int main(void) {
    printf("Hello, World!\n");
    return 0;
}

struct addrinfo hints = {0};
hints.ai_family = AF_INET;          // IPv4
hints.ai_socktype = SOCK_DGRAM;     // Connectionless datagram
hints.ai_protocol = 0;              // IPPROTO_UDP
// Create socket
int sockfd;
if ((sockfd = socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol)) == -1) {
    printf("Socket error, exiting...\n");
    exit(1);
}