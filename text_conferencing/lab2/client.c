#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>

#include "packet.h"
#define INVALID (-1)
#define MAX_NAME 32


// Client details
char buffer[BUFFER_SIZE];

typedef struct s {
    char id[MAX_NAME];
    struct s* next;         // Linked list implementation
} Session;
Session* head = NULL;

bool in_session(const char* id) {
    Session* current = head;
    while (current) {
        if (strcmp(current->id, id) == 0) {
            free(current);
            return true;
        }
        current = current->next;
    }
    free(current);
    return false;
}

void add_session(const char* id) {
    if (head) {
        // Add to head
        Session* new_session = malloc(sizeof(Session));
        strcpy(new_session->id, id);
        new_session->next = head;
        head = new_session;
    } else {
        // Create new head
        head = malloc(sizeof(Session));
        strcpy(head->id, id);
        head->next = NULL;
    }
}

void remove_session(const char* id) {
    // Only 1 session
    if (head->next == NULL) {
        free(head);
        head = NULL;
        return;
    }

    Session *target = head, *del = NULL;
    while (target->next) {
        if (strcmp(target->next->id, id) == 0) {
            break;
        }
        target = target->next;
    }
    del = target->next;
    target->next = target->next->next;
    free(del);
}

void clear_sessions() {
    while (head) {
        Session* current = head;
        head = head->next;
        free(current);
    }
}

// i need to receive session id from server !!!

// Command strings
const char* C_LOGIN = "/login";
const char* C_LOGOUT = "/logout";
const char* C_JOIN_SESSION = "/joinsession";
const char* C_LEAVE_SESSION = "/leavesession";
const char* C_CREATE_SESSION = "/createsession";
const char* C_LIST = "/list";
const char* C_DM = "/dm";
const char* C_QUIT = "/quit";

// Handle messages from server
// void ptr as we want to run this as a separate pthread
void* receive(void* sockfd) {
    int* sockfd_int = (int*) sockfd;
    int numbytes;
    Message* msg = malloc( sizeof(Message) );
    memset(msg, 0, sizeof(Message));

    while (1) {
        if ((numbytes = recv(*sockfd_int, buffer, BUFFER_SIZE - 1, 0)) == INVALID) {
            printf("Client failed to receive\n");
            return NULL;
        }
        if (numbytes == 0) {
            continue;
        }
        buffer[numbytes] = '\0';
        convert_str_to_msg(buffer, msg);

        // Used for acknowledgements and messaging
        switch (msg->type) {
            case JN_ACK:
                if (strcmp((char*) msg -> data, "Session left successfully") == 0) {
                    // Leave session
                    printf("Left session - %s\n", (const char*) msg->data);
                    remove_session(msg->data);
                } else {
                    // Join session
                    printf("Joined session - %s\n", (const char*) msg->data);
                    add_session(msg->data);
                }
                break;
            case JN_NAK:
                printf("Operation failed - %s\n", (const char*) msg->data);
                break;
            case NS_ACK:
                printf("Created and joined session - %s\n", (const char*) msg->data);
                add_session(msg->data);
                break;
            case QU_ACK:
                printf("Userlist %s\n", (const char*) msg->data);
                break;
            case MESSAGE:
                printf("MSG: %s: %s\n", (const char*) msg->source, (const char*) msg->data);
                fflush(stdout);
                break;
            default:
                printf("Error - %d, %s\n", msg->type, (const char*) msg->data);
        }
    }
}

