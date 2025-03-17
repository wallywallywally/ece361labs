#ifndef USER_H
#define USER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#define USERNAME_LEN 64
#define PASSWORD_LEN 64

struct User;  // Forward declare the User struct
// A linked list is used to track all sessions
// This class points to the head of the LL
typedef struct s {
	// Session information
	int id;
	struct User* users;

	// LL implementation
	struct s* next;
} Session;

typedef struct User{
 	char username[USERNAME_LEN];
    char password[PASSWORD_LEN];

    int sockfd;
    pthread_t p;
    bool isConnected;

    // Pointer to Sessions User has joined
    Session *session;

    // For linked list
    struct User *next;
} User;

// =========================================================
// User functions
// =========================================================

/* Global vars to check log-in state */

#define NUM_CREDENTIALS 4
struct Credential {
    char *username;
    char *password;
};

struct Credential credentialList[] = {
    {    "e", "1" },
    { "junxiang", "1234" },
    {  "willson", "1234" },
    {      "ziv", "1234" },
};

// Append `user` to the end of `userList`.
User* add_user(User* userList, User* user) {
    // If list is empty, new user becomes the head
	if (userList == NULL) {
    	return user;
	}

    User* current = userList;
    // Advance current until there is no next User
    while (current -> next) {
    	current = current -> next;
	}

    current -> next = user;
    user -> next = NULL;

    return userList;
}

// Remove User with `username` from `userList`
User* remove_user(User* userList, const char* username) {
	User* current = userList;
    User* previous = NULL;

    // Check if head node needs to be removed
    if (strcmp (username, current -> username) == 0) {
    	User* temp = current;
        userList = current -> next;
        free (temp);

        return userList;
    }

    while (current != NULL && strcmp (username, current -> username) != 0) {
    	previous = current;
        current = current -> next;
    }

    previous -> next = current -> next;
    free (current);

    return userList;
}

// Checks if User `username` is in `userList`
bool in_list(User* userList, const char* username) {
	const User* current = userList;
    while (current != NULL) {
    	if (strcmp (username, current -> username) == 0) {
        	return true;
    	}

        current = current -> next;
    }

    return false;
}

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

// =========================================================
// Session functions
// =========================================================

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
        return in_list(session->users, user->username);
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

// Removes a given user from a given session
Session* leave_session(Session* sessions_head, int id, const User* user) {
    Session* session = check_session(sessions_head, id);
    if (session == NULL || session->users == NULL) {
        printf("Session not found\n");
        return sessions_head;
    }

    if (!in_list(session->users, user->username)) {
        printf("User not found\n");
        return sessions_head;
    }

    session->users = remove_user(session->users, user->username);
    if (session->users == NULL) {
        sessions_head = delete_session(sessions_head, id);
    }

    return sessions_head;
}

#endif //USER_H
