# -*- coding: utf-8 -*-
"""Modulo de comunicación con el servidor de descubrimiento. Solo contiene
la clase Server.

"""
import socket
import time
import sys
import os
from typing import List
import re
from src.user import User


class Server():
    """
        Clase Server que engloba las funciones que
        se comunican con el servidor y la gestion
        de conexiones con el mismo.
    """

    def __init__(self, my_addr, host, port: int, protocols=[0], debug=False):
        """ Inicializa una instancia de la clase que se ocupa de la comunicación
        con el servidor de descubrimiento. Abre una conexión con el servidor y la deja abierta
        para mandar peticiones
        """
        # Creamos el socket
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.my_addr = my_addr
        self.protocols = protocols
        self.debug = debug

        # Conectamos con el servidor
        try:
            self.socket.connect((host, port))
        except Exception as e:
            raise Exception("Error connectando con el servidor: %s" % str(e))

    def __del__(self):
        self.quit()

    def register(self, nickname, port: int, password):
        """ Registra un usuario en la aplicación. Manda un mensaje
        con el formato REGISTER nick ip port password protocols
        y recibe un mensaje del servidor con el formato OK WELCOME timestamo

        ARGS:
            nickname: nick del usuario que se quiere registrar
            port: puerto en el que está la aplicación escuchando
            password: contraseña del usuario

        """
        # Generamos el mensaje
        request = "REGISTER {} {} {} {} {}".format(
            nickname,
            self.my_addr,
            port,
            password,
            "#".join(list(map(lambda x: "V"+str(x), self.protocols))))

        # imprimimos en consola
        if self.debug:
            print("C -> S: " + request)

        # Enviamos el mensaje
        self.socket.send(request.encode('utf-8'))

        # Leemos la respuesta
        response = self.socket.recv(1024).decode('utf-8')

        if self.debug:
            print("S -> C: " + response)

        # Creamos un objeto user si todo ha ido bien
        if response.startswith("OK WELCOME"):
            w = response.split()
            return User(w[2], self.my_addr, port, float(w[3]), password)
        elif response.startswith("NOK WRONG_PASS"):
            return None
        else:
            raise Exception("Bad Response from server")

    def register_u(self, user):
        """ Regista un usuario en la aplicación.

        ARGS: 
            user: objeto usuario a registrar en la aplicación
        """
        return self.register(user.name, user.port, user.password)

    def query(self, nickname) -> User:
        """Solicita la información de un usuario

        ARGS:
            nickname: nick del usuario del que se quiere la información
        """
        request = "QUERY {}".format(nickname)

        if self.debug:
            print("C -> S: " + request)

        self.socket.send(request.encode('utf-8'))
        response = self.socket.recv(1024).decode('utf-8')

        if self.debug:
            print("S -> C: " + response)

        words = response.split()

        if words[0] == 'OK' and words[1] == 'USER_FOUND':
            protocols = words[-1]
            port = int(words[-2])
            ip = words[-3]
            nick = ' '.join(words[2:-3])
            return User(nick, ip, port, 0)
        else:
            return None

    def list_users(self) -> List[User]:
        """Obtiene y devuelve la lista de todos los usuarios.
        """
        self.socket.send(b'LIST_USERS')
        # No sabemos cuantos usuarios hay, por eso leemos un poco
        frame1 = self.socket.recv(30)

        words = frame1.split()

        if words[0] == b"OK" and words[1] == b"USERS_LIST":
            num_users = int(words[2])
            data = b' '.join(words[3:])

            # Contamos los usuarios con una expresión regular porque hay gente muy
            # lista que pone hashtags en su nombre para que un parseo que podria
            # ser muy simple se convierta en un parseo complicado
            num_users_received = len(re.findall(
                "[0-9]{7,11}.[0-9]{1,8}#", data.decode('utf-8')))

            while num_users_received < num_users:
                data += self.socket.recv(2048)
                num_users_received = len(re.findall(
                    "[0-9]{7,11}.[0-9]{1,8}#", data.decode('utf-8')))

            user_lst = []

            separadores = re.findall(
                "[0-9]{7,11}.[0-9]{1,8}#", data.decode('utf-8'))
            last_idx = 0
            data_decoded = data.decode('utf-8')

            for separador in separadores:
                idx_separador = data_decoded.index(separador, last_idx)
                raw_user_data = data_decoded[last_idx:
                                             idx_separador + len(separador) - 1]
                last_idx = idx_separador + len(separador)
                dat = raw_user_data.split()
                user = User(dat[0], dat[1], dat[2], dat[3]
                            if len(dat) == 4 else None)
                user_lst.append(user)
            return user_lst

    def quit(self) -> None:

        try:
            self.socket.send("QUIT".encode('utf-8'))
            response = self.socket.revc(1024)
            self.socket.close()
        except:
            pass
