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

#define BUFFER_SIZE 1024
#define ACK "ACK"
#define NACK "NACK"

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



void send_file(const char *filename, const int sockfd, packet p) {
    // Open file
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Error opening file %s\n", filename);
        exit(1);
    }

    // Determine # fragments required
    fseek(fp, 0, SEEK_END);
    int total_frag = ftell(fp) / DATA_SIZE;
    printf("Total # fragments = %d\n", total_frag);
    rewind(fp);

    // Packetise file
    char ack_buf[BUFFER_SIZE];
    char **packets = malloc(sizeof(char *) * total_frag);
    for (int i = 1; i <= total_frag; i++) {
        packet p;
        p.total_frag = total_frag;
        p.frag_no = i;
        p.filename = filename;
        memset(p.filedata, 0, DATA_SIZE);                   // Clear packet data
        fread(p.filedata, sizeof(char), DATA_SIZE, fp);
        p.size = i != total_frag ? DATA_SIZE : ftell(fp);       // !!! TODO: check if it matches up with actual data size

        // Convert packet to message
        packets[i - 1] = malloc(sizeof(char) * BUFFER_SIZE);
        prepare_packet_msg(p, packets[i - 1]);
    }

    // Execute file transfer


}

int main(int argc, char* argv[]) {		// argv[1] for server address, argv[2] for server port
    // PACKET TESTING
    packet test_p = {0,0,12,"a.txt","lo W\0o\nld!!"};
    char test_payload[BUFFER_SIZE];
    prepare_packet_msg(test_p, test_payload);
    //print_debug(test_payload, BUFFER_SIZE);

    packet p2;
    memset(&p2.filedata, 0, DATA_SIZE);
    unpack_packet_msg(test_payload, &p2);
    char test_payload2[BUFFER_SIZE];
    prepare_packet_msg(p2, test_payload2);
    //print_debug(test_payload2, BUFFER_SIZE);

    // ------------------------------------- NORMAL STUFFS -------------------------------------
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

    // Send message
    /*packet pkt;
    char payload[BUFFER_SIZE];
    packetise(filename, pkt);
    prepare_payload(pkt, payload);
    printf("%s\n", payload);*/

    const char *msg_ftp = "ftp";
    if (sendto(sockfd, msg_ftp, strlen(msg_ftp), 0, (struct sockaddr *) &server, sizeof(server)) < 0) {
        printf("Error in sending FTP message to server, exiting...\n");
        exit(1);
    };
    time_t sent_time = time(NULL);
    printf("Sent time is %ld\n", sent_time);
    
    // Receive message
    char msg_received[BUFFER_SIZE];
    struct sockaddr_storage server_addr;
    socklen_t addr_len = sizeof(server_addr);
    ssize_t bytes_recv;
    if ((bytes_recv = recvfrom(sockfd, msg_received, BUFFER_SIZE, 0, (struct sockaddr*) &server_addr, &addr_len)) == -1) {
        printf("Error in receiving confirmation message from server, exiting...\n");
    };
    time_t received_time = time(NULL);
    double elapsed_time = difftime(received_time, sent_time);
    printf("RTT time is %lf\n", elapsed_time);

    if (bytes_recv < 0) {
        printf("No message received, exiting...\n");
        close(sockfd);
        exit(1);
    }

    char *success_msg = "yes";
    printf("%s\n", msg_received);
    if (strstr(msg_received, success_msg) != NULL) {
        printf("A file transfer can start!\n");
    }

    close(sockfd);
    return 0;
}