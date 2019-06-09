/**
 * @file http.c
 * @author Alberto Ayala
 * @author Mihai Blidaru
 * @brief Modulo que procesa las peticiones http. Realiza el parseo y genera la respuesta
 * correspondiente
 * @version 1.0
 * @date 2019-03-07
 *
 * @copyright Copyright (c) 2019
 *
 */

#define _XOPEN_SOURCE 700

#include "http.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "config.h"
#include "http_utils.h"
#include "picohttpparser.h"

/* Plantillas para mensajes de error*/
#define TEMPLATE_404                                              \
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"      \
    "<html><head>\n"                                              \
    "<title>404 Not Found</title>\n"                              \
    "</head><body>\n"                                             \
    "<h1>Not Found</h1>\n"                                        \
    "<p>The requested URL %s was not found on this server.</p>\n" \
    "<hr>\n"                                                      \
    "<address>%s at 127.0.0.1 Port %ld</address>\n"               \
    "</body></html>\n"

#define TEMPLATE_403                                                  \
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"          \
    "<html><head>\n"                                                  \
    "<title>403 Forbidden</title>\n"                                  \
    "</head><body>\n"                                                 \
    "<h1>Forbidden</h1>\n"                                            \
    "<p>You don't have permission to access %s on this server.</p>\n" \
    "<hr>\n"                                                          \
    "<address>%s at 127.0.0.1 Port %ld</address>\n"                   \
    "</body></html>\n"

#define TEMPLATE_501                                         \
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n" \
    "<html><head>\n"                                         \
    "<title>501 Method Not Implemented</title>\n"            \
    "</head><body>\n"                                        \
    "<h1>501 Method Not Implemented</h1>\n"                  \
    "<p>501 Method Not Implemented</p>\n"                    \
    "<hr>\n"                                                 \
    "<address>%s at 127.0.0.1 Port %ld</address>\n"          \
    "</body></html>\n"

#define TEMPLATE_500                                                             \
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"                     \
    "<html><head>\n"                                                             \
    "<title>500 Internal Server Error</title>\n"                                 \
    "</head><body>\n"                                                            \
    "<h1>500 Internal Server Error</h1>\n"                                       \
    "<p>The server encountered an internal error accesing %s and was unable to " \
    "complete your request.\n"                                                   \
    "<hr>\n"                                                                     \
    "<address>%s at 127.0.0.1 Port %ld</address>\n"                              \
    "</body></html>\n"

// Tamaño de bloque usado para enviar un fichero
#define FILE_CHUNK_SIZE 512

int send_response(ServerCfg *cfg, HttpRequest *req, HttpResponse *res, int clientfd);
int send_file(ServerCfg *cfg, HttpRequest *req, HttpResponse *res, int clientfd);
int send_error(ServerCfg *cfg, HttpRequest *req, HttpResponse *res, int clienfd,
               int codigo);

int run_script(ServerCfg *cfg, HttpRequest *req, HttpResponse *res, int clientfd);
int send_dir(ServerCfg *cfg, HttpRequest *req, HttpResponse *res, int clientfd);

/**
 * @brief Punto de entrada para procesar una petición http.
 * Esta función es invocada tras recibir una conexión.
 *
 * @param cfg Configuración del servidor
 * @param clientfd descriptor de la conexión con el cliente
 * @return int OK si la petición se ha procesado correctamente o ERR en caso contrario
 */
