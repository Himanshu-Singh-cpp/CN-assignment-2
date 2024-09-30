#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

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
   
    read(fd, buffer, sizeof(buffer));
    sscanf(buffer, "%*d %s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %ld %ld",
           proc->name, &proc->user_time, &proc->kernel_time);
    close(fd);
}

// Function to find top two CPU-consuming processes
void find_top_processes(ProcessInfo procs[], int num_procs) {
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL && count < num_procs) {
        if (isdigit(*entry->d_name)) {
            procs[count].pid = atoi(entry->d_name);
            get_cpu_usage(&procs[count]);
            count++;
        }
    }
    closedir(dir);
}

// Function to compare processes by their total CPU time (user_time + kernel_time)
int compare_processes(const void *a, const void *b) {
    ProcessInfo *procA = (ProcessInfo *)a;
    ProcessInfo *procB = (ProcessInfo *)b;
    long total_time_a = procA->user_time + procA->kernel_time;
    long total_time_b = procB->user_time + procB->kernel_time;
    return total_time_b - total_time_a;
}

int main() {
    int server_fd, new_socket, client_sockets[MAX_CLIENTS], max_sd, sd;
    struct sockaddr_in address;
    fd_set readfds;
    char buffer[BUFFER_SIZE];

    // Initialize client sockets
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = 0;
    }

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the address and port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d\n", PORT);

    int addrlen = sizeof(address);

    while (1) {
        // Clear the socket set
        FD_ZERO(&readfds);

        // Add server socket to set
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        // Add client sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }
            if (sd > max_sd) {
                max_sd = sd;
            }
        }

        // Wait for activity on any of the sockets
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            perror("Select error");
        }

        // If something happened on the server socket, it's an incoming connection
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }

            printf("New connection, socket fd is %d\n", new_socket);

            // Find top two CPU-consuming processes
            ProcessInfo procs[MAX_CLIENTS];
            find_top_processes(procs, MAX_CLIENTS);

            // Sort the processes by CPU usage (highest first)
            qsort(procs, MAX_CLIENTS, sizeof(ProcessInfo), compare_processes);

            // Create a response with the top two processes
            char response[BUFFER_SIZE];
            snprintf(response, sizeof(response),
                     "Top 2 CPU-consuming processes:\n1. PID: %d, Name: %s, CPU Time: %ld\n2. PID: %d, Name: %s, CPU Time: %ld\n",
                     procs[0].pid, procs[0].name, procs[0].user_time + procs[0].kernel_time,
                     procs[1].pid, procs[1].name, procs[1].user_time + procs[1].kernel_time);

            // Send the response to the client
            send(new_socket, response, strlen(response), 0);
            printf("Sent top CPU-consuming processes to the client\n");

            // Close the socket after sending the information
            close(new_socket);
        }
    }

    return 0;
}
