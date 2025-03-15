#ifndef SESSION_H
#define SESSION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "user.h"

// A linked list is used to track all sessions
// This class points to the head of the LL
typedef struct s {
    // Session information
    int id;
    User* users;

    // LL implementation
    struct s* next;
} Session;

// Creates a new session
Session* create_session(Session* sessions_head, int new_id) {
    Session* newSession = malloc(sizeof(Session));
    newSession->id = new_id;
    newSession->next = sessions_head;
    return newSession;
}

// Checks and returns a given session
Session* check_session(Session* sessions_head, int id) {
    Session* curr = sessions_head;
    while (curr != NULL) {
        if (curr->id == id) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

// Checks if a given user exists in a given session
bool check_user_in_session(Session* sessions_head, int id, const User* user) {
    Session* session = check_session(sessions_head, id);
    if (session != NULL) {
        return check_user(session->users, user);
    } else {
        return 0;
    }
}

// Adds a given user to a given session
Session* join_session(Session* sessions_head, int id, const User* user) {
    Session* session = check_session(sessions_head, id);
    if (session == NULL) {
        printf("Session not found\n");
        return sessions_head;
    }

    User* user_copy = malloc(sizeof(User));
    memcpy(user_copy, user, sizeof(User));
    session->users = add_user(session->users, user_copy);

    return sessions_head;
}

// Removes a given user from a given session
Session* leave_session(Session* sessions_head, int id, const User* user) {
    Session* session = check_session(sessions_head, id);
    if (session == NULL || session->users == NULL) {
        printf("Session not found\n");
        return sessions_head;
    }

    if (!check_user(session->users, user)) {
        printf("User not found\n");
        return sessions_head;
    }

    session->users = remove_user(session->users, user);
    if (session->users == NULL) {
        sessions_head = delete_session(sessions_head, id);
    }

    return sessions_head;
}

// Delete a given session
Session* delete_session(Session* sessions_head, int id) {
    if (sessions_head->id == id) {
        Session* new_head = sessions_head->next;
        free(sessions_head);
        return new_head;
    } else {
        Session* curr = sessions_head;
        Session* prev = NULL;
        while (curr != NULL) {
            if (curr->id != id) {
                prev = curr;
                curr = curr->next;
            } else {
                prev->next = curr->next;
                free(curr);
                break;
            }
        }
        return sessions_head;
    }
}

#endif //SESSION_H