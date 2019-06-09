
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

#include "common.h"
#include "config.h"
#include "tcp_server.h"

/**
 * @brief Crea el socket y reserva el puerto para
 * la escucha de peticiones
 *
 * @param cfg Configuración del servidor. En esta función se
 * usa para configurar el puerto de escucha, el tamaño
 * máximo del backlog TCP y la opcion SO_REUSEADDR (solo
 * activa para depurar)
 *
 * @return int OK si el socket se ha reservado correctamente
 * o ERR en caso contrario
 */
int bind_server(ServerCfg *cfg) {
    struct sockaddr_in self;
    int sockfd;
    // Creamos el socket tipo TCP */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error en la creación del socket");
        return ERR;
    }

    if (cfg->reuseaddr) {
        fprintf(stderr, "[DEBUG] setsockopt(SO_REUSEADDR)\n");
        int enable = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
            perror("setsockopt(SO_REUSEADDR) failed");
    }

    // Inicializamos estructura de dirección y puerto
    bzero(&self, sizeof(self));
    self.sin_family = AF_INET;
    self.sin_port = htons(cfg->listen_port);
    self.sin_addr.s_addr = INADDR_ANY;

    // Ligamos puerto al socket
    if (bind(sockfd, (struct sockaddr *)&self, sizeof(self)) != 0) {
        perror("socket--bind");
        close(sockfd);
        return ERR;
    }

    // OK, listos para escuchar...
    if (listen(sockfd, cfg->backlog_size) != 0) {
        perror("socket--listen");
        exit(errno);
    }

    printf("Escuchando en [%s:%ld]...\n", inet_ntoa(self.sin_addr), cfg->listen_port);

    return sockfd;
}
