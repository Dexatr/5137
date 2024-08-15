#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 54321          // Define the port number the server will listen on
#define BUFFER_SIZE 1024    // Define the size of the buffer to store incoming data

// Function to handle communication with a connected client
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};  // Buffer to store data received from the client
    int bytes_read;

    // Send a welcome message to the client
    char *welcome_msg = "Hello from Raspberry Pi server!";
    send(client_socket, welcome_msg, strlen(welcome_msg), 0);

    // Receive data from the client
    bytes_read = read(client_socket, buffer, BUFFER_SIZE);
    if (bytes_read > 0) {
        // Print the data received from the client to the console
        printf("Received from client: %s\n", buffer);
    }

    // Close the client socket once communication is complete
    close(client_socket);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Create a socket for the server
    // AF_INET indicates IPv4, SOCK_STREAM indicates TCP
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == 0) {
        perror("Socket creation failed");  // Print an error message if socket creation fails
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the server's IP address and the defined port number
    server_addr.sin_family = AF_INET;               // Use IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;       // Bind to any available network interface
    server_addr.sin_port = htons(PORT);             // Convert the port number to network byte order

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");  // Print an error message if binding fails
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections with a backlog of 3 pending connections
    if (listen(server_socket, 3) < 0) {
        perror("Listen failed");  // Print an error message if listening fails
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);  // Inform the user that the server is ready

    // Main loop to accept and handle incoming client connections
    while (1) {
        // Accept an incoming connection from a client
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
        if (client_socket < 0) {
            perror("Accept failed");  // Print an error message if accepting a connection fails
            close(server_socket);
            exit(EXIT_FAILURE);
        }

        // Print the client's IP address and port number for reference
        printf("Connection accepted from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Handle the client in a separate function
        handle_client(client_socket);
    }

    // Close the server socket before exiting the program (not reached in this loop)
    close(server_socket);
    return 0;
}
