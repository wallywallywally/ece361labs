#ifndef USER_H
#define USER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#define USERNAME_LEN 64
#define PASSWORD_LEN 64

#define NUM_SESSIONS 5

typedef struct User{
 	char username[USERNAME_LEN];
    char password[PASSWORD_LEN];

    int sockfd;
    pthread_t p;
    bool isConnected;

    // List of Sessions User has joined
    char *sessionList[NUM_SESSIONS];
} User;

#define NUM_CREDENTIALS 4
struct Credential {
    char *username;
    char *password;
};

struct Credential credentialList[] = {
    {    "elvin", "1234" },
    { "junxiang", "1234" },
    {  "willson", "1234" },
    {      "ziv", "1234" },
};

// If user is in credential list, return position of user
// Else, return -1
int is_registered(User* user) {
    for (int i = 0; i < NUM_CREDENTIALS; i++) {
        if (strcmp (credentialList[i].username, user -> username) == 0) {
            if (strcmp (credentialList[i].password, user -> password) == 0) {
                return i;
            }
        }
    }

    return -1;
}

#endif //USER_H
