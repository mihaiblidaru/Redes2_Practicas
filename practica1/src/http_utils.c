/**
 * @file http_utils.c
 * @author AlbertoAyala
 * @author Mihai Blidaru
 * @brief Funciones varias para procesar elementos de una peticion HTTP
 * @version 1.0
 * @date 2019-02-16
 *
 * @copyright Copyright (c) 2019
 *
 */
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "common.h"
#include "http.h"
#include "picohttpparser.h"

/// Estructura para guardar los mapeos extensión <-> MIME
typedef struct {
    char *ext;   // Extensión
    char *mime;  // MIME
} ext_pair;

/**
 * @brief tipos de ficheros conocidos
 */
ext_pair known_extensions[70] = {
    {"js", "text/javascript"},
    {"htm", "text/html"},
    {"html", "text/html"},
    {"css", "text/css"},
    {"jpeg", "image/jpeg"},
    {"jpg", "image/jpeg"},
    {"png", "image/png"},
    {"pdf", "application/pdf"},
    {"aac", "audio/aac"},
    {"abw", "application/x-abiword"},
    {"arc", "application/x-freearc"},
    {"avi", "video/x-msvideo"},
    {"gz", "application/gzip"},
    {"azw", "application/vnd.amazon.ebook"},
    {"bin", "application/octet-stream"},
    {"bmp", "image/bmp"},
    {"bz", "application/x-bzip"},
    {"bz2", "application/x-bzip2"},
    {"csh", "application/x-csh"},
    {"csv", "text/csv"},
    {"doc", "application/msword"},
    {"docx",
     "application/"
     "vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {"eot", "application/vnd.ms-fontobject"},
    {"epub", "application/epub+zip"},
    {"gif", "image/gif"},
    {"ico", "image/vnd.microsoft.icon"},
    {"ics", "text/calendar"},
    {"jar", "application/java-archive"},
    {"json", "application/json"},
    {"mid", "audio/midi"},
    {"midi", "audio/midi"},
    {"mjs", "application/javascript"},
    {"mp3", "audio/mpeg"},
    {"mpeg", "video/mpeg"},
    {"mpkg", "application/vnd.apple.installer+xml"},
    {"odp", "application/vnd.oasis.opendocument.presentation"},
    {"ods", "application/vnd.oasis.opendocument.spreadsheet"},
    {"odt", "application/vnd.oasis.opendocument.text"},
    {"oga", "audio/ogg"},
    {"ogv", "video/ogg"},
    {"ogx", "application/ogg"},
    {"otf", "font/otf"},
    {"ppt", "application/vnd.ms-powerpoint"},
    {"pptx",
     "application/"
     "vnd.openxmlformats-officedocument.presentationml.presentation"},
    {"rar", "application/x-rar-compressed"},
    {"rtf", "application/rtf"},
    {"sh", "application/x-sh"},
    {"svg", "image/svg+xml"},
    {"swf", "application/x-shockwave-flash"},
    {"tar", "application/x-tar"},
    {"tif", "image/tiff"},
    {"tiff", "image/tiff	"},
    {"ttf", "font/ttf"},
    {"txt", "text/plain"},
    {"vsd", "application/vnd.visio"},
    {"wav", "audio/wav"},
    {"weba", "audio/webm"},
    {"webm", "video/webm"},
    {"webp", "image/webp"},
    {"woff", "font/woff"},
    {"woff2", "font/woff2"},
    {"xhtml", "application/xhtml+xml"},
    {"xls", "application/vnd.ms-excel"},
    {"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {"xml", "application/xml"},
    {"xul", "application/vnd.mozilla.xul+xml"},
    {"zip", "application/zip"},
    {"3gp", "video/3gpp"},
    {"3g2", "video/3gpp2"},
    {"7z", "application/x-7z-compressed"}};


/**
 * @brief Decodifica caracteres especiales de la url. De momento
 * solo procesa espacios %20. De esta forma permitimos que los nombres
 * de los ficheros servidos puedan contener espacios.
 * 
 * @param path ruta http del recurso solicitado
 * @param path_len longitud de la ruta del recurso solicitado
 * @param new_path_len (RETURN) longitud de la ruta tras procesarla
 */
void decode_path(char* path, size_t path_len, size_t *new_path_len){
    size_t i = 0, j=0;
    *(path+path_len) = '\0';
    //Convertimos a una cadena c
    char* tmp_path = strdup(path);
    memset(path, 0, path_len);

    while(i < path_len){
        if(strncmp(tmp_path + i, "%20", 3) == 0){
            path[j] = ' ';
            i += 3;
        } else {
            path[j] = tmp_path[i];
            i++;
        }
        j++;
    }
    *new_path_len = j;
    free(tmp_path);
}


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
                 size_t *query_len, char **extension, size_t *extension_len) {
    size_t i = 0;
    *real_path_len = 0;
    *extension_len = -1;

    // calcular longitud real del path
    for (i = 0; path[i] != '?' && i < path_len; i++) {
        *real_path_len += 1;
    }

    *(path + *real_path_len) = 0;  // cambiamos "?" por un "\0"

    // todo lo demás es la query
    *query = path + *real_path_len + 1;
    *query_len = path_len - *real_path_len;

    // buscar extensión
    for (i = *real_path_len; path[i] != '.' && path[i] != '/'; i--) {
        *extension_len += 1;
    }

    if (path[i] == '.') {
        // extensión encontrada
        *extension = &path[i + 1];
    } else if (path[i] == '/') {
        // extensión no encotrada(fichero sin extensión o directorio)
        *extension = path + *real_path_len;
        *extension_len = 0;
    }

    return OK;
}

/**
 * @brief Comprueba si un fichero existe y devuelve la información que
 * ofrece el sistema operativo de ese fichero
 *
 * @param file_exists (RETURN) 1 si el fichero existe o 0 en caso contrario
 * @param absolute_path Path al fichero que se quiere comprobar que exista
 * @param info Estructura en la que escribir los datos del fichero
 * @return int True si el fichero existe, False en caso contrario
 */
int check_file_exists(int *file_exists, char *absolute_path, struct stat *info) {
    struct stat tmp;
    struct stat *dest = info != NULL ? info : &tmp;
    int res = stat(absolute_path, dest);
    *file_exists = res ? 0 : 1;
    return res;
}

/**
 * @brief Dada una petición HTTP parseada busca un header con un nombre concreto
 *
 * @param req Petición http recibida
 * @param name Nombre de la cabecera http a buscar
 * @return struct phr_header* Puntero a la cabecera encontrada
 */
struct phr_header *HttpRequestFindHeaderByName(HttpRequest *req, const char *name) {
    size_t i;
    size_t header_name_length = strlen(name);

    for (i = 0; i < req->num_headers; i++) {
        if (req->headers[i].name_len == header_name_length &&
            strncmp(req->headers[i].name, name, header_name_length) == 0) {
            return &req->headers[i];
        }
    }
    return NULL;
}

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
bool es_extension_script(const char *ext, size_t ext_len) {
    switch (ext_len) {
        case 2:
            if (strcasecmp(ext, "py") == 0)
                return true;
            else if (strcasecmp(ext, "pl") == 0)
                return true;
            else
                return false;
        case 3:
            if (strcasecmp(ext, "php") == 0)
                return true;
            else
                return false;
        default:
            return false;
    }
}

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
                         bool value_is_malloced) {
    struct phr_header *header = &res->headers[res->num_headers];

    header->name = name;
    header->name_len = strlen(name);
    header->value = value;
    header->value_len = strlen(value);
    header->malloced = value_is_malloced;
    res->num_headers += 1;
}

