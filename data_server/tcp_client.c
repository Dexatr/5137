#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 54321
#define BUFFER_SIZE 1024

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE] = {0};

    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Define server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "192.168.1.204", &server_addr.sin_addr) <= 0) {  // Use your Raspberry Pi's IP address
        perror("Invalid address or address not supported");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Read message from the server
    read(client_socket, buffer, BUFFER_SIZE);
    printf("Message from server: %s\n", buffer);

    // Send a response to the server
    char *message = "Hello from the Linux host client!";
    send(client_socket, message, strlen(message), 0);

    // Close the socket
    close(client_socket);
    return 0;
}
