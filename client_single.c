#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

int main() {
    int client_socket;
    struct sockaddr_in server_address;
    char buffer[1024] = {0};

    // Create a socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return 1;
    }

    // Define the server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if(inet_pton(AF_INET, "192.168.192.240", &server_address.sin_addr) <= 0){
        perror("inet_pton");
        return 1;
    }

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        return 1;
    }

    // Send a message to the server
    const char *message = "Hello, server!";
    if (send(client_socket, message, strlen(message), 0) < 0) {
        perror("Send failed");
        return 1;
    }
    printf("Message sent to server: %s\n", message);

    // Read the response from the server
    if (read(client_socket, buffer, 1024) < 0) {
        perror("Read failed");
        return 1;
    }
    printf("Message received from server: %s\n", buffer);

    // Close the client socket
    close(client_socket);
    return 0;
}
