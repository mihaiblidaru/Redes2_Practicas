/**
 * @file config.c
 * @author Alberto Ayala
 * @brief Cargador de ajustes del programa
 * @version 1.0
 * @date 2019-05-06
 *
 * @copyright Copyright (c) 2019
 *
 */

#include <confuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "config.h"

/**
 * @brief Carga los ajustes del programa de un fichero .conf
 *
 * @param cfg_file Ruta al fichero que contiene la configuración del servidor
 * @param config Estructura en la que se guardarán los ajustes
 * @return int OK si se han cargardo los ajustes correctamente o ERR en caso contrario
 */
int load_server_config(const char *cfg_file, ServerCfg *config) {
    if (cfg_file == NULL || config == NULL) {
        printf("Error al cargar configuracion del servidor...\n");
        return ERR;
    }
    // Limpiamos la estructura
    memset(config, 0, sizeof(ServerCfg));

    // Definimos que opciones queremos leer del fichero y donde guardar los valores leidos
    cfg_opt_t opts[] = {CFG_SIMPLE_STR("server_root", &config->server_root),
                        CFG_SIMPLE_INT("backlog_size", &config->backlog_size),
                        CFG_SIMPLE_INT("max_clients", &config->max_clients),
                        CFG_SIMPLE_INT("listen_port", &config->listen_port),
                        CFG_SIMPLE_INT("keep_alive_timeout", &config->keep_alive_timeout),
                        CFG_SIMPLE_INT("max_requests_per_keep_alive_connection",
                                       &config->max_requests_per_keep_alive_connection),
                        CFG_SIMPLE_STR("server_signature", &config->server_signature),
                        CFG_SIMPLE_BOOL("debug_reuse_addr", &config->reuseaddr),
                        CFG_END()};

    cfg_t *cfg = NULL;

    // Cargamos los ajustes
    cfg = cfg_init(opts, 0);
    int res = cfg_parse(cfg, cfg_file);
    cfg_free(cfg);

    return res == CFG_SUCCESS ? OK : ERR;
}

/**
 * @brief Libera la memoría reservada al cargar los ajustes del programa.
 * Solo libera las cadenas, ya que solo se reserva memoria para ellas.
 *
 * @param config Estructura con la configuración del programa
 * @return int
 */
int free_server_config(ServerCfg *config) {
    if (config != NULL) {
        free(config->server_signature);
        free(config->server_root);
        return OK;
    };
    return ERR;
}

/**
 * @brief Imprime los ajustes del programa. Solo para
 * depuración
 *
 * @param out Stream usado para imprimir
 * @param config Estructura que contiene los datos del programa
 */
void print_server_config(FILE *out, ServerCfg *config) {
    fprintf(out, "Configuracion del servidor:\n");
    fprintf(out, "server_root: %s\n", config->server_root);
    fprintf(out, "backlog_size: %ld\n", config->backlog_size);
    fprintf(out, "max_clients: %ld\n", config->max_clients);
    fprintf(out, "listen_port: %ld\n", config->listen_port);
    fprintf(out, "keep_alive_timeout: %ld\n", config->keep_alive_timeout);
    fprintf(out, "max_requests_per_keep_alive_connection: %ld\n",
            config->max_requests_per_keep_alive_connection);
    fprintf(out, "server_signature: %s\n", config->server_signature);
}