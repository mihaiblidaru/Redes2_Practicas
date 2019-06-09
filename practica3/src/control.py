# -*- coding: utf-8 -*-
"""Modulo de Control de llamdas
"""
import socket
import threading
import socket
import select
import threading
import time


class Control():
    def __init__(self, gui, port=8080):
        """Inicializa esta clase. Crea los sockets e hilos necesarios para
        enviar y recibir llamadas
        ARGS:
            gui: instancia de la gui de la aplicación
            port: puerto por defecto en el que se esperan conexiones. puede cambiar si el proporcionado
            no está disponible
        """
        self.gui = gui
        self.debug = True
        self.llamada_actual = None

        # Flag para señalizar que los hilos deben cerrarse
        self.stop_threads = False

        # Creamos el socket para la conexión de control
        self.socket_in = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket_in.setblocking(False)

        self.inputs = [self.socket_in]
        self.outputs = []

        while port < 65535:
            try:
                self.socket_in.bind(('0.0.0.0', port))
                self.socket_in.listen(1)
                self.port = port
                break
            except:
                port += 1

        if port == 65536:
            raise Exception(
                "Error reservando puerto para la conexión de control")

        self.port = port

        self.busy = False

        # Crea el hilo que accepta peticiones
        t = threading.Thread(target=self.connection_loop)
        t.daemon = True
        t.start()

    def exit(self):
        """Función que cambia el flag stop_threads indicando que los hilos creados por esta instancia
        deben cerrarse
        """
        self.stop_threads = True

    def connection_loop(self):
        """Hilo que está esperando recibir conexiones. Cuando recibe una conexión lanza otro hilo
        que atiende los mensajes enviados por esa conexión
        """

        while True:
            # comprobamos si se puede hacer un accept usando un timeout de 0.5 segundos
            rl, wl, _ = select.select(
                self.inputs, self.outputs, self.inputs, 0.5)
            for s in rl:
                conn, addr = s.accept()
                t = threading.Thread(
                    target=self.client_thread, args=(conn, addr))
                t.daemon = True
                t.start()

            # si el timeout ha terminado y se ha recibido la señal para parar hilos
            if len(rl) == 0 and self.stop_threads:
                self.socket_in.close()
                break  # terminamos thread

    def client_thread(self, conn, addr):
        """ Hilo que está esperando a recibir mensajes de una conexión tcp.
        ARGS:
            conn: socket TCP en el que se escucha
            addr: dirección ip del otro extremo de la conexión TCP

        """
        conn.setblocking(False)
        _in = [conn]

        while True:
            _in = list(filter(lambda x: x._closed == False, _in))
            if not _in:
                return

            rl, _, _ = select.select(_in, [], _in, 0.2)
            for c in rl:
                try:
                    data = conn.recv(1024)
                except ConnectionResetError:
                    return

                if not data:
                    conn.close()
                    if c is self.llamada_actual:
                        self.gui.callback_connection_closed_unexpectedly()
                    self.busy = False
                    return

                # Pasamos a string
                request = data.decode('utf-8')

                if self.debug:
                    print("OUT -> IN: %s" % request)

                # separamos en palabras
                w = request.split()

                # Parseamos en funcioón del mensaje recibido e invocamos
                # los callbacks de la gui correspondientes
                if request.startswith("CALLING"):
                    if self.busy:
                        conn.send(b"CALL_BUSY")
                        conn.close()
                        self.gui.callback_call_busy(w[1])
                        break
                    else:
                        self.busy = True
                        self.llamada_actual = conn
                        self.src_nick = w[1]
                        self.src_port = int(w[2])
                        self.gui.callback_calling(self.src_port, addr[0])
                elif request.startswith("CALL_ACCEPTED"):
                    if self.llamada_actual == None:
                        self.llamada_actual = conn
                        self.gui.callback_accepted_or_denied(
                            1, int(w[2]), addr[0])
                    else:
                        conn.close()
                        return
                elif request.startswith("CALL_END"):
                    if c is self.llamada_actual:
                        self.llamada_actual = None
                        c.close()
                        self.busy = False
                        self.gui.callback_call_end()
                        return
                elif request.startswith("CALL_DENIED"):
                    self.busy = False
                    conn.close()
                    self.gui.callback_accepted_or_denied(0)
                elif request.startswith("CALL_HOLD"):
                    if c is self.llamada_actual:
                        self.gui.callback_call_pause()
                    else:
                        c.close()
                        return
                elif request.startswith("CALL_RESUME"):
                    if c is self.llamada_actual:
                        self.gui.callback_call_resume()
                    else:
                        c.close()
                        return
                else:
                    c.send(b"BAD REQUEST")
                    c.close()
                    return

            if not rl and self.stop_threads:
                # parar thread cuando se acctiva el flag stop_threads
                break

    # Funciones para enviar comandos al otro extremo.
    def llamar(self, my_user, dest_user):
        """ Realiza la llamada a otro usuario.
        ARGS:
            my_user: instancia de la clase User que contiene todos los datos del usuario
            que esta usando esta aplicación. Importantes nickname y puerto udp
            dest_user: instancia de la claes User que contiene los datos del usuario al que se le
            quiere llamar.
        """

        try:
            # Creamos socket para cuando llammemos nosotros
            self.socket_out = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            # Le dejamos 10 segundos para conectarse
            self.socket_out.settimeout(10)   
            self.socket_out.connect((dest_user.ip, dest_user.port))
            self.socket_out.settimeout(None)   
        except (ConnectionRefusedError, socket.timeout) as _:
            return False

        request = "CALLING {} {}".format(
            my_user.name, my_user.udpport)

        if self.debug:
            print("IN -> OUT: %s" % request)

        # Enviamos calling
        self.socket_out.send(request.encode('utf-8'))

        # Nos ponemos a escuchar en la conexión creada
        t = threading.Thread(target=self.client_thread,
                             args=(self.socket_out, (dest_user.ip,)))
        t.daemon = True
        t.start()

        return True

    def call_accepted(self, name, port):
        """ Manda el mensaje CALL_ACCEPTED al otro extremo de la
        llamada
        """

        request = "CALL_ACCEPTED {} {}".format(
            name, port)

        if self.debug:
            print("IN -> OUT: %s" % request)

        self.llamada_actual.send(request.encode('utf-8'))

    def call_denied(self):
        """ Manda el mensaje CALL_DENIED al otro extremo de la
        llamada indicando que la llamada no se ha aceptado
        """
        request = "CALL_DENIED"

        if self.debug:
            print("IN -> OUT: %s" % request)

        self.llamada_actual.send(request.encode('utf-8'))
        self.busy = False
        self.llamada_actual.close()
        self.llamada_actual = None

    def call_hold(self):
        """ Envia el mensaje CALL_HOLD al otro exremo de la llamada
        indicando que quiere resumir la llamada
        """
        request = "CALL_HOLD"
        if self.debug:
            print("IN -> OUT: %s" % request)

        self.llamada_actual.send(request.encode('utf-8'))

    def call_resume(self):
        """ Envia el mensaje CALL_RESUME al otro exremo de la llamada
        indicando que quiere resumir la llamada
        """
        request = "CALL_RESUME"
        if self.debug:
            print("IN -> OUT: %s" % request)

        self.llamada_actual.send(request.encode('utf-8'))

    def call_end(self, nick):
        """ Envia el mensaje CALL_END al otro extremo de la llamada
        indicando que esta se ha terminado.
        ARGS:
            nick: nick del usuario
        """
        request = "CALL_END %s" % nick
        if self.debug:
            print("IN -> OUT: %s" % request)
        self.llamada_actual.send(request.encode('utf-8'))
        self.llamada_actual = None
