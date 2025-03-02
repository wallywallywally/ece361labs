#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#include "packet.h"

int check_arguments(const int count) {
    if (count != 3) {
        printf("Wrong number of arguments, use 'deliver <server address> <server port number>'\n");
        return 0;
    }
    return 1;
}

void get_filename(char *filename) {
    char dummy[BUFFER_SIZE];
    printf("Choose a file for FTP by replying with 'ftp <file name>'\n");
    scanf("%s %s", dummy, filename);

    // Handle invalid inputs
    while (strcmp(dummy, "ftp") != 0) {
        printf("Wrong format, please reply with format 'ftp <file name>'\n");
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
        printf("%s does not exist, exiting...\n", filename);
        return 0;
    }
}

void calculate_timeout(struct timeval *timeout, clock_t *estimated_RTT, const clock_t *sample_RTT, clock_t *dev_RTT) {
    // Exponential weight moving average: influence of past sample decreases exponentially fast
    *estimated_RTT = 0.875 * ((double) *estimated_RTT) + 0.125 * ((double) *sample_RTT);
    // EWMA of sample RTT deviation from estimated RTT
    *dev_RTT = 0.75 * ((double) *dev_RTT) + 0.25 * abs(*sample_RTT - *estimated_RTT);
    // Timeout is estimated RTT + safety margin
    timeout->tv_usec = *estimated_RTT + 4 * *dev_RTT;
}

void send_file(const char *filename, int sockfd, struct sockaddr_in server, clock_t start_RTT) {
    // Open file
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Error opening file %s\n", filename);
        exit(1);
    }

    // Determine # fragments required
    fseek(fp, 0, SEEK_END);                        // Set pointer to end of file
    int total_frag = ftell(fp) / DATA_SIZE + 1;
    printf("Total # fragments = %d\n", total_frag);
    rewind(fp);                                              // Reset pointer to start of file before packetising

    // Packetise file
    char **packets = malloc(sizeof(char *) * total_frag);
    for (int i = 1; i <= total_frag; i++) {
        packet p;
        p.total_frag = total_frag;
        p.frag_no = i;                                              // Packets are 1-indexed
        p.filename = filename;
        memset(p.filedata, 0, DATA_SIZE);                       // Clear packet data
        fread(p.filedata, sizeof(char), DATA_SIZE, fp);       // Read data from file to buffer
        p.size = i != total_frag ? DATA_SIZE : ftell(fp) % 1000;

        // Convert packet to message
        packets[i - 1] = malloc(sizeof(char) * BUFFER_SIZE);
        prepare_packet_msg(p, packets[i - 1]);
    }

    // Setup connection timeout
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 999999;
    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        // SOL_SOCKET: for specified socket only, not protocol ; SO_RCVTIMEO: timeout for recvfrom
        printf("Error setting initial timeout\n");
        exit(1);
    }

    // Set initial values for timeout calculation
    clock_t estimated_RTT = start_RTT;
    clock_t dev_RTT = start_RTT / 2;
    clock_t sample_RTT, start, end;

    // Execute file transfer
    socklen_t addr_len = sizeof(server);
    packet ack_pkt;
    char recv_buf[BUFFER_SIZE];
    for (int i = 1; i <= total_frag; i++) {
        //print_debug(packets[i - 1], BUFFER_SIZE);
        // Send packet
        start = clock();
        if (sendto(sockfd, packets[i - 1], BUFFER_SIZE, 0, (struct sockaddr *) &server, sizeof(server)) < 0) {
            printf("Error sending packet %d out of %d to server\n", i, total_frag);
            exit(1);
        };

        // Receive acknowledgement
        memset(recv_buf, 0, BUFFER_SIZE);
        if (recvfrom(sockfd, recv_buf, BUFFER_SIZE, 0, (struct sockaddr *) &server, &addr_len) == -1) {
            // LAB 3 - RETRANSMISSION
            // Timeout occurred
            printf("Timeout (%.2lds) in receiving ACK for packet %d out of %d from server\n", timeout.tv_usec * 1000000, i, total_frag);
            end = clock();

            // Update timeout value
            sample_RTT = end - start;
            calculate_timeout(&timeout, &estimated_RTT, &sample_RTT, &dev_RTT);
            if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
                printf("Error updating timeout\n");
                exit(1);
            }
            printf("Timeout value updated to %.2lds\n", timeout.tv_usec * 1000000);

            // Retransmit
            printf("Retransmitting packet %d out of %d\n", i, total_frag);
            i--;
            continue;
        }

        // Check acknowledgement
        unpack_packet_msg(recv_buf, &ack_pkt);
        if (strcmp(ack_pkt.filename, filename) == 0) {
            if (ack_pkt.frag_no == i) {
                if (strcmp(ack_pkt.filedata, "ACK") == 0) {
                    printf("ACK for packet %d out of %d received from server\n", i, total_frag);
                    continue;
                }
            }
        }

        // Retransmit if acknowledgement fails
        printf("ACK for packet %d out of %d not received from server - retransmitting...\n", i, total_frag);
        i--;
    }

    // Clean up
    for (int i = 1; i <= total_frag; i++) {
        free(packets[i - 1]);
    }
    free(packets);
    fclose(fp);

    printf("Transfer of %s complete\n", filename);
}

int main(int argc, char* argv[]) {		// argv[1] for server address, argv[2] for server port
    // LAB 1 - SETUP
    if (!check_arguments(argc)) {
        exit(0);
    }

    char filename[BUFFER_SIZE];
    get_filename(filename);
    if (!check_file_exists(filename)) {
        exit(1);
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
    };

    // Setup recipient (server)
    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));
    if (inet_aton(argv[1] , &server.sin_addr) == 0) {
        printf("Error in setting up server, exiting...\n");
        exit(1);
    };

    // Signal FTP to server
    const char *msg_ftp = "ftp";
    if (sendto(sockfd, msg_ftp, strlen(msg_ftp), 0, (struct sockaddr *) &server, sizeof(server)) < 0) {
        printf("Error in sending FTP message to server, exiting...\n");
        exit(1);
    };
    clock_t sent_time = clock();        // CPU ticks elapsed since process started running

    // Receive acknowledgement from server
    char msg_received[BUFFER_SIZE];
    struct sockaddr_storage server_addr;
    socklen_t addr_len = sizeof(server_addr);
    ssize_t bytes_recv;
    if ((bytes_recv = recvfrom(sockfd, msg_received, BUFFER_SIZE, 0, (struct sockaddr*) &server_addr, &addr_len)) == -1) {
        printf("Error in receiving confirmation message from server, exiting...\n");
    };
    clock_t received_time = clock();
    clock_t start_RTT = received_time - sent_time;
    double elapsed_time = (double) (start_RTT) / CLOCKS_PER_SEC;
    printf("RTT time is %lfs\n", elapsed_time);

    if (bytes_recv < 0) {
        printf("No message received, exiting...\n");
        close(sockfd);
        exit(1);
    }

    char *success_msg = "yes";
    if (strstr(msg_received, success_msg) != NULL) {
        printf("A file transfer can start!\n\n");
    }

    // LAB 2 - FILE TRANSFER
    send_file(filename, sockfd, server, start_RTT);

    close(sockfd);
    return 0;
}