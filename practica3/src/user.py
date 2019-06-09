# -*- coding: utf-8 -*-
""" Modulo de gestión de datos de usuario. Facilita la gestión de usarios
y el autoregistro en la aplicación.
"""
import json
import os


class User():
    filename = 'user.json'

    def __init__(self, name, ip, port: int, creation_timestamp=None, password=None):
        """ Crea una nueva instancia de la clase User
        ARGS:
            name: nickname del usuario
            ip: ip del usuario
            port: puerto TCP en el que el usuario escucha 
            creation_timestamp: epoch de la creación del usuario
            password: contraseña del usuario
        """
        self.name = name
        self.ip = ip
        self.port = port
        self.password = password
        self.creation_timestamp = creation_timestamp

    def __str__(self):
        """Devuelve una cadena formateada con los datos del usuario
        """
        return "{} {} {} {}".format(self.name, self.ip, self.port, self.creation_timestamp)

    @classmethod
    def load_from_file(cls):
        """Carga los datos del usuario desde un fichero.
        La ruta del fichero se especifica en la variable cls.filename
        """
        try:
            with open(cls.filename, "r") as fp:
                raw = json.load(fp)
                return User(raw['name'], raw['ip'], raw['port'], raw['creation_timestamp'], raw['password'])
        except Exception as e:
            print(str(e))

    @classmethod
    def delete_user_file(cls):
        """ Borra el fichero que contiene los datos del usuario actual
        La ruta del fichero se especifica en la variable cls.filename
        """
        try:
            os.remove(cls.filename)
        except Exception as e:
            print(str(e))

    def save_to_file(self):
        """Guarda los datos del usuario en un fichero en formato
        JSON
        """
        try:
            with open(self.filename, "w") as fp:
                data = {
                    'name': self.name,
                    'ip': self.ip,
                    'port': self.port,
                    'creation_timestamp': self.creation_timestamp,
                    'password': self.password
                }

                json.dump(data, fp, indent=2)
        except Exception as e:
            print(str(e))
