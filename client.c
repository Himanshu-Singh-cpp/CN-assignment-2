#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8081

// Function to handle each client request in a separate thread
void* client_task(void* arg) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    const char *hello = "Hello from client";

    // Get the thread ID
    pthread_t id = pthread_self();

    // Create a socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return NULL;
    }

    // Define the server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "192.168.192.220", &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        return NULL;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        return NULL;
    }

    // Send a message to the server
    if (send(sock, hello, strlen(hello), 0) < 0) {
        perror("send");
        return NULL;
    }

    printf("Hello message sent from thread %lu\n", id);

    // Read the server's response
    if (read(sock, buffer, 1024) < 0) {
        perror("read");
        return NULL;
    }

    printf("Message received from server in thread %lu: %s\n", id, buffer);

    close(sock);

    return NULL;
}

int main(int argc, char *argv[]) {
    // Check if the correct number of arguments is provided
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_threads>\n", argv[0]);
        return 1;
    }

    // Convert the command-line argument to an integer
    int n = atoi(argv[1]);

    // Check if the provided argument is a valid positive integer
    if (n <= 0) {
        fprintf(stderr, "Error: The number of threads must be a positive integer.\n");
        return 1;
    }

    // Allocate memory for threads
    pthread_t* threads = (pthread_t*)malloc(n * sizeof(pthread_t));
    if (threads == NULL) {
        perror("malloc");
        return 1;
    }

    // Create n threads
    for (int i = 0; i < n; i++) {
        if (pthread_create(&threads[i], NULL, client_task, NULL) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < n; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("pthread_join");
            return 1;
        }
    }

    // Free the allocated memory for threads
    free(threads);

    return 0;
}
