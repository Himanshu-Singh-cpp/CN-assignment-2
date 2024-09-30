#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char message[BUFFER_SIZE];

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, "192.168.192.240", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        return -1;
    }

    printf("Connected to server. Enter a message: \n");

    // Read a single message from the user
    if (fgets(message, BUFFER_SIZE, stdin) != NULL) {
        // Remove newline character added by fgets
        size_t len = strlen(message);
        if (message[len - 1] == '\n') {
            message[len - 1] = '\0';
        }

        // Send the message to the server
        int send_status = send(sock, message, strlen(message), 0);
        if (send_status < 0) {
            perror("Send failed");
            close(sock);
            return -1;
        }
        printf("Message sent\n");

        // Wait for the server's response
        int valread = read(sock, buffer, BUFFER_SIZE);
        if (valread < 0) {
            perror("Read failed");
        } else if (valread == 0) {
            printf("Server closed the connection\n");
        } else {
            buffer[valread] = '\0';  // Null-terminate the buffer
            printf("Server replied: %s\n", buffer);
        }
    }

    // Close the socket
    close(sock);
    return 0;
}
