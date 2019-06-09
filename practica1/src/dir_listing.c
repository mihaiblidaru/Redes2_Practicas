/**
 * @file dir_listing.c
 * @author Mihai Blidaru
 * @brief Lista directorios en html
 * Crea una página HTML con el listado de ficheros
 * de un directorio dado al estilo de Apache.
 *
 * @version 0.1
 * @date 2019-03-12
 *
 * @copyright Copyright (c) 2019
 */

#include <dirent.h>
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
#include "http.h"
#include "http_utils.h"
#include "picohttpparser.h"

/// Cabecera del fichero html. Contiene información de estilo e iconos codificados
/// en base64
#define HEAD                                                                   \
    "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">"                \
    "<html>"                                                                   \
    " <head>"                                                                  \
    " <style> "                                                                \
    " a.icon { text-decoration: none; padding-left: 1.5em;}"                   \
    "a.file {"                                                                 \
    "    background : "                                                        \
    "url(\"data:image/"                                                        \
    "png;base64,"                                                              \
    "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAABnRSTlMAAAAAAABupgeRAAAB" \
    "HUlEQVR42o2RMW7DIBiF3498iHRJD5JKHurL+CRVBp+"                              \
    "i2T16tTynF2gO0KSb5ZrBBl4HHDBuK/WXACH4eO9/"                                \
    "CAAAbdvijzLGNE1TVZXfZuHg6XCAQESAZXbOKaXO57eiKG6ft9PrKQIkCQqFoIiQFBGlFIB5" \
    "nvM8t9aOX2Nd18oDzjnPgCDpn/"                                               \
    "BH4zh2XZdlWVmWiUK4IgCBoFMUz9eP6zRN75cLgEQhcmTQIbl72O0f9865qLAAsURAAgKBJK" \
    "EtgLXWvyjLuFsThCSstb8rBCaAQhDYWgIZ7myM+TUBjDHrHlZcbMYYk34cN0YSLcgS+"      \
    "wL0fe9TXDMbY33fR2AYBvyQ8L0Gk8MwREBrTfKe4TpTzwhArXWi8HI84h/"               \
    "1DfwI5mhxJamFAAAAAElFTkSuQmCC \") left top no-repeat;}"                   \
    "a.dir {"                                                                  \
    "    background : "                                                        \
    "url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/"   \
    "9hAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAAd5JREFUeNqMU79rFU" \
    "EQ/vbuodFEEkzAImBpkUabFP4ldpaJhZXYm/RiZWsv/"                              \
    "hkWFglBUyTIgyAIIfgIRjHv3r39MePM7N3LcbxAFvZ2b2bn22/"                       \
    "mm3XMjF+HL3YW7q28YSIw8mBKoBihhhgCsoORot9d3/ywg3YowMXwNde/"                \
    "PzGnk2vn6PitrT+/PGeNaecg4+qNY3D43vy16A5wDDd4Aqg/ngmrjl/"                  \
    "GoN0U5V1QquHQG3q+TPDVhVwyBffcmQGJmSVfyZk7R3SngI4JKfwDJ2+"                 \
    "05zIg8gbiereTZRHhJ5KCMOwDFLjhoBTn2g0ghagfKeIYJDPFyibJVBtTREwq60SpYvh5++"  \
    "PpwatHsxSm9QRLSQpEVSd7/"                                                  \
    "TYJUb49TX7gztpjjEffnoVw66+"                                               \
    "Ytovs14Yp7HaKmUXeX9rKUoMoLNW3srqI5fWn8JejrVkK0QcrkFLOgS39yoKUQe292WJ1guU" \
    "HG8K2o8K00oO1BTvXoW4yasclUTgZYJY9aFNfAThX5CZRmczAV52oAPoupHhWRIUUAOoyUIl" \
    "YVaAa/VbLbyiZUiyFbjQFNwiZQSGl4IDy9sO5Wrty0QLKhdZPxmgGcDo8ejn+c/"          \
    "6eiK9poz15Kw7Dr/vN/z6W7q++091/AQYA5mZ8GYJ9K0AAAAAASUVORK5CYII= \") left " \
    "top no-repeat;  }</style>"

/// Subcabecera.
#define SUB_HEAD                   \
    "  <title>Index of %s</title>" \
    " </head>"                     \
    " <body>"                      \
    "<h1>Index of %s</h1>"

/// Primera parte de la tabla de ficheros
#define PART3                \
    "<table>"                \
    "<tr> "                  \
    "   <th valign=\"top\">" \
    "  Name</th>"            \
    "</tr>"                  \
    "<tr><th colspan=\"2\"><hr></th></tr>\n"

/// Parte final de la tabla de ficheros
#define PART4                                    \
    "<th colspan=\"2\"><hr></th></tr></table>\n" \
    "<address>%s at 127.0.0.1 Port "             \
    "%ld</address>"                              \
    "</body></html>"

/// Formato de una fila de la tabla de ficheros
#define ROW_FMT "<tr><td><a class=\"icon %s\" href=\"%s\">%s</a></td>\n"

int calculate_file_size(struct dirent **namelist, int n, HttpRequest *req,
                        ServerCfg *cfg);

/**
 * @brief Calcula el tamaño que ocupará la página
 * HTML devuelta
 *
 * @param namelist array con la información de los ficheros de un directorio
 * @param n número de ficheros del directorio a listar
 * @param req petición http
 * @param cfg Estructura con la configuración del programa. Usado para incluir
 * la firma del servidor en la página.
 * @return int tamaño en bytes de lo que ocupará la página HTML
 */
int calculate_file_size(struct dirent **namelist, int n, HttpRequest *req,
                        ServerCfg *cfg) {
    int i;
    int size = 0;

    size += snprintf(NULL, 0, HEAD);
    size += snprintf(NULL, 0, SUB_HEAD, req->path, req->path);
    size += snprintf(NULL, 0, PART3);

    for (i = 0; i < n; i++) {
        int type = namelist[i]->d_type;
        if (type == DT_REG) {
            size += snprintf(NULL, 0, ROW_FMT, "file", namelist[i]->d_name,
                             namelist[i]->d_name);
        } else if (type == DT_DIR) {
            size += snprintf(NULL, 0, ROW_FMT, "dir", namelist[i]->d_name,
                             namelist[i]->d_name);
        }
    }
    size += snprintf(NULL, 0, PART4, cfg->server_signature, cfg->listen_port);
    return size;
}

/**
 * @brief Lista un directorio en html
 *
 * @param cfg Configuración del programa
 * @param req Petición http
 * @param res Estructura con los datos de la respuesta que se va a enviar
 * @param clientfd descriptor de la conexión tcp con el cliente
 * @return int OK si se ha enviado el listado correctamente o ERR en caso contrario
 */
int list_dir(ServerCfg *cfg, HttpRequest *req, HttpResponse *res, int clientfd) {
    struct dirent **namelist;
    int i, n;
    int size = 0;

    char *path = NULL;

    if (req->real_path_len == 1) {
        path = ".";
    } else {
        path = req->path + 1;
    }

    n = scandir(path, &namelist, 0, alphasort);

    char *buf = calloc(calculate_file_size(namelist, n, req, cfg) + 200, sizeof(char));

    size += sprintf(buf + size, HEAD);
    size += sprintf(buf + size, SUB_HEAD, req->path, req->path);
    size += sprintf(buf + size, PART3);

    for (i = 0; i < n; i++) {
        int type = namelist[i]->d_type;
        if (type == DT_REG) {
            size += sprintf(buf + size, ROW_FMT, "file", namelist[i]->d_name,
                            namelist[i]->d_name);
        } else if (type == DT_DIR) {
            size += sprintf(buf + size, ROW_FMT, "dir", namelist[i]->d_name,
                            namelist[i]->d_name);
        }

        free(namelist[i]);
    }
    size += sprintf(buf + size, PART4, cfg->server_signature, cfg->listen_port);

    free(namelist);

    char buff[20];
    sprintf(buff, "%ld", strlen(buf));
    res->status = 200;
    response_add_header(res, "Content-Length", buff, 0);
    response_add_header(res, "Content-Type", get_mime_from_extension("html", 4), false);

    response_send_header(cfg, res, clientfd);

    send(clientfd, buf, size, 0);
    free(buf);
    return OK;
}