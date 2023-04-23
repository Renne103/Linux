#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <semaphore.h>
#include <stdint.h>

#define NUM_ROOMS 25
#define MAX_CLIENTS 50
#define SHARED_MEM_NAME "/shared_mem"
#define NUM_SEMS 4

int res;

typedef struct {
    int room_num;
    int room_type; // 1 - 2000 руб, 2 - 4000 руб,, 3 - 6000 руб
    int is_free;
} room_t;

typedef struct {
    int num_rooms_1;
    int num_rooms_2;
    int num_rooms_3;
    int num_free_rooms;
    room_t rooms[NUM_ROOMS];
} shared_data_t;

int cl(int fd, sem_t* semaphores[NUM_SEMS], shared_data_t* shared_data) {
    res = munmap(shared_data, sizeof(shared_data_t));
    if (res == -1) {
        perror("munmap failed");
        return -1;
    }

    res = close(fd);
    if (res == -1) {
        perror("close failed");
        return -1;
    }

    res = shm_unlink(SHARED_MEM_NAME);
    if (res == -1) {
        perror("shm_unlink failed");
        return -1;
    }

    for (int i = 0; i < NUM_SEMS; i++) {
        res = sem_close(semaphores[i]);
        if (res == -1) {
            perror("sem_close failed");
            return -1;
        }
    }

    return 0;
}

void handle_sigint(int signal, int fd, sem_t* semaphores[NUM_SEMS], shared_data_t* shared_data) {
    cl(fd, semaphores, shared_data);
    exit(0);
}

