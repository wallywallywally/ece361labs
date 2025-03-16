#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "packet.h"
#include "user.h"

#define BACKLOG 16

// Attempt to bind a socket to the specified port.
// If port is already in use, return false. Else, return true.
bool isPortAvailable(int port) {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
    	fprintf(stderr, "Error creating socket\n");

        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    // `htons()` converts integer from host byte order to network byte order (big endian)
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    	close(sockfd);
        return false;
    }

    close(sockfd);

    return true;
}

int main(int argc, char *argv[]) {
  	if (argc != 2) {
    	fprintf(stderr, "Usage: server <TCP port number to listen on>\n");
        return 1;
  	}

    int port = atoi(argv[1]);
    if (!isPortAvailable(port)) {
    	fprintf(stderr, "Port %d is not available\n", port);
        return -1;
    }

    printf("Port: %d is available. Starting server...\n", port);

    // Set up server
    int sockfd; 							// Socket file descriptor for the server
    int result; 							// Variable for return values

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));		// Clear out garbage values from hints

    hints.ai_family = AF_INET;				// Use IPv4 address family
    hints.ai_socktype = SOCK_STREAM;		// Specify that we want a stream socket for TCP
    hints.ai_protocol = IPPROTO_TCP;		// Specify that we want to use TCP
    hints.ai_flags = AI_PASSIVE; 			// Fill in IP for us

    result = getaddrinfo(NULL, argv[1], &hints, &res);
    if (result) {
    	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
    	sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
        	continue;						// Try the next address
        }

        result = bind(sockfd, p->ai_addr, p->ai_addrlen);
        if (result) {
        	close(sockfd); 					// Bind failed, try next address
            continue;
        }

        break;
    }

    freeaddrinfo(res);
    if (p == NULL) {
    	fprintf(stderr, "Unable to bind\n");
        return -1;
    }

    result = listen(sockfd, BACKLOG);
    if (result) {
    	fprintf(stderr, "Unable to listen\n");
    }

    printf("Waiting for connections...\n");

    while (true) {
    	User *newUser = malloc(sizeof(User));
        struct sockaddr peerAddr;
        socklen_t peerAddrLen = sizeof(peerAddr);
        newUser -> sockfd = accept(sockfd, &peerAddr, &peerAddrLen);
        if (newUser -> sockfd == -1) {
        	fprintf(stderr, "Error accepting connection\n");
            break;
        }

        // Extract information from new connection
        printf("Connection type: %s\n", peerAddr.sa_family == AF_INET ? "IPv4" : "IPv6");
        if (peerAddr.sa_family == AF_INET) {
        	struct sockaddr_in *addr = (struct sockaddr_in *) &peerAddr;
            int port = ntohs(addr->sin_port);

            char ipAddress[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr->sin_addr, ipAddress, INET_ADDRSTRLEN);
            printf("Connection from %s:%d\n", ipAddress, port);
        }
    }

    return 0;
};