/**
 * @brief Dado un codigo de estado HTTP devuelve su representación
 * como cadena
 *
 * @param status HTTP status
 * @return const char* cadena correspondiente al status indicado o NULL si el
 * estado indicado no está soportado
 */
const char *get_status_as_text(int status) {
    switch (status) {
        case 200:
            return "OK";
        case 301:
            return "Moved Permanently";
        case 304:
            return "Not Modified";
        case 400:
            return "Bad Request";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 411:
            return "Length Required";
        case 500:
            return "Internal Server Error";
        case 501:
            return "Not Implemented";
        default:
            return NULL;
    }
}

/**
 * @brief Devuelve el tipo MIME asociado a una extension o
 * application/octet-stream cuando no se encuentra ninguna asociación
 *
 * @param extension extensión de la que se desea obtener el tipo MIME
 * @param ext_len longitud de la extensión
 * @return char* el tipo mime asociado a la extensión.
 */
char *get_mime_from_extension(char *extension, size_t ext_len) {
    int i;

    if (ext_len != 0) {
        for (i = 0; i < 70; i++) {
            if (strncasecmp(extension, known_extensions[i].ext, ext_len) == 0) {
                return known_extensions[i].mime;
            }
        }
    }
    return "application/octet-stream";
}

/**
 * @brief libera los valores de las cabeceras de la respuesta http
 * apuntada por res que han sido previamente reservados con malloc
 *
 * @param res respuesta http
 */
