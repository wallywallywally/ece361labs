#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <assert.h>

#include "packet.h"

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

  #define BUFF_SIZE 1024
  char buffer[BUFF_SIZE] = {0};

  // File pointer
  FILE *file;
  // Expected fragment number is 1 at first
  int expected_frag_no = 1;

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

     if (strcmp(buffer, "ftp") == 0) {
       char *response = "yes\0";
       ssize_t bytes_sent = sendto(sockfd, response, strlen(response), 0,
                                 (struct sockaddr *) &client_addr, client_addr_len);

       if(bytes_sent == -1) {
         fprintf(stderr, "sendto error\n");
         close(sockfd);
         exit(EXIT_FAILURE);
       }

       continue;
     }

     packet pkt;
     unpack_packet_msg(buffer, &pkt);
     // print_debug(buffer, sizeof(pkt));

     packet resp_pkt = pkt;
     resp_pkt.size = 4;

     // Error checking
     if (expected_frag_no != pkt.frag_no) {
       fprintf(stderr, "expected_frag_no: %d\npacket frag_no: %d\n", expected_frag_no, pkt.frag_no);
       strcpy(resp_pkt.filedata, "NAK");
     } else {
       strcpy(resp_pkt.filedata, "ACK");

       if (pkt.frag_no == 1) {
         // Open a file in write mode
         char cwd[1024];
         getcwd(cwd, sizeof(cwd));
         char filePath[1024];
         snprintf(filePath, sizeof(filePath), "%s/%s", cwd, pkt.filename);

         file = fopen(filePath, "wb");

         if (file == NULL) {
           fprintf(stderr, "open file error\n");
           return 1;
         }
         assert(file != NULL);
         printf("File %s created\n", pkt.filename);
       }
       fwrite(pkt.filedata, sizeof(char), pkt.size, file);
       // fprintf(file, "%s", pkt.filedata);

       // If last packet has been sent close the file stream.
       if (pkt.frag_no == pkt.total_frag) {
         if (fclose(file) == 0) {
           printf("File %s closed\n", pkt.filename);
         } else {
           fprintf(stderr, "File %s close error\n", pkt.filename);
         }

         expected_frag_no = 1;
       } else {
         expected_frag_no = pkt.frag_no + 1;
       }
     }

     char response[BUFF_SIZE] = {0};
     prepare_packet_msg(resp_pkt, response);

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
