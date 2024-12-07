#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/select.h>
#include <errno.h>

#define MAX_CLIENTS 1

volatile sig_atomic_t wasSigHup = 0;

void sigHupHandler(int r) {
    wasSigHup = 1;
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addrs[MAX_CLIENTS];
    socklen_t server_addr_len = sizeof(server_addr);
    int clients[MAX_CLIENTS] = {0};  
    int PORT = atoi(argv[1]);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT); 

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, MAX_CLIENTS) == -1) {
        perror("Listen failed");
        close(server_fd);
        return 1;
    }

    struct sigaction sa;
    sigaction(SIGHUP, NULL, &sa);
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART;
    sigaction(SIGHUP, &sa, NULL);

    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    if (sigprocmask(SIG_BLOCK, &blockedMask, &origMask) == -1) {
        perror("sigprocmask error");
        return 1;
    }

    printf("Server listening on port %d\n", PORT);
    struct timespec timeout = {5, 0}; 

    while (true) {
        fd_set fds; 
        FD_ZERO(&fds); 
        FD_SET(server_fd, &fds);
        int maxFd = server_fd; 

        for (int i = 0; i < MAX_CLIENTS; i++) { 
            if (clients[i] > 0) { 
                FD_SET(clients[i], &fds); 
                if (clients[i] > maxFd) { 
                    maxFd = clients[i]; 
                }  
            }
        }

        int pselect_flag = pselect(maxFd + 1, &fds, NULL, NULL, &timeout, &origMask);

        if (pselect_flag == -1) {
            if (errno == EINTR){
                if (wasSigHup) {
                    printf("Received SIGHUP signal\n");
                    wasSigHup = 0;
                    continue;
                }
            }else {
                perror("pselect error");
                break;
            } 
        } else if (pselect_flag == 0) {
            printf("No activity on sockets\n");
        } else {
            
            if (FD_ISSET(server_fd, &fds)) {
                int new_client_index = -1; 
                for (int i = 0; i < MAX_CLIENTS; i++) { 
                    if (clients[i] == 0) { 
                        new_client_index = i;
                        break;
                    }
                }

                if (new_client_index != -1) { 
                    if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addrs[new_client_index], &server_addr_len)) == -1) {
                        perror("Accept failed"); 
                        return 1;
                    } else {
                        char client_ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &client_addrs[new_client_index].sin_addr, client_ip, sizeof(client_ip));
                        int client_port = ntohs(client_addrs[new_client_index].sin_port);
                        
                        clients[new_client_index] = client_fd;
                        printf("New connection accepted from %s:%d\n", client_ip, client_port);
                    }
                } else {
                    
                    struct sockaddr_in temp_client_addr;
                    socklen_t temp_client_addr_len = sizeof(temp_client_addr);
                    int temp_client_fd = accept(server_fd, (struct sockaddr *)&temp_client_addr, &temp_client_addr_len);
                    if (temp_client_fd != -1) {
                        char temp_client_ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &temp_client_addr.sin_addr, temp_client_ip, sizeof(temp_client_ip));
                        int temp_client_port = ntohs(temp_client_addr.sin_port);

                        printf("Connection from %s:%d rejected. Maximum clients reached.\n", temp_client_ip, temp_client_port);
                        
                        close(temp_client_fd);
                    }
                }
            }

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] > 0 && FD_ISSET(clients[i], &fds)) {
                char buffer[1024] = {0};                   
                ssize_t valread = read(clients[i], buffer, sizeof(buffer));
               
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addrs[i].sin_addr, client_ip, sizeof(client_ip));
                int client_port = ntohs(client_addrs[i].sin_port);

                    if (valread == -1) {
                        perror("Recieve failed");
                    } else if (valread == 0) {
                        printf("Connection closed by a client %s:%d\n", client_ip, client_port);
                        close(clients[i]);
                        clients[i] = 0;
                    } else if (valread > 0) {
                        printf("Received data from client %s:%d: %s", client_ip, client_port, buffer);
                    }
                }
            }
        }
    }

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] > 0) {
            close(clients[i]);
        }
    }

    close(server_fd);

    return 0;
}