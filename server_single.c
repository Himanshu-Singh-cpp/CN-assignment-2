#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>

#define PORT 8080

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

// Function to compare two processes based on total CPU time (user_time + kernel_time)
int compare_processes(const void *a, const void *b) {
    ProcessInfo *procA = (ProcessInfo *)a;
    ProcessInfo *procB = (ProcessInfo *)b;
    long total_time_A = procA->user_time + procA->kernel_time;
    long total_time_B = procB->user_time + procB->kernel_time;
    return total_time_B - total_time_A; // Sort in descending order
}

// Function to find top two CPU-consuming processes (by sorting all processes)
void find_top_processes(ProcessInfo procs[], int num_procs) {
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    int count = 0;

    // Collect all processes' information
    while ((entry = readdir(dir)) != NULL) {
        if (isdigit(*entry->d_name)) {
            procs[count].pid = atoi(entry->d_name);
            get_cpu_usage(&procs[count]);
            count++;
        }
    }
    closedir(dir);

    // Sort the processes based on total CPU time (user_time + kernel_time)
    qsort(procs, count, sizeof(ProcessInfo), compare_processes);

    // Only keep the top `num_procs` processes (in this case, 2)
    if (count > num_procs) {
        count = num_procs;
    }
}


void handle_client(int client_socket) {
    char buffer[1024] = {0};

    // Read the message from the client
    if (read(client_socket, buffer, 1024) < 0) {
        perror("read");
        return;
    }
    printf("Message received: %s\n", buffer);

    // Send a message to the client
    ProcessInfo procs[2];
    find_top_processes(procs, 2);
    char message[1024];
    sprintf(message, "Top two CPU-consuming processes:\n"
                     "1. Name: %s, PID: %d, User Time: %ld, Kernel Time: %ld\n"
                     "2. Name: %s, PID: %d, User Time: %ld, Kernel Time: %ld\n",
                     procs[0].name, procs[0].pid, procs[0].user_time, procs[0].kernel_time,
                     procs[1].name, procs[1].pid, procs[1].user_time, procs[1].kernel_time);
    if (send(client_socket, message, strlen(message), 0) < 0) {
        perror("send");
        return;
    }
    printf("Message sent: %s\n", message);

    // Close the client socket
    close(client_socket);
}

int main() {
    int server_socket;
    struct sockaddr_in server_address;
    int addr_size = sizeof(server_address);

    // Create a socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return 1;
    }

    // Define the server address
    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_port = htons(PORT); // Port
    server_address.sin_addr.s_addr = INADDR_ANY; // Any IP address

    // Bind the socket to the address
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("bind");
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_socket, 1) < 0) { // Handle only one client at a time
        perror("listen");
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);

    // Accept incoming connections
    while (1) {
        int client_socket;
        if ((client_socket = accept(server_socket, (struct sockaddr *)&server_address, (socklen_t *)&addr_size)) < 0) {
            perror("accept");
            return 1;
        }

        // Handle the client request in the same thread (synchronously)
        handle_client(client_socket);
    }

    // Close the server socket
    close(server_socket);
    return 0;
}
