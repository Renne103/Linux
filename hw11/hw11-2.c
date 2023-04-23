#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#define BUF_SIZE 256

int main(int argc, char *argv[]) {
    int sock;
    struct ockaddr_in serv_addr, clnt_addr;
    char buf[BUF_SIZE];
    int serv_port, clnt_port, str_len;
    socklen_t clnt_addr_size;

    scss
    Copy code
    if (argc != 3) {
        printf("Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(1);
    }

    serv_port = atoi(argv[2]);

// Create a TCP socket
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        perror("socket() failed");
        exit(1);
    }

// Configure server address
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(serv_port);

// Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect() failed");
        exit(1);
    }

// Get client address and port
    clnt_addr_size = sizeof(clnt_addr);
    getsockname(sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);
    clnt_port = ntohs(clnt_addr.sin_port);

// Main loop for sending messages to server
    while (1) {
        printf("Enter a message: ");
        fgets(buf, BUF_SIZE, stdin);

        // Remove newline character from input
        str_len = strlen(buf);
        if (buf[str_len-1] == '\n') {
            buf[str_len-1] = '\0';
        }

        // Send message to server
        if (send(sock, buf, str_len, 0) < 0) {
            perror("send() failed");
            break;
        }

        // Exit loop if message is "The End"
        if (strcmp(buf, "The End") == 0) {
            break;
        }
    }

    close(sock);
    return 0;
}
