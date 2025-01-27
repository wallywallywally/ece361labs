#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int check_arguments(const int count) {
    if (count != 3) {
        printf("Wrong number of arguments, use 'deliver <server address> <server port number>'\n");
        return 0;
    }
    return 1;
}

void get_filename(char *filename) {
    char dummy[BUFFER_SIZE];
    printf("Choose a file for FTP by replying with 'ftp <file name>'");
    scanf("%s %s", dummy, filename);

    // Handle invalid inputs
    while (strcmp(dummy, "ftp") != 0) {
        printf("Wrong format, please reply with format 'ftp <file name>'");
        scanf("%s %s", dummy, filename);
    }
}

int check_file_exists(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp != NULL) {
        printf("%s exists\n", filename);
        fclose(fp);
        return 1;
    } else {
        printf("%s does not exist, exiting...", filename);
        return 0;
    }
}

int main(int argc, char* argv[]) {		// argv[1] for server address, argv[2] for server port
    if (!check_arguments(argc)) {
        exit(0);
    }

    char filename[BUFFER_SIZE];
    get_filename(filename);
    if (!check_file_exists(filename)) {
        exit(1);
    }

    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;

    // Get server info
    //struct addrinfo *server_info = NULL;
    //int status = 0;
    //if ((status = getaddrinfo(argv[1], argv[2], &hints, &server_info)) != 0) {
    //    printf("getaddrinfo() failed\n");
    //    printf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    //    exit(0);
    //};

    // Create socket
    int sockfd;
    if ((sockfd = socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol)) == -1) {
        printf("Socket error, exiting...");
        exit(1);
    };

    // Setup recipient (server)
    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));
    if (inet_aton(argv[1] , &server.sin_addr) == 0) {
        printf("Error in setting up server, exiting...");
        exit(1);
    };

    // Send message
    const char *msg_ftp = "ftp";
    if (sendto(sockfd, msg_ftp, strlen(msg_ftp), 0, (struct sockaddr *) &server, sizeof(server)) < 0) {
        printf("Error in sending FTP message to server, exiting...");
        exit(1);
    };

    // Receive message
    char msg_received[BUFFER_SIZE];
    struct sockaddr_storage server_addr;
    socklen_t addr_len = sizeof(server_addr);
    ssize_t bytes_recv;
    // Blocked here waiting for response...
    if ((bytes_recv = recvfrom(sockfd, msg_received, BUFFER_SIZE, 0, (struct sockaddr*) &server_addr, &addr_len)) == -1) {
        printf("Error in receiving confirmation message from server, exiting...");
    };

    if (bytes_recv < 0) {
        printf("No message received, exiting...");
        close(sockfd);
        exit(0);
    }

    char *success_msg = "yes";
    if (strstr(msg_received, success_msg) != NULL) {
        printf("A file transfer can start!\n");
    }

    close(sockfd);
    return 0;
}