int process_http_request(ServerCfg *cfg, int clientfd) {
    char buf[4096];
    char aux[50];
    HttpRequest req;
    int num_requests_per_connection = cfg->max_requests_per_keep_alive_connection;
    int keep_alive_timeout = cfg->keep_alive_timeout;
    bool active = true;
    memset(&req, 0, sizeof(HttpRequest));

    while (num_requests_per_connection > 0) {
        int pret;
        size_t buflen = 0, prevbuflen = 0;
        ssize_t rret;

        while (1) {
            rret = recv(clientfd, buf + buflen, sizeof(buf) - buflen, 0);
            if (rret <= 0) return ERR;

            prevbuflen = buflen;
            buflen += rret;

            req.num_headers = sizeof(req.headers) / sizeof(req.headers[0]);

            pret = phr_parse_request(buf, buflen, &req._method, &req.method_len,
                                     &req.path, &req.path_len, &req.minor_version,
                                     req.headers, &req.num_headers, prevbuflen);

            if (pret > 0)  // Ha leido bien la respuesta
                break;

            if (buflen == sizeof(buf)) return ERR;
        }

        // seguimos procesando cosas de la peticion que nos interesen
        // metodo, ruta, query, extensión y permisos
        req.method = get_method_id(req._method, req.method_len);

        req._path = req.path;

        decode_path(req.path, req.path_len, &req.path_len);

        process_path(req.path, req.path_len, &req.real_path_len, &req.query,
                     &req.query_len, &req.extension, &req.extension_len);

        if (req.real_path_len == 1 && *req.path == '/') {
            check_file_exists(&req.file_exists, ".", &req.file_info);
        } else {
            check_file_exists(&req.file_exists, req.path + 1, &req.file_info);
        }

        // parseamos el tipo de conexión
        req.connection_type = request_get_connection_type(&req);

        // Si es POST, hay que leer los datos
        if (req.method == METHOD_POST) {
            req.post_data = buf + pret;
        }

        // Creamos un objeto para guardar datos de la respuestas
        HttpResponse res;
        memset(&res, 0, sizeof(res));

        // si Connection=Close o se ha alcanzado el número máximo de peticiones
        // por conexion devolver close y cerrar la conexión después de enviar la
        // respuesta
        if (req.connection_type == UNKNOWN || req.connection_type == CONNECTION_CLOSE ||
            num_requests_per_connection == 1) {
            response_add_header(&res, "Connection", "close", 0);
            active = false;
        } else if (req.connection_type == CONNECTION_KEEPALIVE) {
            sprintf(aux, "timeout=%d, max=%d", keep_alive_timeout,
                    num_requests_per_connection);
            response_add_header(&res, "Connection", "keep-alive", 0);
            response_add_header(&res, "Keep-Alive", aux, 0);
            struct timeval tv;
            tv.tv_sec = cfg->keep_alive_timeout;
            tv.tv_usec = 0;
            setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
        }

        // enviar la respuesta correspondiente
        int ret = send_response(cfg, &req, &res, clientfd);
        response_free_headers(&res);

        syslog(LOG_INFO, "%.*s %s %d %s", (int)req.method_len, req._method, req._path,
               res.status, get_status_as_text(res.status));

        num_requests_per_connection--;

        if (!active) return ret;
    }
    return OK;
}

/**
 * @brief Genera la respuesta HTTP tras haber parseado la petición
 *
 * @param cfg Configuración del servidor
 * @param req Petición http
 * @param res Estructura con los datos de la respuesta http (inicialmente vacia)
 * @param clientfd conexión con el cliente
 * @return int OK si se ha envido una respuesta correctamente o ERR en caso contrario
 */
