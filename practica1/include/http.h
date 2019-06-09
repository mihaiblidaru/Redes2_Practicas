/**
 * @file http.h
 * @author Alberto Ayala
 * @brief Define la estructura de una petición HTTP. Tipos de metodos y conexiones
 * soportadas.
 * @version 0.1
 * @date 2019-05-06
 *
 * @copyright Copyright (c) 2019
 *
 */

#ifndef HTTP_H
#define HTTP_H

#include <config.h>
#include <picohttpparser.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

/// Número máximo de cabeceras http que se esperan
#define HTTP_RESPONSE_MAX_HEADERS 20

/// Tipos de métodos soportados
#define METHOD_UNKNOWN -1
#define METHOD_HEAD 1
#define METHOD_GET 2
#define METHOD_POST 3
#define METHOD_OPTIONS 4

/// Tipos de conexiones soportadas
#define CONNECTION_KEEPALIVE 9
#define CONNECTION_CLOSE 10
#define UNKNOWN -1

/// Estructura para guardar los datos de la petición http
typedef struct _http_request {
    char *_method;                  // Verbo HTTP como cadena
    size_t method_len;              // Longitud de la cadena que contiene el verbo http
    int method;                     // Metodo parseado
    char *path;                     // Path de la peticion http
    size_t path_len;                // Longitud de la ruta http
    char *_path;                    // Copia del path original recibido
    size_t real_path_len;           // longitud de la ruta sin query
    char *extension;                // extensión del fichero pedido
    size_t extension_len;           // longitud de la extensión
    int minor_version;              // Versión del protocolo http
    struct phr_header headers[20];  // Cabeceras http recibidas
    size_t num_headers;             // número de cabeceras
    struct stat file_info;          // Información del fichero pedido en disco
    char *query;                    // Parametros de la petición GET
    size_t query_len;               // longitud de los parametros GET
    int file_exists;                // indica si el fichero pedido existe
    int connection_type;            // tipo de conexión http
    char *post_data;                // datos de la petición post
} HttpRequest;

/// Estructura para guardar los datos de la respuesta http antes de enviarla
typedef struct _http_response {
    int minor_version;                                     // Versión del protocolo http
    int status;                                            // codigo de estado
    int num_headers;                                       // número de cabeceras http
    struct phr_header headers[HTTP_RESPONSE_MAX_HEADERS];  // cabeceras de la respuestas
} HttpResponse;

/**
 * @brief Punto de entrada para procesar una petición http.
 * Esta función es invocada tras recibir una conexión.
 *
 * @param cfg Configuración del servidor
 * @param clientfd descriptor de la conexión con el cliente
 * @return int OK si la petición se ha procesado correctamente o ERR en caso contrario
 */
int process_http_request(ServerCfg *cfg, int clientfd);

/**
 * @brief Lista un directorio en html
 *
 * @param cfg Configuración del programa
 * @param req Petición http
 * @param res Estructura con los datos de la respuesta que se va a enviar
 * @param clientfd descriptor de la conexión tcp con el cliente
 * @return int OK si se ha enviado el listado correctamente o ERR en caso contrario
 */
int list_dir(ServerCfg *cfg, HttpRequest *req, HttpResponse *res, int clientfd);

/**
 * @brief Envia la cabecera de la petición HTTP.
 * Esta función se debe llamar solo una vez por petición HTTP y solo
 * cuando se sabe exactamente cual es el contenido del mensaje http.
 *
 * @param cfg Configuración del servidor
 * @param res Estructura de la respuesta http que contiene las cabeceras a enviar
 * @param clientfd conexión tcp con el cliente
 */
void response_send_header(ServerCfg *cfg, HttpResponse *res, int clientfd);

#endif
