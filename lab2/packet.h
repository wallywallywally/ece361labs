#ifndef PACKET_H
#define PACKET_H

#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024
#define DATA_SIZE 1000

typedef struct {
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char* filename;
    char filedata[DATA_SIZE];
} packet;

// Sender stuff
void prepare_packet_msg(packet p, char *payload) {
    snprintf(payload, BUFFER_SIZE, "%d:%d:%d:%s:%s", p.total_frag, p.frag_no, p.size, p.filename, p.filedata);
}

void unpack_packet_msg(packet p, char *buffer) {

}

#endif //PACKET_H