int send_response(ServerCfg *cfg, HttpRequest *req, HttpResponse *res, int clientfd) {
    int ret = -1;

    // La versión que devolvemos siempre es HTTP/1.1
    res->minor_version = 1;

    // Si el método es desconocido, devolver 501
    if (req->method == METHOD_UNKNOWN) {
        response_add_header(res, "Allow", "GET,POST,OPTIONS", false);
        ret = send_error(cfg, req, res, clientfd, 501);
    } else {
        if (!req->file_exists) {
            // si el fichero no existe, devolver 404
            ret = send_error(cfg, req, res, clientfd, 404);
        } else {
            if (req->method == METHOD_GET || req->method == METHOD_POST) {
                // si es un directorio
                if (S_ISDIR(req->file_info.st_mode)) {
                    if (req->path[req->real_path_len - 1] != '/') {
                        // Si es un directorio pero no acaba en "/""
                        // redireccionamos a la misma ruta acabada en /
                        char location[1024] = "http://";

                        res->status = 301;

                        // Construimos la ruta a la que queremos redireccionar
                        struct phr_header *host =
                            HttpRequestFindHeaderByName(req, "Host");
                        strncat(location, host->value, host->value_len);
                        strncat(location, req->path, req->real_path_len);
                        strcat(location, "/");
                        if (req->query_len) {
                            strcat(location, "?");
                            strncat(location, req->query, req->query_len);
                        }

                        response_add_header(res, "Location", location, false);
                        response_send_header(cfg, res, clientfd);
                        return OK;
                    } else {
                        // si es directorio y la ruta acaba en /
                        return send_dir(cfg, req, res, clientfd);
                    }
                } else if (es_extension_script(req->extension, req->extension_len)) {
                    run_script(cfg, req, res, clientfd);

                    ret = ERR;
                } else {
                    // llamar a la función que sirve un fichero
                    ret = send_file(cfg, req, res, clientfd);
                }
            } else if (req->method == METHOD_OPTIONS) {
                if (es_extension_script(req->extension, req->extension_len)) {
                    response_add_header(res, "Allow", "GET, POST, OPTIONS", false);
                } else {
                    response_add_header(res, "Allow", "GET, OPTIONS", false);
                }
                res->status = 200;
                response_add_header(res, "Content-Length", "0", false);
                response_send_header(cfg, res, clientfd);
                ret = OK;
            }
        }
    }

    return ret;
}

/**
 * @brief Sirve la petición http cuando la ruta se corresponde a
 * un directorio. Intenta servir primero index.html, py o php y
 * en caso de no encontrar ninguno de estos ficheros lista todos
 * los ficheros de ese directorio
 *
 * @param cfg Configuración del servidor
 * @param req Petición http
 * @param res respuesta http
 * @param clientfd conexión con el cliente
 * @return int OK si se ha enviado la respuesta correctamente o ERR en caso contrario
 */
int send_dir(ServerCfg *cfg, HttpRequest *req, HttpResponse *res, int clientfd) {
    char *idx_path = calloc(req->path_len + 30, sizeof(char));
    strncpy(idx_path, req->path, req->real_path_len);
    strcat(idx_path, "index.html");
    int flag;
    check_file_exists(&flag, idx_path + 1, &req->file_info);

    if (flag) {  // hemos encontrado un fichero index
        req->path = idx_path;
        req->extension = "html";
        req->extension_len = 4;
        int ret = send_file(cfg, req, res, clientfd);
        free(idx_path);
        return ret;
    }

    free(idx_path);
    return list_dir(cfg, req, res, clientfd);
}

/**
 * @brief Sirve un fichero al recibir una petición get. Incluye control de cache.
 *
 * @param cfg Configuración del servidor
 * @param req Petición http
 * @param res Respuesta http
 * @param clientfd conexión con el cliente
 * @return int OK si el fichero se ha enviado correctamente, ERR en caso de error.
 */
