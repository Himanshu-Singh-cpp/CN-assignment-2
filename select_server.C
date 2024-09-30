#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <stdbool.h>
#include <dirent.h>
#include <ctype.h>

#define PORT 8080
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
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        perror("fopen");
        return;
    }
    
    fread(buffer, 1, sizeof(buffer), fp);
    sscanf(buffer, "%*d %s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %ld %ld",
           proc->name, &proc->user_time, &proc->kernel_time);
    fclose(fp);
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

int main() {
    int server_socket, client_socket, max_sd, activity, new_socket;
    int client_sockets[MAX_CLIENTS] = {0};
    struct sockaddr_in server_address;
    fd_set readfds;
    
    int addrlen = sizeof(server_address);

    // Create a socket
    if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set server address options
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the address
    if(bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if(listen(server_socket, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while(true) {
        // Clear the socket set and add server socket to set
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);
        max_sd = server_socket;

        // Add child sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // Wait for activity on any of the sockets
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            perror("select error");
        }

        // If something happened on the master socket, it's an incoming connection
        if (FD_ISSET(server_socket, &readfds)) {
            if ((new_socket = accept(server_socket, (struct sockaddr *)&server_address, (socklen_t *)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            printf("New connection, socket fd is %d, ip is : %s, port : %d\n",
                   new_socket, inet_ntoa(server_address.sin_addr), ntohs(server_address.sin_port));

            // Add new socket to client_sockets array
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    printf("Adding to list of sockets as %d\n", i);
                    break;
                }
            }
        }

        // Check if any of the client sockets have data to read
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];

            if (FD_ISSET(sd, &readfds)) {
                char buffer[1024] = {0};
                int valread = read(sd, buffer, 1024);

                if (valread == 0) {
                    // Connection closed
                    printf("Client disconnected, socket fd is %d\n", sd);
                    close(sd);
                    client_sockets[i] = 0;
                } else {
                    buffer[valread] = '\0';
                    printf("Message received: %s\n", buffer);

                    // Find top two CPU-consuming processes
                    ProcessInfo procs[2];
                    find_top_processes(procs, 2);
                    char message[1024];
                    sprintf(message, "Top two CPU-consuming processes:\n"
                                     "1. Name: %s, PID: %d, User Time: %ld, Kernel Time: %ld\n"
                                     "2. Name: %s, PID: %d, User Time: %ld, Kernel Time: %ld\n",
                                     procs[0].name, procs[0].pid, procs[0].user_time, procs[0].kernel_time,
                                     procs[1].name, procs[1].pid, procs[1].user_time, procs[1].kernel_time);

                    // Send the message back to the client
                    send(sd, message, strlen(message), 0);
                    printf("Message sent: %s\n", message);
                }
            }
        }
    }

    close(server_socket);
    return 0;
}
