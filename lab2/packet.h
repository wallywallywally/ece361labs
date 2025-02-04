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
void packetise(const char *filename, packet p) {
    // fill pkt -> need to parse our file from filename
}

void prepare_payload(packet p, char *payload) {
    snprintf(payload, BUFFER_SIZE, "%d:%d:%d:%s:%s", p.total_frag, p.frag_no, p.size, p.filename, p.filedata);
}

#endif //PACKET_H
