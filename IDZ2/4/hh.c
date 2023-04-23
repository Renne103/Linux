#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sem.h>
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

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int cl(int fd,int semid, shared_data_t *shared_data){
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

    res = semctl(semid, 0, IPC_RMID);
    if (res == -1) {
        perror("semctl IPC_RMID failed");
        return -1;
    }
}

void handle_sigint(int signal, int fd,int semid, shared_data_t *shared_data) {
    cl(fd, semid, shared_data);
    exit(0);
}

int reserve_room(shared_data_t *shared_data, int room_type, int amount, int semid) {
    int num_rooms, room_price, i, can_reserve = 0, room_num;
    int sem_num;
    if (room_type == 1) {
        room_price = 2000;
    } else if (room_type == 2) {
        room_price = 4000;
    } else if (room_type == 3) {
        room_price = 6000;
    }
    if (amount < 2000) {
        return -3;
    }
    sem_num = room_type - 1;
    struct sembuf sops[1] = {{sem_num, -1, SEM_UNDO}};
    while (semop(semid, sops, 1) == -1 && errno == EINTR); // Handle signals
    for (i = 0; i < NUM_ROOMS; i++) {
        if (shared_data->rooms[i].room_type == room_type &&
            shared_data->rooms[i].is_free == 1) {
            can_reserve = 1;
            room_num = shared_data->rooms[i].room_num;
            shared_data->rooms[i].is_free = 0;
            shared_data->num_free_rooms--;
            break;
        }
    }
    sops[0] = (struct sembuf) {sem_num, 1, SEM_UNDO};
    semop(semid, sops, 1);

    if (!can_reserve) {
        // If no rooms of the selected type are available, try to find a room of a different type
        if (room_type == 1 && shared_data->num_rooms_2 > 0 && amount >= 4000) {
            room_type = 2;
            sem_num = 1;
        } else if (room_type == 1 && shared_data->num_rooms_3 > 0 && amount >= 6000) {
            room_type = 3;
            sem_num = 2;
        } else if (room_type == 2 && shared_data->num_rooms_1 > 0 && amount >= 2000) {
            room_type = 1;
            sem_num = 0;
        } else if (room_type == 2 && shared_data->num_rooms_3 > 0 && amount >= 6000) {
            room_type = 3;
            sem_num = 2;
        } else if (room_type == 3 && shared_data->num_rooms_1 > 0 && amount >= 2000) {
            room_type = 1;
            sem_num = 0;
        } else if (room_type == 3 && shared_data->num_rooms_2 > 0 && amount >= 4000) {
            room_type = 2;
            sem_num = 1;
        } else {
            return -2; // No available rooms of any type
        }

        // Try to reserve a room of the new type
        struct sembuf sops[1] = {{sem_num, -1, SEM_UNDO}};
        while (semop(semid, sops, 1) == -1 && errno == EINTR); // Handle signals
        for (i = 0; i < NUM_ROOMS; i++) {
            if (shared_data->rooms[i].room_type == room_type &&
                shared_data->rooms[i].is_free == 1) {
                can_reserve = 1;
                room_num = shared_data->rooms[i].room_num;
                shared_data->rooms[i].is_free = 0;
                shared_data->num_free_rooms--;
                break;
            }
        }
        sops[0] = (struct sembuf) {sem_num, 1, SEM_UNDO};
        semop(semid, sops, 1);

        if (!can_reserve) {
            return -2;
        }
    }


    sops[0] = (struct sembuf) {3, -1, SEM_UNDO};
    while (semop(semid, sops, 1) == -1 && errno == EINTR); // Handle signals
    if (room_type == 1) {
        shared_data->num_rooms_1--;
    } else if (room_type == 2) {
        shared_data->num_rooms_2--;
    } else if (room_type == 3) {
        shared_data->num_rooms_3--;
    }
    sops[0] = (struct sembuf) {3, 1, SEM_UNDO};
    semop(semid, sops, 1);
    return room_num;
}




int release_room(shared_data_t *shared_data, int room_num, int semid) {
    int i, room_type, sem_num;
    for (i = 0; i < NUM_ROOMS; i++) {
        if (shared_data->rooms[i].room_num == room_num) {
            room_type = shared_data->rooms[i].room_type;
            shared_data->rooms[i].is_free = 1;
            shared_data->num_free_rooms++;
            break;
        }
    }
    sem_num = room_type - 1;
    struct sembuf sops[1] = {{sem_num, -1, SEM_UNDO}};
    while (semop(semid, sops, 1) == -1 && errno == EINTR); // Handle signals
    if (room_type == 1) {
        shared_data->num_rooms_1++;
    } else if (room_type == 2) {
        shared_data->num_rooms_2++;
    } else if (room_type == 3) {
        shared_data->num_rooms_3++;
    }
    sops[0] = (struct sembuf) {sem_num, 1, SEM_UNDO};
    semop(semid, sops, 1);

    return 0;
}

int main(int argc, char *argv[]) {
    int fd, amount, i, j, semid;
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


    fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0666);
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

    semid = semget(IPC_PRIVATE, NUM_SEMS, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget failed");
        return -1;
    }

    union semun arg;
    unsigned short init_values[NUM_SEMS] = {10, 10, 5, 1};
    arg.array = init_values;
    res = semctl(semid, 0, SETALL, arg);
    if (res == -1) {
        perror("semctl SETALL failed");
        return -1;
    }
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
            int result = reserve_room(shared_data, room_type, money, semid);
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

    cl(fd, semid, shared_data);


    return 0;
}
