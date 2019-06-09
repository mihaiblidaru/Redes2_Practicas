/**
 * @file http_utils.h
 * @author your name (you@domain.com)
 * @brief Funciones varias para parsear, buscar cabeceras,
 *   auxiliares etc, del servidor http/1.1
 * @version 0.1
 * @date 2019-02-16
 *
 * @copyright Copyright (c) 2019
 *
 */
#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include <time.h>
#include "http.h"


/**
 * @brief Decodifica caracteres especiales de la url. De momento
 * solo procesa espacios %20. De esta forma permitimos que los nombres
 * de los ficheros servidos puedan contener espacios.
 * 
 * @param path ruta http del recurso solicitado
 * @param path_len longitud de la ruta del recurso solicitado
 * @param new_path_len (RETURN) longitud de la ruta tras procesarla
 */
void decode_path(char* path, size_t path_len, size_t *new_path_len);

/**
 * @brief Dato el path de una peticion HTTP extrae
 * la extensión, el path real (sin incluir la query) y
 * la query de la petición get
 *
 * @param path Ruta de la peticion HTTP
 * @param path_len longitud de la ruta de la peticion HTTP
 * @param real_path_len (retorno) la longitud de la ruta real
 * @param query (retorno) Puntero al inicio de la query de la petición GET
 * @param query_len longitud de la query cuando el metodo es GET
 * @param extension estensión del fichero
 * @param extension_len longitud de la extensión del fichero
 * @return int OK al terminar de parsear el path
 */
int process_path(char *path, size_t path_len, size_t *real_path_len, char **query,
                 size_t *query_len, char **extension, size_t *extension_len);

/**
 * @brief Comprueba si un fichero existe y devuelve la información que
 * ofrece el sistema operativo de ese fichero
 *
 * @param file_exists (RETURN) 1 si el fichero existe o 0 en caso contrario
 * @param absolute_path Path al fichero que se quiere comprobar que exista
 * @param info Estructura en la que escribir los datos del fichero
 * @return int True si el fichero existe, False en caso contrario
 */
int check_file_exists(int *file_exists, char *absolute_path, struct stat *info);

/**
 * @brief Dada una petición HTTP parseada busca un header con un nombre concreto
 *
 * @param req Petición http recibida
 * @param name Nombre de la cabecera http a buscar
 * @return struct phr_header* Puntero a la cabecera encontrada
 */
struct phr_header *HttpRequestFindHeaderByName(HttpRequest *req, const char *name);

/**
 * @brief Dada una extensión, devuelve si esa extensión se
 * corresponde a un script.
 *
 * @param ext puntero a donde esté la extensión en memoria en formato ascii
 * @param ext_len longitud de la extensión. (la extensión puede no ser seguida
 * de un \0)
 * @return int 1 si la extensión se corresponde a un script o 0 en caso
 * contrario
 */
bool es_extension_script(const char *ext, size_t ext_len);

/**
 * @brief Añade una cabecera http a la respuesta http
 *
 * @param res objeto respuesta http
 * @param name nombre de la cabecera
 * @param value valor de la cabecera
 * @param value_is_malloced indica si el valor de la cabecera reside en una zona de
 * memoria reservada con malloc, o sea, en el heap.
 */
void response_add_header(HttpResponse *res, char *name, char *value,
                         bool value_is_malloced);

/**
 * @brief Dado un codigo de estado HTTP devuelve su representación
 * como cadena
 *
 * @param status HTTP status
 * @return const char* cadena correspondiente al status indicado o NULL si el
 * estado indicado no está soportado
 */
const char *get_status_as_text(int status);

/**
 * @brief Devuelve el tipo MIME asociado a una extension o
 * application/octet-stream cuando no se encuentra ninguna asociación
 *
 * @param extension extensión de la que se desea obtener el tipo MIME
 * @param ext_len longitud de la extensión
 * @return char* el tipo mime asociado a la extensión.
 */
char *get_mime_from_extension(char *extension, size_t ext_len);

/**
 * @brief libera los valores de las cabeceras de la respuesta http
 * apuntada por res que han sido previamente reservados con malloc
 *
 * @param res respuesta http
 */
void response_free_headers(HttpResponse *res);

/**
 * @brief Dato un timestamp, devuelve la fecha actual en formato especificado en
 * la rfc1123.
 *
 * @param t timestamp a partir del cual se va a generar la fecha
 * @return char* cadena que contiene la fecha en formato rfc1123
 */
char *get_date_from_timestamp(time_t t);

/**
 * @brief Dado un verbo HTTP representado por una cadena, devuelve
 * un identicador entero asociado a ese verbo
 *
 * @param method verbo HTTP
 * @param method_len longitud del verbo HTTP
 * @return int identificador asociado al verbo HTTP
 */
int get_method_id(const char *method, size_t method_len);

/**
 * @brief Devuelve el tipo de conexión solicitado en una petición HTTP:
 * keep-alive o close. Si con consigue encontrar la cabecera connection,
 * devuelve UNKNOWN,
 *
 * @param req Petición http
 * @return int Tipo de conexión solicitada
 */
int request_get_connection_type(HttpRequest *req);

/**
 * @brief Convierte una cadena de una longitud determinada
 *        en número. Util para cuando una cadena no es NULL terminated.
 * @param data buffer que contiene el número a convertir
 * @param length longitud de la cadena
 * @return int número parseado
 */
int atoin(char *data, int length);

#endif