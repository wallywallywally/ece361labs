#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
  if(argc != 2) {
    fprintf(stderr, "Usage: %s <UDP listen port>\n", argv[0]);
    exit(EXIT_FAILURE); // Terminate the program
  }

  int udp_port = atoi(argv[1]);

  // File Descriptor for the new socket.
  // Pass this FD into all other socket functions.
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  struct sockaddr_in server_addr;
  // memset fills the memory with `0`, ie. clears the structure.
  memset(&server_addr, 0, sizeof(server_addr));
  // Internet Address Family
  server_addr.sin_family = AF_INET;
  // Bind the socket to all local interfaces.
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  // Set sin_port to server's UDP listen port.
  server_addr.sin_port = htons(udp_port);

  int res = bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
  if(res == -1) {
    fprintf(stderr, "bind error\n");
    close(sockfd);
    exit(EXIT_FAILURE); // Terminate the program
  }

  #define BUFF_SIZE 100
  char buffer[BUFF_SIZE] = {0};

  // Client address structure to store client info
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  while (1) {
     ssize_t bytes = recvfrom(sockfd, buffer, BUFF_SIZE, 0,
                             (struct sockaddr *) &client_addr, &client_addr_len);

     if (bytes == -1) {
       fprintf(stderr, "recvfrom error\n");
       close(sockfd);
       exit(EXIT_FAILURE);
     }

     char message[bytes + 1];
     strncpy(message, buffer, bytes);
     message[bytes] = '\0';

     char response[4];
     if(strcmp(message, "ftp") == 0) {
       strcpy(response, "yes");
     } else {
       strcpy(response, "no");
     }

     ssize_t bytes_sent = sendto(sockfd, response, strlen(response), 0,
                                 (struct sockaddr *) &client_addr, client_addr_len);
     if(bytes_sent == -1) {
       fprintf(stderr, "sendto error\n");
       close(sockfd);
       exit(EXIT_FAILURE);
     }
  }

  close(sockfd);
  return 0;
}
