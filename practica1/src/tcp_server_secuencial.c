
/**
 * @file tcp_server_secuencial.c
 * @author Alberto Ayala
 * @brief Servidor tcp secuencias
 * @version 0.1
 * @date 2019-05-06
 * 
 * @copyright Copyright (c) 2019
 * 
 */
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
 * @brief Servidor tcp secuencial, usado solo para depuración
 * 
 * @param cfg Configuración del servidor
 * @param sockfd socket usado para recibir conexiones
 * @param process_request función que procesa las peticiones
 * @return int 
 */
void run_server(ServerCfg *cfg, int sockfd,
               int (*process_request)(ServerCfg *config, int clientfd)) {
    while (1) {
        int clientfd = -1;
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &addrlen);
        if (clientfd != -1) {
            process_request(cfg, clientfd);
            close(clientfd);
        }
    }
}