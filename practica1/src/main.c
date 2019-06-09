
/**
 * @file main.c
 * @author Mihai Blidaru
 * @authos Alberto Ayala
 * @brief Punto de entrada. Lee la configuración, crea el socket y se pone a escuchar
 * peticiones
 * @version 0.1
 * @date 2019-05-06
 *
 * @copyright Copyright (c) 2019
 *
 */

#include <arpa/inet.h>
#include <confuse.h>
#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include "common.h"
#include "config.h"
#include "http.h"
#include "tcp_server.h"

ServerCfg conf;
int sockfd = ERR;

void pipe_handler(__attribute__((unused)) int sig) {}
void int_handler(__attribute__((unused)) int sig) {
    syslog(LOG_INFO, "Stopped by user (Ctrl+C)");
    closelog();
    free_server_config(&conf);
    exit(1);

}

int main(void) {
    char path[2048];
    
    

    // Signal handlers
    signal(SIGPIPE, &pipe_handler);
    signal(SIGINT, &int_handler);

    if (load_server_config("server.conf", &conf) == ERR) return EXIT_FAILURE;

    setlogmask(LOG_UPTO(LOG_INFO));
    openlog(conf.server_signature, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

    syslog(LOG_INFO, "Loading config");
    syslog(LOG_INFO, "Startup by user %d", getuid());
    

    print_server_config(stdout, &conf);

    if (chdir(conf.server_root)) {
        fprintf(stderr, "chdir error. \"%s\" - %s\n", conf.server_root, strerror(errno));
        return EXIT_FAILURE;
    } else {
        fprintf(stderr, "server ready to serve files from %s\n",
                getcwd(path, sizeof(path)));
    }

    setlocale(LC_TIME, "en_US");  // cambiar locale para que al generar fechas
                                  // los meses aparezcan en ingles

    syslog(LOG_INFO, "Bind socket on port %ld", conf.listen_port);

    // Intentamos crear el socket
    sockfd = bind_server(&conf);

    if (sockfd == ERR) {
        free_server_config(&conf);
        return EXIT_FAILURE;
    }

    // Nos ponemos a escuchar conexiones
    run_server(&conf, sockfd, process_http_request);

    return EXIT_SUCCESS;
}
