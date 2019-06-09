/**
 * @file tcp_server_thread.c
 * @author Mihai Blidaru
 * @brief Servidor tcp que lanza un hilo por cada petición recibida.
 * @version 0.1
 * @date 2019-05-06
 *
 * @copyright Copyright (c) 2019
 *
 */

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <resolv.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "config.h"
#include "qsem.h"
#include "tcp_server.h"

/// Identificador del semaforo que controla el número máximo de clientes
#define NUM_CLIENTS_LOCK 0

int (*_pr)(ServerCfg *config,
           int clientfd);  // Puntero a la función que atiende una petición
ServerCfg *_cfg;           // Estructura con la configuración del servidor
int _semid;                // Identificador del array de semaforos

/**
 * @brief Hilo que procesa las peticiones http
 *
 * @param arg descriptor de la conexión tcp con el cliente
 * @return void* NULL
 */
static void *worker(void *arg) {
    int clientfd = (uintptr_t)arg;
    _pr(_cfg, clientfd);
    close(clientfd);
    qsem_leave(_semid, NUM_CLIENTS_LOCK);
    pthread_exit(NULL);
}

/**
 * @brief Arranca el servidor TCP y se queda acceptando conexiones para siempre.
 *
 * @param cfg Configuración global del servidor
 * @param sockfd socket en el que se esperan conexiones
 * @param process_request función que atenderá la petición
 * @return int OK (no debería devolver nunca)
 */
void run_server(ServerCfg *cfg, int sockfd,
               int (*process_request)(ServerCfg *config, int clientfd)) {
    _pr = process_request;
    _cfg = cfg;
    int i;

    fclose(fopen("/tmp/semlock", "w"));

    // un semaforo para controlar el número de clientes
    _semid = qsem_init("/tmp/semlock", 'q', 1, true);

    // ajustamos su valor inicial
    for (i = 1; i < cfg->max_clients; i++) {
        qsem_leave(_semid, NUM_CLIENTS_LOCK);
    }

    while (true) {
        int clientfd = -1;
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &addrlen);
        if (clientfd != -1) {
            uintptr_t thread_connfd = clientfd;
            pthread_t t;
            qsem_enter(_semid, NUM_CLIENTS_LOCK);
            pthread_create(&t, NULL, &worker, (void *)thread_connfd);
            pthread_detach(t);
        }
    }

}