#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <dirent.h>   
#include <ctype.h>    
#include <stdlib.h>
#include <stdint.h> // Include for intptr_t

#define PORT 8081
#define MAX_CLIENTS 10

typedef struct {
    char name[256];
    int pid;
    long user_time;
    long kernel_time;
} ProcessInfo;

// Function to read CPU time from /proc/[pid]/stat
void get_cpu_usage(ProcessInfo *proc) {
    char path[256], buffer[1024];
    sprintf(path, "/proc/%d/stat", proc->pid);
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return;
    }

    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';  // Null-terminate the buffer
        sscanf(buffer, "%*d %s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %ld %ld",
               proc->name, &proc->user_time, &proc->kernel_time);
    } else {
        perror("read");
    }

    close(fd);
}

// Comparison function for sorting processes based on total CPU time (user + kernel)
int compare_cpu_usage(const void *a, const void *b) {
    const ProcessInfo *procA = (const ProcessInfo *)a;
    const ProcessInfo *procB = (const ProcessInfo *)b;

    // Calculate total CPU time for both processes
    long totalA = procA->user_time + procA->kernel_time;
    long totalB = procB->user_time + procB->kernel_time;

    // Sort in descending order (larger total CPU time first)
    return (totalB - totalA);
}

// Function to find and sort all CPU-consuming processes
int find_all_processes(ProcessInfo **procs) {
    DIR *dir = opendir("/proc");
    if (dir == NULL) {
        perror("opendir");
        return 0;
    }

    struct dirent *entry;
    int count = 0;
    int capacity = 100;  // Start with space for 100 processes

    // Allocate memory for storing process info
    *procs = malloc(capacity * sizeof(ProcessInfo));
    if (*procs == NULL) {
        perror("malloc");
        closedir(dir);
        return 0;
    }

    // Read all the directories in /proc
    while ((entry = readdir(dir)) != NULL) {
        if (isdigit(*entry->d_name)) {
            // Expand the array if needed
            if (count >= capacity) {
                capacity *= 2;
                *procs = realloc(*procs, capacity * sizeof(ProcessInfo));
                if (*procs == NULL) {
                    perror("realloc");
                    closedir(dir);
                    return count;
                }
            }
            (*procs)[count].pid = atoi(entry->d_name);
            get_cpu_usage(&(*procs)[count]);
            count++;
        }
    }
    closedir(dir);

    // Sort processes based on the total CPU time (user_time + kernel_time)
    qsort(*procs, count, sizeof(ProcessInfo), compare_cpu_usage);

    return count;
}

void* handle_client(void* arg) {
    // Correctly cast the argument to a pointer
    int client_socket = *(int*)arg;
    free(arg);

    char buffer[1024] = {0};

    // Read the message from the client
    if (read(client_socket, buffer, 1024) < 0) {
        perror("read");
        close(client_socket);
        return NULL;
    }
    printf("Message received: %s\n", buffer);

    // Find and send top two CPU-consuming processes
    ProcessInfo *procs = NULL;
    int num_procs = find_all_processes(&procs);
    
    if (num_procs < 2) {
        perror("Not enough processes");
        free(procs);
        close(client_socket);
        return NULL;
    }

    char message[1024];
    sprintf(message, "Top two CPU-consuming processes:\n"
                     "1. Name: %s, PID: %d, User Time: %ld, Kernel Time: %ld\n"
                     "2. Name: %s, PID: %d, User Time: %ld, Kernel Time: %ld\n",
                     procs[0].name, procs[0].pid, procs[0].user_time, procs[0].kernel_time,
                     procs[1].name, procs[1].pid, procs[1].user_time, procs[1].kernel_time);

    if (send(client_socket, message, strlen(message), 0) < 0) {
        perror("send");
        free(procs);
        close(client_socket);
        return NULL;
    }
    printf("Message sent: %s\n", message);

    // Free allocated memory and close the client socket
    free(procs);
    close(client_socket);
    pthread_exit(NULL);
}

int main() {
    int server_socket;
    struct sockaddr_in server_address;
    socklen_t addr_size = sizeof(server_address);

    // Create a socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return 1;
    }

    // Define the server address
    server_address.sin_family = AF_INET;         // IPv4
    server_address.sin_port = htons(PORT);       // Port
    server_address.sin_addr.s_addr = INADDR_ANY; // Any IP address

    // Bind the socket to the address
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("bind");
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("listen");
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);

    // Accept incoming connections
    while (true) {
        int *client_socket = malloc(sizeof(int));
        if (!client_socket) {
            perror("malloc");
            continue;
        }

        *client_socket = accept(server_socket, (struct sockaddr *)&server_address, &addr_size);
        if (*client_socket < 0) {
            perror("accept");
            free(client_socket);
            continue;
        }

        // Run every client in a separate thread
        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, (void*)client_socket) != 0) {
            perror("pthread_create");
            free(client_socket);
            continue;
        }

        // Detach the thread so it cleans up after finishing
        pthread_detach(thread);
    }

    // Close the server socket
    close(server_socket);
    return 0;
}
