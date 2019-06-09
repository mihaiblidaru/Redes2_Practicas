
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"
#include "tcp_server.h"

/**
 * Arranca el servidor TCP y se queda acceptando conexiones para siempre.
 * cfg: Configuración global del servidor
 * process_request: Función de atención a la conexion
 */
void run_server(ServerCfg *cfg, int sockfd,
               int (*process_request)(ServerCfg *config, int clientfd)) {
    while (1) {
        int clientfd = -1;
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &addrlen);
        if (clientfd != -1) {
            if (fork() == 0) {
                process_request(cfg, clientfd);
                close(clientfd);
                exit(0);
            }
            close(clientfd);
        }
    }

}