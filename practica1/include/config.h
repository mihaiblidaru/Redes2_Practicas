
#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdio.h>
#include "confuse.h"

/// Estructura para guardar los ajustes del servidor
typedef struct {
    char *server_root;            // Ruta desde la cual se sirven archivos
    long int backlog_size;        // Tamaño del backlog tcp
    long int max_clients;         // Número máximo de conexiones tcp
    long int listen_port;         // Puerto en el que se escucha
    long int keep_alive_timeout;  // Keep-alive timeout
    long int max_requests_per_keep_alive_connection;  // Numero máximo de conexiones por
                                                      // Petición de tipo keep-alive
    char *server_signature;                           // Nombre del servidor
    cfg_bool_t reuseaddr;                             // (DEBUG) flag que indica que se
} ServerCfg;

int load_server_config(const char *cfg_file, ServerCfg *config);
int free_server_config(ServerCfg *config);
void print_server_config(FILE *out, ServerCfg *config);

#endif