// COMMANDS
void login(char* curr, int* sockfd_int, pthread_t* recv_thread) {
    // Get arguments
    char *client_id, *password, *server_ip, *server_port;
    curr = strtok(NULL, " ");
    client_id = curr;
    curr = strtok(NULL, " ");
    password = curr;
    curr = strtok(NULL, " ");
    server_ip = curr;
    curr = strtok(NULL, " \n");
    server_port = curr;

    // sockfd_int tracks if client is logged in
    if (*sockfd_int != INVALID) {
        printf("Failed to login - already logged in to a server\n");
        return;
    } else if (client_id == NULL || password == NULL || server_ip == NULL || server_port == NULL) {
        printf("Failed to login - invalid arguments\n");
        return;
    } else {
        // TCP CONNECTION
        struct addrinfo hints, *server_info, *p;
        hints.ai_family = AF_INET;          // IPv4
        hints.ai_socktype = SOCK_STREAM;    // Bytestream
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;
        printf("%s(%s) connecting to server at %s:%s\n", client_id, password, server_ip, server_port);

        // Get server's IP address and socket structure
        int help;
        if ((help = getaddrinfo(server_ip, server_port, &hints, &server_info)) != 0) {
            printf("Failed to get server address, error %s\n", gai_strerror(help));
            return;
        }

        // Loop through results to find one that works
        for (p = server_info; p != NULL; p = p->ai_next) {
            // Create socket
            if ((*sockfd_int = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                continue;
            }
            // Connect TCP
            if (connect(*sockfd_int, p->ai_addr, p->ai_addrlen) == -1) {
                close(*sockfd_int);
                continue;
            }
            break;
        }
        if (p == NULL) {
            printf("Failed to connect - no server found\n");
            *sockfd_int = INVALID;
            return;
        }

        // Successfully connected
        freeaddrinfo(server_info);
        printf("Connected to server\n");

        // LOGIN FLOW
        ssize_t numbytes;
        Message msg = {
            .type = LOGIN,
            .size = strlen(password),
        };
        strcpy(msg.source, client_id);
        strcpy(msg.data, password);
        convert_msg_to_str(&msg, buffer);

        // Send login message
        if ((numbytes = send(*sockfd_int, buffer, BUFFER_SIZE - 1, 0)) == -1) {
            printf("Failed to login\n");
            close(*sockfd_int);
            *sockfd_int = INVALID;
            return;
        }

        // Receive result
        if ((numbytes = recv(*sockfd_int, buffer, BUFFER_SIZE - 1, 0)) == -1) {
            printf("Failed to receive login response\n");
            close(*sockfd_int);
            *sockfd_int = INVALID;
            return;
        }
        buffer[numbytes] = '\0';
        convert_str_to_msg(buffer, &msg);
        if (msg.type == LO_ACK && pthread_create(recv_thread, NULL, receive, sockfd_int) == 0) {
            printf("Logged in\n");
        } else if (msg.type == LO_NAK) {
            printf("Failed to login, %s\n", msg.data);
            close(*sockfd_int);
            *sockfd_int = INVALID;
            return;
        } else {
            printf("Unexpected error on login, %d, %s\n", msg.type, msg.data);
            close(*sockfd_int);
            *sockfd_int = INVALID;
            return;
        }
    }
}

void logout(int* sockfd_int, pthread_t* recv_thread) {
    if (*sockfd_int == INVALID) {
        printf("Failed to logout - not logged in\n");
        return;
    }

    ssize_t numbytes;
    Message msg = {
        .type = EXIT,
        .size = 0,
    };
    convert_msg_to_str(&msg, buffer);
    if ((numbytes = send(*sockfd_int, buffer, BUFFER_SIZE - 1, 0)) == -1) {
        printf("Failed to logout\n");
        return;
    }

    if (pthread_cancel(*recv_thread)) {
         printf("Failed to logout - recv thread still open\n");
    } else {
         printf("Logged out\n");
    }
    clear_sessions();
    close(*sockfd_int);
    *sockfd_int = INVALID;
}

void joinsession(char* curr, int* sockfd_int) {
    if (*sockfd_int == INVALID) {
        printf("Failed to join session - not logged in\n");
        return;
    }

    curr = strtok(NULL, " ");
    const char* id = curr;
    if (id == NULL) {
        printf("Failed to join session - invalid arguments\n");
        return;
    } else if (in_session(id)) {
        printf("Failed to join session - already in given session\n");
        return;
    }

    ssize_t numbytes;
    Message msg = {
        .type = JOIN,
        .size = strlen(id),
    };
    strcpy(msg.data, id);
    convert_msg_to_str(&msg, buffer);

    if ((numbytes = send(*sockfd_int, buffer, BUFFER_SIZE - 1, 0)) == -1) {
        printf("Failed to join session - failed to send request\n");
    }
}

void leavesession(char* curr, int* sockfd_int) {
    if (*sockfd_int == INVALID) {
        printf("Failed to leave session - not logged in\n");
        return;
    }

    curr = strtok(NULL, " ");
    const char* id = curr;
    if (id == NULL) {
        printf("Failed to leave session - invalid arguments\n");
        return;
    } else if (head == NULL || in_session(id)) {
        printf("Failed to leave session - not in given session\n");
        return;
    }

    int numbytes;
    Message msg = {
        .type = LEAVE_SESS,
        .size = 0,
    };
    strcpy(msg.data, id);
    convert_msg_to_str(&msg, buffer);

    if ((numbytes = send(*sockfd_int, buffer, BUFFER_SIZE - 1, 0)) == -1) {
        printf("Failed to leave session - failed to send request\n");
    }
}

void createsession(char* curr, int* sockfd_int) {
    if (*sockfd_int == INVALID) {
        printf("Failed to create session - not logged in\n");
        return;
    }

    curr = strtok(NULL, " ");
    const char* id = curr;
    if (id == NULL) {
        printf("Failed to create session - invalid arguments\n");
        return;
    } else if (in_session(id)) {
        printf("Failed to create session - already in given session\n");
        return;
    }

    int numbytes;
    Message msg = {
        .type = NEW_SESS,
        .size = 0,
    };
    strcpy(msg.data, id);
    convert_msg_to_str(&msg, buffer);

    if ((numbytes = send(*sockfd_int, buffer, BUFFER_SIZE - 1, 0)) == -1) {
        printf("Failed to create session - failed to send request\n");
    }
}

void send_text(int* sockfd_int) {
    if (*sockfd_int == INVALID) {
        printf("Failed to send text - not logged in\n");
        return;
    } else if (head == NULL) {
        printf("Failed to send text - not in any session\n");
        return;
    }

    int numbytes;
    Message msg = {
        .type = MESSAGE,
        .size = strlen(buffer),
    };
    strcpy(msg.data, buffer);
    convert_msg_to_str(&msg, buffer);

    if ((numbytes = send(*sockfd_int, buffer, BUFFER_SIZE - 1, 0)) == -1) {
        printf("Failed to send text - failed to send request\n");
        return;
    }
}

void list(int* sockfd_int) {
    if (*sockfd_int == INVALID) {
        printf("Failed to list - not logged in\n");
        return;
    }

    int numbytes;
    Message msg = {
        .type = QUERY,
        .size = 0,
    };
    convert_msg_to_str(&msg, buffer);

    if ((numbytes = send(*sockfd_int, buffer, BUFFER_SIZE - 1, 0)) == -1) {
        printf("Failed to list - failed to send request\n");
        return;
    }
}

void send_dm(char* curr, int* sockfd_int) {
    if (*sockfd_int == INVALID) {
        printf("Failed to send private message - not logged in\n");
        return;
    }

    // curr = strtok(NULL, " ");
    // const char* user = curr;
    // if (user == NULL) {
    //     printf("Failed to send private message - invalid arguments\n");
    //     return;
    // }
    //
    // ssize_t numbytes;
    // Message msg = {
    //     .type = JOIN,
    //     .size = strlen(id),
    // };
    // strcpy(msg.data, id);
    // convert_msg_to_str(&msg, buffer);
    //
    // if ((numbytes = send(*sockfd_int, buffer, BUFFER_SIZE - 1, 0)) == -1) {
    //     printf("Failed to join session - failed to send request\n");
    // }
}

// MAIN
int main(void) {
    char* curr;
    int toklen;
    int sockfd = INVALID;
    pthread_t recv_thread;

    // PARSE USER INPUT
    while (1) {
        // Read input, terminate on newline
        fgets(buffer, BUFFER_SIZE - 1, stdin);

        // Clean input
        buffer[strcspn(buffer, "\n")] = '\0';        // Remove newline
        curr = buffer;
        while (*curr == ' ') {                        // Remove leading whitespaces
            curr++;
        }

        // Ignore empty command
        if (*curr == '\0') {
            continue;
        }

        // Handle valid commands
        curr = strtok(buffer, " ");        // Extract command
        toklen = strlen(curr);
        if (strcmp(curr, C_LOGIN) == 0) {
            login(curr, &sockfd, &recv_thread);
        } else if (strcmp(curr, C_LOGOUT) == 0) {
            logout(&sockfd, &recv_thread);
        } else if (strcmp(curr, C_JOIN_SESSION) == 0) {
            joinsession(curr, &sockfd);
        } else if (strcmp(curr, C_LEAVE_SESSION) == 0) {
            leavesession(curr, &sockfd);
        } else if (strcmp(curr, C_CREATE_SESSION) == 0) {
            createsession(curr, &sockfd);
        } else if (strcmp(curr, C_LIST) == 0) {
            list(&sockfd);
        } else if (strcmp(curr, C_DM) == 0) {
            send_dm(curr, &sockfd);
        } else if (strcmp(curr, C_QUIT) == 0) {
            logout(&sockfd, &recv_thread);
            break;                        // Stop code entirely
        } else {
            // Send message
            buffer[toklen] = ' ';        // Reset extraction of command word
            send_text(&sockfd);
        }
    }

    printf("Quitting...goodbye!\n");
    return 0;
}
