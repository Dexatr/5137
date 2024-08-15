#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 54321          // Define the port number that the server is listening on
#define BUFFER_SIZE 1024    // Define the size of the buffer to store incoming data

int main() {
    int client_socket;  // Variable to store the client socket descriptor
    struct sockaddr_in server_addr;  // Struct to hold the server address information
    char buffer[BUFFER_SIZE] = {0};  // Buffer to store the message received from the server

    // Create a socket for the client
    // AF_INET indicates IPv4, SOCK_STREAM indicates TCP
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");  // Print an error message if socket creation fails
        exit(EXIT_FAILURE);
    }

    // Define the server's address
    server_addr.sin_family = AF_INET;  // Use IPv4
    server_addr.sin_port = htons(PORT);  // Convert the port number to network byte order

    // Convert the server's IP address from text to binary form and store it in the server_addr struct
    if (inet_pton(AF_INET, "192.168.1.204", &server_addr.sin_addr) <= 0) {  // Replace with your Raspberry Pi's IP address
        perror("Invalid address or address not supported");  // Print an error message if IP address conversion fails
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Connect to the server using the specified IP address and port
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");  // Print an error message if the connection fails
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Read the message sent by the server
    read(client_socket, buffer, BUFFER_SIZE);
    // Print the received message to the console
    printf("Message from server: %s\n", buffer);

    // Prepare a response message to send to the server
    char *message = "Hello from the Linux host client!";
    // Send the response message to the server
    send(client_socket, message, strlen(message), 0);

    // Close the client socket after communication is complete
    close(client_socket);
    return 0;
}
