#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#define BUF_SIZE 256

int main(int argc, char *argv[]) {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    char buf[BUF_SIZE];

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // Создаем сокет
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket failed");
        exit(1);
    }

    // Задаем адрес и порт сервера
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(argv[1]));

    // Привязываем сокет к адресу и порту
    if (bind(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(1);
    }

    // Начинаем прослушивание
    if (listen(server_sock, 1) < 0) {
        perror("listen failed");
        exit(1);
    }

    printf("Server started on port %s\n", argv[1]);

    // Принимаем соединения и обрабатываем запросы
    while (1) {
        client_addr_len = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("accept failed");
            continue;
        }

        printf("Connection from %s:%d accepted\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Обрабатываем запросы
        while (1) {
            int n = recv(client_sock, buf, BUF_SIZE, 0);
            if (n < 0) {
                perror("recv failed");
                break;
            } else if (n == 0) {
                printf("Connection from %s:%d closed\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                break;
            }

            buf[n] = '\0';
            printf("Received message from %s:%d: %s", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buf);

            // Отправляем сообщение клиенту №2
            send(client_sock, buf, strlen(buf), 0);
            printf("Message forwarded to client 2\n");

            // Проверяем, не было ли передано сообщение "The End"
            if (strcmp(buf, "The End\n") == 0) {
                printf("Closing connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                close(client_sock);
                break;
            }
        }
    }

    return 0;
}
