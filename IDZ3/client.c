#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"

int generate_random_number(int min, int max) {
    return (rand() % (max - min + 1)) + min;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("pls enter port\n");
        exit(1);
    }

    int port = atoi(argv[1]);

    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\nSocket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported\n");
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection failed\n");
        return -1;
    }

    // Generate random budget and room type
    srand(time(0));
    int budget = generate_random_number(0, 10000);
    int room_type;

    // Send budget to server
    printf("Budget: %d\n", budget);
    snprintf(buffer, sizeof(buffer), "%d", budget);
    send(sock, buffer, strlen(buffer), 0);

    // Read server response
    valread = read(sock, buffer, 1024);
    printf("%s\n", buffer);

    if (strcmp(buffer, "1") == 0 || strcmp(buffer, "2") == 0 || strcmp(buffer, "3") == 0) {
        // Send room type to server
        printf("Avaible room type: %d\n", atoi(buffer));
        room_type = rand() % atoi(buffer) + 1;
        printf("Chosen type: %d\n", room_type);
        char answer[5];
        snprintf(answer, sizeof(buffer), "%d", room_type);
        send(sock, buffer, strlen(buffer), 0);

        // Read server response
        valread = recv(sock, buffer, 1024, 0);
        printf("%s\n", buffer);
    } else if (strcmp(buffer, "No rooms") == 0) {
        printf("Client left the hotel due to insufficient funds.\n");
    }

    // Close socket
    close(sock);

    return 0;
}
