#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include "packet.h"
#include "user.h"

#define BACKLOG 16

/* Shared Resources protected by mutex */
User* userList[NUM_CREDENTIALS] = { NULL };
char* sessionList[NUM_CREDENTIALS] = { NULL };
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Attempt to bind a socket to the specified port.
// If port is already in use, return false. Else, return true.
bool isPortAvailable(int port) {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
    	fprintf(stderr, "Error creating socket\n");

        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    // `htons()` converts integer from host byte order to network byte order (big endian)
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    	close(sockfd);
        return false;
    }

    close(sockfd);

    return true;
}

void setMessage(Message *response, unsigned int type, char* data) {
	response->type = type;
	strcpy((char *)response -> data, data);
	response -> size = strlen(data);
}

void *clientThread(void *arg) {
	User *user = (User *)arg;
    char buffer[BUFFER_SIZE];
    Message *message = malloc( sizeof(Message) );
    Message *response = malloc( sizeof(Message) );

    bool isAuthenticated = false;

    while (true) {
        start:
        // Clear previous values in buffer and packet
    	memset(buffer, 0, sizeof(char) * BUFFER_SIZE);
        memset(message, 0, sizeof(Message));
    	memset(response, 0, sizeof(Message));

        ssize_t bytes = recv(user->sockfd, buffer, BUFFER_SIZE, 0);
        if (bytes < 0) {
        	fprintf(stderr, "Error receiving message\n");
        }
        if (bytes == 0) {
        	break;
        }
        buffer[bytes] = '\0'; // NULL-terminate buffer

        convert_str_to_msg(buffer, message);

        if (!isAuthenticated) {
        	if (message -> type == LOGIN) {

            	strcpy(user -> username, (const char*) message -> source);
                strcpy(user -> password, (const char*) message -> data);

                pthread_mutex_lock(&mutex);

                int result = is_registered(user);
                if (result == -1) { 	// user with credentials does not exist
                	setMessage(response, LO_NAK, "User not found");
                }
                else if (userList[result] != NULL) {
                    setMessage(response, LO_NAK, "User already logged in");
                }
                else {
                	setMessage(response, LO_ACK, "User logged in successfully");
                    userList[result] = user;
                    isAuthenticated = true;
                }

                pthread_mutex_unlock(&mutex);
        	} else {
        		setMessage(response, LO_NAK, "User not logged in yet");
        	}

            goto send_message;
        }

        assert(isAuthenticated == true);

        if (message -> type == NEW_SESS) {
        	int result = is_registered(user);
            assert(result != -1);

        	pthread_mutex_lock(&mutex);

        	// Check if user is already in a Session
            if (sessionList[result] != NULL) {
            	setMessage(response, JN_NAK, "User already in a session");
            	pthread_mutex_unlock(&mutex);
                goto send_message;
            }

            // Check if session already exists
            bool doesSessionExist = false;
            for (int i = 0; i < NUM_CREDENTIALS; i++) {
                if (sessionList[i] != NULL && strcmp(sessionList[i], (char*) message -> data) == 0) {
                    doesSessionExist = true;
                }
            }

            if (doesSessionExist) {
                setMessage(response, JN_NAK, "Session already exists");
                pthread_mutex_unlock(&mutex);
                goto send_message;
            }

            sessionList[result] = malloc(sizeof(char) * message -> size);
        	strcpy(sessionList[result], (const char*) message -> data);
            setMessage(response, NS_ACK, "New session created");

            pthread_mutex_unlock(&mutex);
            goto send_message;
        }

        if (message -> type == JOIN) {
        	int result = is_registered(user);
        	assert(result != -1);

        	pthread_mutex_lock(&mutex);

        	// Check if user is already in a Session
        	if (sessionList[result] != NULL) {
        		setMessage(response, JN_NAK, "User already in a session");
        		pthread_mutex_unlock(&mutex);
        		goto send_message;
        	}

            // Check if session already exists
        	bool doesSessionExist = false;
        	for (int i = 0; i < NUM_CREDENTIALS; i++) {
        		if (sessionList[i] != NULL && strcmp(sessionList[i], message -> data) == 0) {
        			doesSessionExist = true;
        		}
        	}

        	if (!doesSessionExist) {
        		setMessage(response, JN_NAK, "Session does not exist");
        		pthread_mutex_unlock(&mutex);
        		goto send_message;
        	}

            sessionList[result] = malloc(sizeof(char) * message -> size);
        	strcpy(sessionList[result], (const char*) message -> data);
        	setMessage(response, JN_ACK, "Session joined successfully");
        	pthread_mutex_unlock(&mutex);
        	goto send_message;
        }

        if (message -> type == LEAVE_SESS) {
        	int result = is_registered(user);
        	assert(result != -1);

        	pthread_mutex_lock(&mutex);

        	// Check if user is already in a Session
        	if (sessionList[result] == NULL) {
        		setMessage(response, JN_NAK, "User is not in a session");
        		pthread_mutex_unlock(&mutex);
        		goto send_message;
        	}

            free(sessionList[result]);
            sessionList[result] = NULL;

        	setMessage(response, JN_ACK, "Session left successfully");
        	pthread_mutex_unlock(&mutex);
        	goto send_message;
        }

        if (message -> type == QUERY) {
            char data[MAX_DATA] = { '\0' };
            pthread_mutex_lock(&mutex);

        	for (int i = 0; i < NUM_CREDENTIALS; i++) {
            	if (userList[i] != NULL) {
                	char temp[MAX_DATA] = { '\0' };
                    snprintf(temp, MAX_DATA, "Username - %s Session - %s | ", userList[i] -> username, sessionList[i]);
                    strcat(data, temp);
            	}
        	}

            pthread_mutex_unlock(&mutex);

            setMessage(response, QU_ACK, data);
            goto send_message;
        }

        if (message -> type == MESSAGE) {
        	int result = is_registered(user);
        	assert(result != -1);

            pthread_mutex_lock(&mutex);

            if (sessionList[result] == NULL) goto start;

            for (int i = 0; i < NUM_CREDENTIALS; i++) {
            	if (i == result || userList[i] == NULL || sessionList[i] == NULL) continue;
                	setMessage(response, MESSAGE, message -> data);
                    strcpy(response -> source, userList[result] -> username);
                	convert_msg_to_str(response, buffer);

                    ssize_t bytes_sent = send(userList[i]->sockfd, buffer, BUFFER_SIZE - 1, 0);
    				if (bytes_sent < 0) {
    					fprintf(stderr, "Error sending message\n");
    				}
            }

            pthread_mutex_unlock(&mutex);

            goto start;
        }

        if (message -> type == EXIT) {
            int result = is_registered(user);
        	assert(result != -1);

        	isAuthenticated = false;
            pthread_mutex_lock(&mutex);

            userList[result] = NULL;
            if (sessionList[result] != NULL) free(sessionList[result]);
            sessionList[result] = NULL;

            pthread_mutex_unlock(&mutex);

            break;
        }

        send_message:
    		convert_msg_to_str(response, buffer);
    		ssize_t bytes_sent = send(user->sockfd, buffer, BUFFER_SIZE - 1, 0);
    		if (bytes_sent < 0) {
    			fprintf(stderr, "Error sending message\n");
    		}

        goto start;
    }

    close(user->sockfd);
    free(message);
    free(response);

    pthread_exit(NULL);
    return NULL;
}

