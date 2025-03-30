#ifndef PACKET_H
#define PACKET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024
#define MAX_NAME 32
#define MAX_DATA 1024

typedef struct {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
} Message;

enum Message_type {
    LOGIN,
    LO_ACK,
    LO_NAK,
    EXIT,
    JOIN,
    JN_ACK,
    JN_NAK,
    LEAVE_SESS,
    NEW_SESS,
    NS_ACK,
    MESSAGE,
    QUERY,
    QU_ACK
};

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

void convert_msg_to_str(const Message *msg, char *payload) {
    snprintf(payload, BUFFER_SIZE, "%d:%d:%s:%s", msg->type, msg->size, (char *) msg->source, (char *) msg->data);
}

void convert_str_to_msg(char *payload, Message *msg) {
    int count = 0;
    char *field_ptrs[4];
    field_ptrs[count++] = payload;

    for (char *c = payload; c != NULL; c++) {
        if (count >= 4) {
            break;                              // Prevent code from changing filedata
        }
        if (*c == ':') {
            *c = '\0';                          // Replace with null terminator
            field_ptrs[count++] = c + 1;
        }
    }

    msg->type = atoi(field_ptrs[0]);
    msg->size = atoi(field_ptrs[1]);
    memcpy(msg->source, field_ptrs[2], MAX_NAME);
    memcpy(msg->data, field_ptrs[3], MAX_DATA);
}

#endif //PACKET_H