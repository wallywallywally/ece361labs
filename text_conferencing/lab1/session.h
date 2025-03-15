#ifndef SESSION_H
#define SESSION_H

#include <stdlib.h>

#include "user.h"

// A linked list is used to track all sessions
// This class points to the head of the LL
typedef struct _session {
    // Session information
    int id;
    User* users;

    // LL implementation
    struct _session* next;
} Session;

// Start a new session
Session* start_session(Session* sessions_head, int new_id) {
    Session *newSession = malloc(sizeof(Session));
    newSession->id = new_id;
    newSession->next = sessions_head;
    return newSession;
}

// Checks and returns a given session
Session* check_session(Session *sessions_head, int id) {
    Session *curr = sessions_head;
    while(curr != NULL) {
        if(curr->id == id) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

// Checks if a given user exists in a given session
bool check_user_in_session(Session *sessions_head, int id, const User *user) {
    Session *session = check_session(sessions_head, id);
    if(session != NULL) {
        // Session exists, then check if user is in session
        return in_list(session->users, user);
    } else {
        return 0;
    }
}

// Adds a user to a given session
Session* join_session(Session* sessions_head, int id, const User *usr) {
    Session *session = check_session(sessions_head, id);

    // Malloc new user
    User *newUsr = malloc(sizeof(User));
    memcpy((void *)newUsr, (void *)usr, sizeof(User));

    // Insert into session list
    cur -> usr = add_user(cur -> usr, newUsr);

    return sessions_head;
}

#endif //SESSION_H
