#ifndef SERVER_H
#define SERVER_H

#include "config.h"

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
int bind_server(ServerCfg* cfg);

/**
 * @brief Arranca el servidor TCP y se queda acceptando conexiones para siempre.
 *
 * @param cfg Configuración global del servidor
 * @param sockfd socket en el que se esperan conexiones
 * @param process_request función que atenderá la petición
 * @return int OK (no debería devolver nunca)
 */
void run_server( ServerCfg *cfg, int sockfd, int (*process_request)(ServerCfg *config, int clientfd));

#endif