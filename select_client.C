#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080

int main() {
    int sock = 0;
    struct sockaddr_in server_address;
    char buffer[1024] = {0};
    
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    
    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    // Send a message to the server
    const char *message = "Requesting top two CPU-consuming processes";
    send(sock, message, strlen(message), 0);
    printf("Message sent: %s\n", message);

    // Receive the server's response
    int valread = read(sock, buffer, 1024);
    printf("Message from server: %s\n", buffer);

    // Close the socket
    close(sock);
    return 0;
}
