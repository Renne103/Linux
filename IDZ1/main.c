#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define CHUNK_SIZE 5152 // block size for reading the string

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input file> <output file>\n", argv[0]);
        return 1;
    }
    char arr1[] = "Parentheses are nested correctly here";
    char arr2[] = "Parentheses are not nested correctly.";
    int fd1, fd2, fd12[2], fd23[2]; // file descriptors for unnamed channels
    pid_t pid1, pid2; // process IDs for three processes
    char *buffer = (char *) calloc(5152, sizeof(char)); // buffer for reading/writing characters
    if (buffer == NULL) {
        printf("Ошибка: не удалось выделить память для буфера.\n");
        return 1;
    }
    ssize_t read_bytes, write_size, pipe_size;

    // input and output file pointers
    char *in = (char *) calloc(5152, sizeof(char));
    char *out = (char *) calloc(5152, sizeof(char));

    strcpy(in,  argv[1]);
    strcpy(out, argv[2]);



    if (pipe(fd12) < 0) {
        printf("Can\'t create pipe\n");
        exit(-1);
    }
    pid1 = fork(); // create second child process

    if (pid1 < 0) {
        printf("Error with fork\n");
    } else if (pid1 > 0) { // first process
        if ((fd1 = open(in, O_CREAT | O_RDONLY, 0666)) <
            0) { //file pointer, flags, mode
            printf("Can\'t open file for reading\n");
            exit(-1);
        }
        do {
            read_bytes = read(fd1, buffer, CHUNK_SIZE);
            if (read_bytes == -1) {
                printf("Can\'t write this file\n");
                exit(-1);
            }
            buffer[read_bytes] = '\0';
        } while (read_bytes == CHUNK_SIZE);
        if (close(fd1) < 0) {
            printf("Can\'t close file\n");
        }

        close(fd12[0]);
        // write data from buffer into pipe
        pipe_size = write(fd12[1], buffer, strlen(buffer));
        if (pipe_size != strlen(buffer)) {
            printf("parent Can\'t write all string\n");
            exit(-1);
        }
        // end of parent work
        close(fd12[1]);
    } else if (pid1 == 0) { // second process
        // read data from pipe into buffer
        close(fd12[1]);
        pipe_size = read(fd12[0], buffer, CHUNK_SIZE);
        if (pipe_size < 0) {
            printf("Can\'t read string\n");
            exit(-1);
        }
        int count = 0;
        int fl = 0; //flag
        for (int i = 0; i < CHUNK_SIZE; ++i) {
            if (buffer[i] == '(') {
                count++;
            } else if (buffer[i] == ')') {
                count--;
                if (count < 0) {
                    fl = 1;
                    break;
                }
            }
        }
        close(fd23[0]); // close read end of channel 2

        // create third process
        if (pipe(fd23) < 0) {
            printf("Can\'t create pipe2\n");
            exit(-1);
        }

        pid2 = fork();

        if (pid2 > 0) {
            if (fl == 1) {
                close(fd12[0]); // close read end of channel 1
                pipe_size = write(fd23[1], arr2, strlen(arr2)); // write count to channel 2
                if (pipe_size != strlen(arr2)) {
                    printf("child Can\'t write all string (fl=1)\n");
                    exit(-1);
                }
                close(fd23[1]); // close write end of channel 2
            } else {
                close(fd12[0]); // close read end of channel 1
                pipe_size = write(fd23[1], arr1, strlen(arr1)); // write count to channel 2
                if (pipe_size != strlen(arr1)) {
                    printf("child Can\'t write all string (fl=0)\n");
                    exit(-1);
                }
                close(fd23[1]); // close write end of channel 2
            }
            //close(fd23[0]);
            char *arr = (char *) calloc(37, sizeof(char));
            //third process
        } else if (pid2 == 0) {
            if ((fd2 = open(out, O_CREAT | O_WRONLY, 0666)) <
                0) {  //file pointer, flags, mode
                printf("Can\'t open file for writing\n");
                exit(-1);
            }
            close(fd23[1]);
            char *arr = (char *) calloc(37, sizeof(char));
            // read data from channel to array
            pipe_size = read(fd23[0], arr, 37);
            if (pipe_size < 0) {
                printf("Can\'t read string\n");
                exit(-1);
            }
            close(fd23[0]);
            // write array into file
            write_size = write(fd2, arr, 37);

            if (write_size != 37) {
                printf("2nd child Can\'t write all string\n");
                exit(-1);
            }

            if (close(fd2) < 0) {
                printf("Can\'t close file\n");
            }
        }
    }
}