
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
#include "qsem.h"

#include "config.h"
#include "tcp_server.h"


// Identificadores de los semáforos en el array de semáforos
#define ACCEPT_LOCK 0
#define NUM_CLIENTS_LOCK 1

int worker_process(int sockfd);

int (*_pr)(ServerCfg *config, int clientfd);
ServerCfg *_cfg;
int _semid;

/**
 * @brief Hilo que procesa las conexiones recibidas
 * 
 * @param arg descriptor de la conexión con un cliente casteada a uintptr_t
 * @return void* 
 */
static void *worker_thread(void *arg) {
    int clientfd = (uintptr_t)arg;
    _pr(_cfg, clientfd);
    close(clientfd);
    qsem_leave(_semid, NUM_CLIENTS_LOCK);
    pthread_exit(NULL);
}

/**
 * Arranca el servidor TCP y se queda acceptando conexiones para siempre.
 * cfg: Configuración global del servidor
 * process_request: Función de atención a la conexion
 */
void run_server(ServerCfg *cfg, int sockfd,
               int (*process_request)(ServerCfg *config, int clientfd)) {
    _pr = process_request;
    _cfg = cfg;

    int num_worker_processes = 5, i;

    fclose(fopen("/tmp/semlock", "w"));

    // dos semaforos, uno para la entrada en el accept y otro
    // para controlar el número de clientes activos
    _semid = qsem_init("/tmp/semlock", 'q', 2, true);

    for (i = 1; i < cfg->max_clients; i++) {
        qsem_leave(_semid, NUM_CLIENTS_LOCK);
    }


    // prefork procesos
    for (i = 0; i < num_worker_processes; i++) {
        if (fork() == 0) {
            worker_process(sockfd);
            return;
        }
    }

    for (i = 0; i < num_worker_processes; i++) {
        wait(NULL);
    }

}

int worker_process(int sockfd) {
    while (1) {
        int clientfd = -1;
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        
        // Aceptamos la conexión
        qsem_enter(_semid, ACCEPT_LOCK);
        clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &addrlen);
        qsem_leave(_semid, ACCEPT_LOCK);

        // Y lanzamos el hilo que la va a atenter
        if (clientfd != -1) {
            qsem_enter(_semid, NUM_CLIENTS_LOCK);
            uintptr_t thread_connfd = clientfd;
            pthread_t t;
            pthread_create(&t, NULL, &worker_thread, (void *)thread_connfd);
            pthread_detach(t);
        }
    }
}