#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define NUM_ROOMS 25
#define MAX_CLIENTS 50

typedef struct {
    int room_num;
    int room_type; // 1 - 2000 руб, 2 - 4000 руб, 3 - 6000 руб
    int is_free;
} room_t;

typedef struct {
    int num_rooms_1;
    int num_rooms_2;
    int num_rooms_3;
    int num_free_rooms;
    room_t rooms[NUM_ROOMS];
} shared_data_t;

void reserve_room(shared_data_t *shared_data, int room_type, int client_sockfd) {
    int i, room_num;
    for (i = 0; i < NUM_ROOMS; i++) {
        if (shared_data->rooms[i].room_type == room_type &&
            shared_data->rooms[i].is_free == 1) {
            room_num = shared_data->rooms[i].room_num;
            shared_data->rooms[i].is_free = 0;
            shared_data->num_free_rooms--;
            break;
        }
    }
    char response[100];
    if (i < NUM_ROOMS) {
        snprintf(response, sizeof(response), "Room %d reserved", room_num);
        printf("Client reserved Room %d\n", room_num);
    } else {
        snprintf(response, sizeof(response), "No available rooms of type %d", room_type);
        printf("Client requested Room of type %d, but no available rooms\n", room_type);
    }
    write(client_sockfd, response, strlen(response) + 1);
}

int main() {
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    shared_data_t shared_data;

    // Initialize shared data
    shared_data.num_rooms_1 = 10;
    shared_data.num_rooms_2 = 10;
    shared_data.num_rooms_3 = 5;
    shared_data.num_free_rooms = 25;
    int i;
    for (i = 0; i < NUM_ROOMS; i++) {
        shared_data.rooms[i].room_num = i;
        if (i < 10) {
            shared_data.rooms[i].room_type = 1;
            shared_data.rooms[i].is_free = 1;
        } else if (i < 20) {
            shared_data.rooms[i].room_type = 2;
            shared_data.rooms[i].is_free = 1;
        } else {
            shared_data.rooms[i].room_type = 3;
            shared_data.rooms[i].is_free = 1;
        }
    }

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Set address parameters
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket to address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server started. Waiting for connections...\n");

    // Accept client connections
    while ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen))) {
        printf("New client connected\n");

        // Read client request
        valread = read(new_socket, buffer, 1024);
        if (valread == 0) {
            printf("Client disconnected\n");
            close(new_socket);
            continue;
        }

        // Process client request
        int budget = atoi(buffer);
        printf("Client budget: %d\n", budget);

        int room_type = 0;
        if (budget >= 6000 && shared_data.num_rooms_3 > 0) {
            room_type = 3;
        } else if (budget >= 4000 && shared_data.num_rooms_2 > 0) {
            room_type = 2;
        } else if (budget >= 2000 && shared_data.num_rooms_1 > 0) {
            room_type = 1;
        }

        if (room_type == 0) {
            printf("Client cannot afford any available room or no rooms available\n");
            char response[] = "Client left the hotel";
            write(new_socket, response, strlen(response) + 1);
        } else {
            reserve_room(&shared_data, room_type, new_socket);
        }

        // Close the client socket
        close(new_socket);
    }

    return 0;
}
