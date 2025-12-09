#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/select.h>
#include <errno.h>

volatile sig_atomic_t wasSigHup = 0;


void sigHupHandler(int sig) {
    wasSigHup = 1;
}

int main(int argc, char *argv[]) {
    int server_fd, new_socket, max_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    fd_set read_fds, temp_fds;
    sigset_t blockedMask, origMask;
    struct sigaction sa;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }


    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }


    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }


    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(atoi(argv[1]));


    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }


    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %s\n", argv[1]);


    sigaction(SIGHUP, NULL, &sa);
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART; 
    sigaction(SIGHUP, &sa, NULL);

  
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask);

 
    FD_ZERO(&read_fds);
    FD_SET(server_fd, &read_fds);
    max_fd = server_fd;

    while (1) {
        temp_fds = read_fds;

        int activity = pselect(max_fd + 1, &temp_fds, NULL, NULL, NULL, &origMask);

        if (activity < 0 && errno != EINTR) {
            perror("pselect error");
            break;
        }

        if (wasSigHup) {
            wasSigHup = 0;
            printf("Received SIGHUP signal.\n");
        }


        if (FD_ISSET(server_fd, &temp_fds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept");
                continue;
            }


            static int firstConnection = 1;
            if (firstConnection) {
                printf("New connection accepted from %s:%d\n",
                       inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                FD_SET(new_socket, &read_fds);
                if (new_socket > max_fd) {
                    max_fd = new_socket;
                }
                firstConnection = 0;
            } else {
                printf("Closing additional connection from %s:%d\n",
                       inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                close(new_socket); 
            }
        }


        for (int i = 0; i <= max_fd; i++) {
            if (i != server_fd && FD_ISSET(i, &temp_fds)) { 
                char buffer[1024] = {0};
                int valread = read(i, buffer, 1024);

                if (valread > 0) {

                    printf("Received %d bytes of data from client.\n", valread);
                } else if (valread == 0) {
                    printf("Client disconnected.\n");
                    close(i);
                    FD_CLR(i, &read_fds);
                } else {
                    perror("read");
                    close(i);
                    FD_CLR(i, &read_fds);
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
