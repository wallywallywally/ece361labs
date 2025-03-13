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

void print_debug(const char *data, int size) {
    printf("Raw data: ");
    for (int i = 0; i < size; i++) {
        if (data[i] == '\0') {
            printf("\\0 ");
        } else if (data[i] == '\n') {
            printf("\\n ");
        } else {
            printf("%c", data[i]);
        }
    }
    printf("\n\n");
}

void prepare_packet_msg(packet p, char *payload) {
    int offset = snprintf(payload, BUFFER_SIZE, "%d:%d:%d:%s:", p.total_frag, p.frag_no, p.size, p.filename);
    memcpy(payload + offset, p.filedata, p.size);       // memcpy to ensure data integrity
}

void unpack_packet_msg(char *payload, packet *p) {
    int count = 0;
    char *field_ptrs[5];
    field_ptrs[count++] = payload;

    for (char *c = payload; c != NULL; c++) {
        if (count >= 5) {
            break;                              // Prevent code from changing filedata
        }
        if (*c == ':') {
            *c = '\0';                          // Replace with null terminator
            field_ptrs[count++] = c + 1;
        }
    }

    p->total_frag = atoi(field_ptrs[0]);
    p->frag_no = atoi(field_ptrs[1]);
    p->size = atoi(field_ptrs[2]);
    p->filename = field_ptrs[3];
    memcpy(p->filedata, field_ptrs[4], p->size);
}

#endif //PACKET_H
