# -*- coding: utf-8 -*-
""" Modulo de funciones y clases auxiliares
del cliente securebox implementado para la practica 2 de
REDES2, curso 2018/2019, Grupo 2311, Pareja 12.

AUTORES:
    Alberto Ayala
    Mihai Blidaru
    
Este fichero contiene todas aquellas funciones auxiliares
usadas en el desarrollo del cliente securebox que no pueden ser
incluidas en ninguno de los demás modulos.

"""
import os.path
import sys
import io
import argparse
import json
import requests
from termcolor import colored


class Payload(io.BytesIO):
    """ Fichero en memoría como BytesIO pero con
    nombre para que la libreria requests lo detecte
    correctamente
    """

    def __init__(self, name, initial_bytes=b''):
        self.name = name
        super().__init__(initial_bytes)


class CustomParser(argparse.ArgumentParser):
    """Argparser con ayuda personalizada y algunos tweaks

    Esta clase se ha creado con el fin de poder personalizar 
    la ayuda que muestra por defecto el módulo argparse. En este
    caso sustituimos esa funcionalidad e imprimimos directamente
    una cadena generadada por nosotros.
    
    """

    def __init__(self, help_string, prog=None):
        """Crea un nuevo objeto CustomParser

        ARGUMENTOS:
            help_string: cadena de ayuda que se mostrará en la terminal
            cuando lo solicite el usuario con --help o cuando se introduzcan
            paraetros incorrectos
            prog: nombre del programa 
        
        """

        self.help_string = help_string
        super().__init__(prog=prog)

    def print_help(self):
        """Override de la función con el mismo nombre de la clase ArgumentParser
        
        Imprime directamente la cadena guardada en el atributo de instancia 
        self.help_string
        """
        print(self.help_string)

    def error(self, message):
        """ Override de la función con el mismo nombre de la clase ArgumentParser

        Imprime el error de parseo de argumentos con un formato especificado por nosotros.
        Tras imprimir el error, se realiza una llamada a sys.exit() y el programa termina.

        """
        sys.stderr.write('\n%s: error: %s\n\n' % (self.prog, message))
        self.print_help()
        sys.exit(2)


def find_unused_filename(filename):
    """Dado el nombre de un cichero, si este ya existe genera un
    nombre alternativo. Si no existe, devuelve el mismo nombre.

    ARGUMENTOS:
        filename: nombre del fichero a partir del cual se intenta generar uno nuevo

    RETURN:
        Devuelve el nombre del fichero generado.

    """
    if os.path.exists(filename):
        for num in range(1, 50000):
            new_filename = filename + "." + str(num)
            if not os.path.exists(new_filename):
                return new_filename, True
    else:
        return filename, False


def ask_for_filename_if_needed(filename):
    """Dado el nombre de un fichero, comprueba si está disponible
    y si no lo está pregunta al usuario un nombre de archivo alternativo.

    ARGUMENTOS:
        filename: nombre del fichero a comprobar si se puede usar o no

    RETURN:
        Nombre del fichero sin alterar o el nuevo nombre de fichero introducido por el usuario

    """
    new_filename, modified = find_unused_filename(filename)
    if modified:
        not_usable = True
        aux = filename
        while not_usable:
            print('Error: File "%s" already exists!' % aux)
            aux = input(
                'Enter new filename (Press Enter for default: %s): ' % new_filename)
            if len(aux) < 1:
                return new_filename
            else:
                _, modified = find_unused_filename(aux)
                if not modified:
                    return aux
    else:
        return filename


def format_error(res):
    """Dado un error enviado por la api de securebox, devuelve una cadena
    con el formato: "Codigo_http Codigo error : Descripción del error"

    ARGUMENTOS:
        res: diccionario con la respuesta enviada por la api de securebox

    RETURN:
        Devuelve una cadena que contiene el código http, el código de error y una descripción del erorr
        con el formato: "Codigo_http Codigo error : Descripción del error"
    
    """

    return '{} {}: {}\n'.format(res['http_error_code'], res['error_code'], res['description'])


def api_call(base, api_key, endpoint, args=None, files=None):
    """Realiza llamadas a la api de securebox

    ARGUMENTOS:
        base: la base de la url a la que enviar las peticiones
        api_key: token para usar con la api
        endpoint: endpoint al que enviar la petición. Se concatena con la base
        args: diccionario con los los parametros a enviar en formato JSON. Por defecto None
        files: ficheros a enviar a el servidor. Por defecto None

    RETURN:
        La respuesta parseada en formato JSON y el codigo de estado HTTP

    """
    url = '{}/{}'.format(base, endpoint)

    headers = {'Authorization': 'Bearer ' + api_key}

    r = requests.post(url, json=args, files=files, headers=headers)
    return r.json(), r.status_code


def query_yes_no(question, default="yes"):
    """Función para hacer preguntas de SI o No. Esta función repite 
    la pregunta hasta que recibe una respuesta válida.

    ARGUMENTOS:
        question: Pregunta que se quiere imprimir en la terminal
        default: respuesta por defecto para cuando el usuario pulsa
        la tecla Intro sin haber escrito una respuesta

    RETURN: 
        True si el usuario ha constestado Yes o False en caso de que haya contestado No

    """

    valid = {"yes": True, "y": True, "ye": True,
             "no": False, "n": False}
    if default is None:
        prompt = " [y/n] "
    elif default == "yes":
        prompt = " [Y/n] "
    elif default == "no":
        prompt = " [y/N] "
    else:
        raise ValueError("invalid default answer: '%s'" % default)

    while True:
        sys.stdout.write(question + prompt)
        choice = input().lower()
        if default is not None and choice == '':
            return valid[default]
        elif choice in valid:
            return valid[choice]
        else:
            sys.stdout.write("Please respond with 'yes' or 'no' "
                             "(or 'y' or 'n').\n")
