#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT 8080

volatile sig_atomic_t wasSigHup = 0;

void sigHupHandler(int sig) {
    wasSigHup = 1;
}

int main() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket, 1) < 0) {
        perror("listen");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    printf("Сервер запущен на порту %d. Ожидание соединений...\n", PORT);

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sigHupHandler;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("sigaction");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    if (sigprocmask(SIG_BLOCK, &blockedMask, &origMask) == -1) {
        perror("sigprocmask");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    int clientSocket = -1;

    while (1) {
        fd_set readFds;
        FD_ZERO(&readFds);
        FD_SET(serverSocket, &readFds);
        int maxFd = serverSocket;

        if (clientSocket != -1) {
            FD_SET(clientSocket, &readFds);
            if (clientSocket > maxFd)
                maxFd = clientSocket;
        }

        int ready = pselect(maxFd + 1, &readFds, NULL, NULL, NULL, &origMask);
        if (ready == -1) {
            if (errno == EINTR) {
                if (wasSigHup) {
                    wasSigHup = 0;
                    printf("Получен и обработан сигнал SIGHUP\n");
                }
                continue;
            } else {
                perror("pselect");
                break;
            }
        }

        if (FD_ISSET(serverSocket, &readFds)) {
            if (clientSocket == -1) {
                struct sockaddr_in clientAddr;
                socklen_t clientLen = sizeof(clientAddr);
                clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientLen);
                if (clientSocket >= 0) {
                    char ipStr[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
                    printf("Новое соединение от %s:%d\n", ipStr, ntohs(clientAddr.sin_port));
                } else {
                    perror("accept");
                }
            } else {
                int tmp = accept(serverSocket, NULL, NULL);
                if (tmp != -1) {
                    printf("Отклонено дополнительное соединение\n");
                    close(tmp);
                }
            }
        }

        if (clientSocket != -1 && FD_ISSET(clientSocket, &readFds)) {
            char buffer[1024];
            ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
            if (bytesRead > 0) {
                printf("Получено %zd байт от клиента\n", bytesRead);
            } else if (bytesRead == 0) {
                printf("Клиент отключился\n");
                close(clientSocket);
                clientSocket = -1;
            } else {
                perror("read");
                close(clientSocket);
                clientSocket = -1;
            }
        }
    }

    if (clientSocket != -1) close(clientSocket);
    close(serverSocket);
    return 0;
}