void response_free_headers(HttpResponse *res) {
    int i;
    for (i = 0; i < res->num_headers; i++) {
        if (res->headers[i].malloced) free(res->headers[i].value);
    }
}

/**
 * @brief Dato un timestamp, devuelve la fecha actual en formato especificado en
 * la rfc1123.
 *
 * @param t timestamp a partir del cual se va a generar la fecha
 * @return char* cadena que contiene la fecha en formato rfc1123
 */
char *get_date_from_timestamp(time_t t) {
    struct tm datetime;
    char *res = calloc(35, sizeof(char));

    gmtime_r(&t, &datetime);
    strftime(res, 33, "%a, %d %b %Y %H:%M:%S GMT", &datetime);
    return res;
}

/**
 * @brief Dado un verbo HTTP representado por una cadena, devuelve
 * un identicador entero asociado a ese verbo
 *
 * @param method verbo HTTP
 * @param method_len longitud del verbo HTTP
 * @return int identificador asociado al verbo HTTP
 */
int get_method_id(const char *method, size_t method_len) {
    if (strncmp(method, "GET", method_len) == 0) {
        return METHOD_GET;
    } else if (strncmp(method, "POST", method_len) == 0) {
        return METHOD_POST;
    } else if (strncmp(method, "HEAD", method_len) == 0) {
        return METHOD_HEAD;
    } else if (strncmp(method, "OPTIONS", method_len) == 0) {
        return METHOD_OPTIONS;
    }
    return METHOD_UNKNOWN;
}

/**
 * @brief Devuelve el tipo de conexión solicitado en una petición HTTP:
 * keep-alive o close. Si con consigue encontrar la cabecera connection,
 * devuelve UNKNOWN,
 *
 * @param req Petición http
 * @return int Tipo de conexión solicitada
 */
int request_get_connection_type(HttpRequest *req) {
    struct phr_header *conn_header = HttpRequestFindHeaderByName(req, "Connection");
    if (conn_header != NULL) {
        if (strncasecmp(conn_header->value, "keep-alive", conn_header->value_len) == 0) {
            return CONNECTION_KEEPALIVE;
        } else if (strncasecmp(conn_header->value, "close", conn_header->value_len)) {
            return CONNECTION_CLOSE;
        }
    }
    return UNKNOWN;
}

/**
 * @brief Convierte una cadena de una longitud determinada
 *        en número. Util para cuando una cadena no es NULL terminated.
 * @param data buffer que contiene el número a convertir
 * @param length longitud de la cadena
 * @return int número parseado
 */
int atoin(char *data, int length) {
    char tmp[20] = {0};
    strncpy(tmp, data, length);
    return atoi(tmp);
}