int send_file(ServerCfg *cfg, HttpRequest *req, HttpResponse *res, int clientfd) {
    FILE *fp = NULL;

    fp = fopen(req->path + 1, "rb");
    // Aunque el fichero exista, es posible que el servidor no tenga permisos
    // suficientes o que haya sido cambiado por otro proceso
    // https://en.wikipedia.org/wiki/Time_of_check_to_time_of_use
    if (fp == NULL) {
        if (errno == EACCES) {
            return send_error(cfg, req, res, clientfd, 403);
        } else {
            return send_error(cfg, req, res, clientfd, 503);
        }
    } else {
        char buff[FILE_CHUNK_SIZE];
        bool file_modified = true;
        size_t bytes_read;
        size_t file_size;
        res->status = 200;

        struct phr_header *if_modified_header =
            HttpRequestFindHeaderByName(req, "If-Modified-Since");

        if (if_modified_header) {
            struct tm tm = {0};
            struct tm file_tm = {0};
            time_t epoch;
            char fecha[100] = {};

            strncpy(fecha, if_modified_header->value, if_modified_header->value_len);
            if (strptime(fecha, "%a, %d %b %Y %H:%M:%S %Z", &tm) != NULL) {
                epoch = mktime(&tm);
                gmtime_r(&req->file_info.st_mtime, &file_tm);
                time_t file_epoch = mktime(&file_tm);

                // si el fichero no ha sido modificado, enviamos 304
                if (difftime(epoch, file_epoch) >= 0) {
                    file_modified = false;
                    res->status = 304;
                }
            }
        }

        // medimos el tamaño del fichero
        fseek(fp, 0, SEEK_END);
        file_size = ftell(fp);

        sprintf(buff, "%zu", file_size);
        if (file_modified) response_add_header(res, "Content-Length", buff, 0);
        response_add_header(res, "Content-Type",
                            get_mime_from_extension(req->extension, req->extension_len),
                            false);

        response_add_header(res, "Last-Modified",
                            get_date_from_timestamp(req->file_info.st_mtime), true);

        response_send_header(cfg, res, clientfd);

        // volvemos al principio
        fseek(fp, 0, SEEK_SET);

        // solo si es GET. Si es HEAD no hay que enviar nada
        if (req->method == METHOD_GET && file_modified) {
            // leer del fichero y enviamos al cliente
            while ((bytes_read = fread(buff, 1, FILE_CHUNK_SIZE, fp)) > 0) {
                send(clientfd, buff, bytes_read, 0);
            }
        }

        // cerramos el fichero
        fclose(fp);
    }

    return res->status;
}

/**
 * @brief Función usada para enviar una página de error http
 *
 * @param cfg Configuración del servidor
 * @param req Petición http recibida
 * @param res Respuesta http
 * @param clienfd conexión con el cliente
 * @param codigo Codigo de error http
 * @return int OK si se ha enviado correctamente la página de error, o ERR en caso
 * contrario.
 */
int send_error(ServerCfg *cfg, HttpRequest *req, HttpResponse *res, int clienfd,
               int codigo) {
    char buff[1024];
    char buff2[30];
    char *format;

    // Seleccionamos la plantilla a usar
    switch (codigo) {
        case 404:
            format = TEMPLATE_404;
            break;
        case 501:
            format = TEMPLATE_501;
            break;
        case 500:
            format = TEMPLATE_500;
            break;
        case 403:
            format = TEMPLATE_403;
            break;
        default:
            format = NULL;
            break;
    }

    // rellenamos la plantilla con los datos necesarios
    size_t size =
        sprintf(buff, format, req->path, cfg->server_signature, cfg->listen_port);
    res->status = codigo;
    sprintf(buff2, "%zu", size);
    response_add_header(res, "Content-Length", buff2, 0);

    // Enviamos cabeceras.
    response_send_header(cfg, res, clienfd);

    // Enviamos el contendido de la página de error.
    return send(clienfd, buff, size, 0);
}

/**
 * @brief Envia la cabecera de la petición HTTP.
 * Esta función se debe llamar solo una vez por petición HTTP y solo
 * cuando se sabe exactamente cual es el contenido del mensaje http.
 *
 * @param cfg Configuración del servidor
 * @param res Estructura de la respuesta http que contiene las cabeceras a enviar
 * @param clientfd conexión tcp con el cliente
 */
void response_send_header(ServerCfg *cfg, HttpResponse *res, int clientfd) {
    char buff[100];
    sprintf(buff, "HTTP/1.%d %d %s\r\n", res->minor_version, res->status,
            get_status_as_text(res->status));

    // enviamos la primera linea.
    send(clientfd, buff, strlen(buff), 0);

    // añadimos las cabeceras Server y Date
    response_add_header(res, "Server", cfg->server_signature, 0);
    response_add_header(res, "Date", get_date_from_timestamp(time(NULL)), 1);

    // Enviamos todas las cabeceras de la estructura res
    for (int i = 0; i < res->num_headers; i++) {
        send(clientfd, res->headers[i].name, res->headers[i].name_len, 0);
        send(clientfd, ": ", 2, 0);
        send(clientfd, res->headers[i].value, res->headers[i].value_len, 0);
        send(clientfd, "\r\n", 2, 0);
    }

    // enviamos el CRLF final
    send(clientfd, "\r\n", 2, 0);
}