int main(int argc, char *argv[]) {
  	if (argc != 2) {
    	fprintf(stderr, "Usage: server <TCP port number to listen on>\n");
        return 1;
  	}

    int port = atoi(argv[1]);
    if (!isPortAvailable(port)) {
    	fprintf(stderr, "Port %d is not available\n", port);
        return -1;
    }

    printf("Port: %d is available. Starting server...\n", port);

    // Set up server
    int sockfd; 							// Socket file descriptor for the server
    int result; 							// Variable for return values

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));		// Clear out garbage values from hints

    hints.ai_family = AF_INET;				// Use IPv4 address family
    hints.ai_socktype = SOCK_STREAM;		// Specify that we want a stream socket for TCP
    hints.ai_protocol = IPPROTO_TCP;		// Specify that we want to use TCP
    hints.ai_flags = AI_PASSIVE; 			// Fill in IP for us

    result = getaddrinfo(NULL, argv[1], &hints, &res);
    if (result) {
    	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
    	sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
        	continue;						// Try the next address
        }

        result = bind(sockfd, p->ai_addr, p->ai_addrlen);
        if (result) {
        	close(sockfd); 					// Bind failed, try next address
            continue;
        }

        break;
    }

    freeaddrinfo(res);
    if (p == NULL) {
    	fprintf(stderr, "Unable to bind\n");
        return -1;
    }

    result = listen(sockfd, BACKLOG);
    if (result) {
    	fprintf(stderr, "Unable to listen\n");
    }

    printf("Waiting for connections...\n");

    while (true) {
    	User *newUser = malloc(sizeof(User));
        struct sockaddr peerAddr;
        socklen_t peerAddrLen = sizeof(peerAddr);
        newUser -> sockfd = accept(sockfd, &peerAddr, &peerAddrLen);
        if (newUser -> sockfd == -1) {
        	fprintf(stderr, "Error accepting connection\n");
            break;
        }

        // Extract information from new connection
        printf("Connection type: %s\n", peerAddr.sa_family == AF_INET ? "IPv4" : "IPv6");
        if (peerAddr.sa_family == AF_INET) {
        	struct sockaddr_in *addr = (struct sockaddr_in *) &peerAddr;
            int port = ntohs(addr->sin_port);

            char ipAddress[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr->sin_addr, ipAddress, INET_ADDRSTRLEN);
            printf("Connection from %s:%d\n", ipAddress, port);
        }

		pthread_create(&(newUser -> p), NULL, clientThread, (void*) newUser);
    }

    // Cleanup
    close(sockfd);

    return 0;
};