int reserve_room(shared_data_t* shared_data, int room_type, int amount, sem_t* semaphores[NUM_SEMS]) {
    int num_rooms, room_price, i, can_reserve = 0, room_num;
    int sem_num;
    if (room_type == 1) {
        room_price = 2000;
        num_rooms = shared_data->num_rooms_1;
        sem_num = 0;
    } else if (room_type == 2) {
        room_price = 4000;
        num_rooms = shared_data->num_rooms_2;
        sem_num = 1;
    } else if (room_type == 3) {
        room_price = 6000;
        num_rooms = shared_data->num_rooms_3;
        sem_num = 2;
    } else {
        printf("Invalid room type\n");
        return -1;
    }
    if (amount * room_price > 50000) {
        printf("Reservation amount exceeds maximum allowed\n");
        return -1;
    }

    res = sem_wait(semaphores[sem_num]);
    if (res == -1) {
        perror("sem_wait failed");
        return -1;
    }

    for (i = 0; i < NUM_ROOMS; i++) {
        if (shared_data->rooms[i].room_type == room_type && shared_data->rooms[i].is_free == 1) {
            can_reserve++;
        }
    }

    if (can_reserve < amount) {
        printf("Not enough free rooms of selected type\n");
        res = sem_post(semaphores[sem_num]);
        if (res == -1) {
            perror("sem_post failed");
            return -1;
        }
        return -1;
    }

    for (i = 0; i < NUM_ROOMS && amount > 0; i++) {
        if (shared_data->rooms[i].room_type == room_type && shared_data->rooms[i].is_free == 1) {
            shared_data->rooms[i].is_free = 0;
            shared_data->num_free_rooms--;
            shared_data->rooms[i].room_num = i + 1;
            amount--;
        }
    }

    if (room_type == 1) {
        shared_data->num_rooms_1 -= (num_rooms - shared_data->num_free_rooms);
    } else if (room_type == 2) {
        shared_data->num_rooms_2 -= (num_rooms - shared_data->num_free_rooms);
    } else {
        shared_data->num_rooms_3 -= (num_rooms - shared_data->num_free_rooms);
    }

    res = sem_post(semaphores[sem_num]);
    if (res == -1) {
        perror("sem_post failed");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int fd, i, j, room_type, amount;
    pid_t pid;
    shared_data_t *shared_data;
    if (argc != 2) {
        printf("Welcome to the hotel on Bali! Write a %s number of people, which want to chill in the hotel\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, handle_sigint); // обработчик прерывания

    amount = atoi(argv[1]);
    if (amount <= 0) {
        printf("Invalid amount\n");
        exit(EXIT_FAILURE);
    }
    char* cmd;
    sem_t* semaphores[NUM_SEMS];
    for (i = 0; i < NUM_SEMS; i++) {
        semaphores[i] = NULL;
    }
    fd = shm_open(SHARED_MEM_NAME, O_RDWR | O_CREAT | O_EXCL, 0666);
    if (fd == -1) {
        perror("shm_open failed");
        return -1;
    }

    res = ftruncate(fd, sizeof(shared_data_t));
    if (res == -1) {
        perror("ftruncate failed");
        return -1;
    }

    shared_data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared_data == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }

    shared_data->num_rooms_1 = 10;
    shared_data->num_rooms_2 = 10;
    shared_data->num_rooms_3 = 5;
    shared_data->num_free_rooms = 25;
    for (i = 0; i < NUM_ROOMS; i++) {
        shared_data->rooms[i].room_num = i+1;
        if (i < 10) {
            shared_data->rooms[i].room_type = 1;
            shared_data->rooms[i].is_free = 1;
        } else if (i < 20) {
            shared_data->rooms[i].room_type = 2;
            shared_data->rooms[i].is_free = 1;
        } else {
            shared_data->rooms[i].room_type = 3;
            shared_data->rooms[i].is_free = 1;
        }
    }


    for (i = 0; i < NUM_SEMS; i++) {
        semaphores[i] = sem_open("/semaphore", O_CREAT | O_EXCL, 0666, 1);
        if (semaphores[i] == SEM_FAILED) {
            perror("sem_open failed");
            return -1;
        }
    }

    signal(SIGINT, handle_sigint);

    for (i = 0; i < NUM_ROOMS; i++) {
        shared_data->rooms[i].room_num = i;
        if (i < 10) {
            shared_data->rooms[i].room_type = 1;
            shared_data->rooms[i].is_free = 1;
        } else if (i < 20) {
            shared_data->rooms[i].room_type = 2;
            shared_data->rooms[i].is_free = 1;
        } else {
            shared_data->rooms[i].room_type = 3;
            shared_data->rooms[i].is_free = 1;
        }
    }


    union semun arg;
    unsigned short init_values[NUM_SEMS] = {10, 10, 5, 1};
    arg.array = init_values;
    printf("Hotel simulation started\n");
    for (i = 0; i < amount; i++) {
        pid = fork();
        if (pid == -1) {
            perror("fork failed");
            return -1;
        } else if (pid == 0) {
            // Child process
            srand(time(NULL) + getpid());
            int money = (rand() % 10000) + 1000;
            int room_type;
            if (money < 4000 && money >= 2000) {
                room_type = 1;
            } else if ( money >= 4000 && money < 6000) {
                room_type = (rand() % 2) + 1;
            } else if (money >= 6000) {
                room_type =  (rand() % 3) + 1;
            }
            else{
                printf("Client %d: Amount %d is not sufficient for rent the room\n", getpid(), money);
                return 0;
            }
            printf("Client %d wants to reserve a room of type %d with amount %d\n", getpid(), room_type, money);
            int result = reserve_room(shared_data, room_type, money,  &semaphores[NUM_SEMS]);
            if (result == -1) {
                printf("Client %d: Invalid room type\n", getpid());
            } else if (result == -2) {
                printf("Client %d: No free rooms of this type\n", getpid());
            } else if (result == -3) {
                printf("Client %d: Amount is not sufficient\n", getpid());
            }
            else{
                printf("Client %d: Room %d reserved\n", getpid(), result);
            }
            return 0;
        }
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
        wait(NULL);
    }

    cl(fd,  &semaphores[NUM_SEMS], shared_data);


    return 0;
}