/**
 * @brief Ejecuta scripts externos cuando el recurso solicitado tiene la extensión
 * py, php, pl, etc
 *
 * @param cfg Configuración del servidor
 * @param req Petición http
 * @param res Respueta http
 * @param clientfd Conexión tcp con el cliente
 * @return int OK si se ha ejecutado y enviado correctamente la respuesta del script o ERR
 * en caso contrario
 */
int run_script(ServerCfg *cfg, HttpRequest *req, HttpResponse *res, int clientfd) {
    int fdi[2], fdo[2];
    pid_t pid;

    // Creamos las tuberias necesarias
    if (pipe(fdi) == -1) {
        return send_error(cfg, req, res, clientfd, 500);
    }

    if (pipe(fdo) == -1) {
        close(fdi[0]);
        close(fdi[1]);
        return send_error(cfg, req, res, clientfd, 500);
    }

    // hacemos el fork
    pid = fork();

    if (pid == 0) {
        // cerramos tuberias sin usar
        close(fdi[1]);
        close(fdo[0]);

        // reemplazamos stdin y stdout por los extremos correspondientes de las tuberias
        // creadas
        dup2(fdi[0], STDIN_FILENO);
        dup2(fdo[1], STDOUT_FILENO);

        char *interprete = NULL;
        // seleccionamos el interprete correspontiente
        if (strncmp(req->extension, "py", req->extension_len) == 0) {
            interprete = "python3";
        } else if (strncmp(req->extension, "php", req->extension_len) == 0) {
            interprete = "php";
        }

        // si es get, mandamos la query como argumentos del programa
        char *script_args = NULL;
        if (req->method == METHOD_GET && req->query_len > 0) {
            script_args = req->query;
        }

        if (execlp(interprete, interprete, req->path + 1, script_args, (char *)NULL) ==
            -1) {
            perror("execlp");
            exit(0);
        }

    } else if (pid > 0) {
        int real_tam_buffer = 256;
        int bytes_usados = 0;
        int bytes_leidos;
        int status;
        char *read_buff, buff_len[15];
        char *new_buff;
        read_buff = (char *)malloc(256 * sizeof(char));
        close(fdi[0]);
        close(fdo[1]);

        // enviamos los parametros recibidos
        if (req->method == METHOD_POST) {
            struct phr_header *_content_length =
                HttpRequestFindHeaderByName(req, "Content-Length");
            int length = atoin(_content_length->value, _content_length->value_len);
            write(fdi[1], req->post_data, length);

            // enviamos el final del mensaje
            write(fdi[1], "\n", 1);
        }

        // leemos la respuesta del script en bloques y redimensionamos el buffer cuando
        // hace falta
        while (1) {
            bytes_leidos =
                read(fdo[0], read_buff + bytes_usados, real_tam_buffer - bytes_usados);

            if (bytes_leidos <= 0) {
                break;
            }

            bytes_usados += bytes_leidos;
            if (bytes_usados == real_tam_buffer) {
                real_tam_buffer *= 2;
                new_buff = realloc(read_buff, real_tam_buffer * sizeof(char));
                if (new_buff == NULL) {
                    free(read_buff);
                    return send_error(cfg, req, res, clientfd, 500);
                }

                read_buff = new_buff;
                new_buff = NULL;
            }
        }

        // Añadimos las cabeceras Content-Length y Content-Type y el estado
        res->status = 200;
        sprintf(buff_len, "%d", bytes_usados);
        response_add_header(res, "Content-Length", buff_len, 0);
        response_add_header(res, "Content-Type", "text/html", false);

        // Enviamos las cabeceras y el fichero
        response_send_header(cfg, res, clientfd);
        send(clientfd, read_buff, bytes_usados, 0);
        free(read_buff);
        
        close(fdi[1]);
        close(fdo[0]);

        // No dejamos zombie al proceso hijo
        waitpid(pid, &status, 0);
    } else {
        return send_error(cfg, req, res, clientfd, 500);
    }
    return OK;
}
