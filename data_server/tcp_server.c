#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 54321
#define BUFFER_SIZE 1024

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    int bytes_read;

    // Send a welcome message to the client
    char *welcome_msg = "Hello from Raspberry Pi server!";
    send(client_socket, welcome_msg, strlen(welcome_msg), 0);

    // Receive data from the client
    bytes_read = read(client_socket, buffer, BUFFER_SIZE);
    if (bytes_read > 0) {
        printf("Received from client: %s\n", buffer);
    }

    // Close the socket
    close(client_socket);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind to a port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 3) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    // Accept incoming connections
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            close(server_socket);
            exit(EXIT_FAILURE);
        }

        printf("Connection accepted from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Handle the client in a separate function
        handle_client(client_socket);
    }

    // Close the server socket
    close(server_socket);
    return 0;
}
