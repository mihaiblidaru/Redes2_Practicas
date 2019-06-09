#!/usr/bin/python3
# -*- coding: utf-8 -*-
"""Uso:
  securebox_client.py <flag1> [parametros flag] ...
  
General flags:

  -h, --help  muestra este mensaje de ayuda
  
Gestión de identidades:

    --create_id NAME EMAIL [ALIAS]
                            Crea una nueva identidad
    --delete_id ID          Borra una identidad
    --search_id TERM        Busca usuarios

Descarga y subida de ficheros:

    --upload FILE           Sube un fichero al servidor tras firmarlo usando SHA256 
                            y RSA y cifrarlo con AES256-CBC. Hay que especificar
                            el destinadatio usando --dest_id
    --source_id ID          ID del usuario emisor. Usar en combinación 
                            con --download
    --dest_id ID            ID del usuario destinatario. Usar en combinación con
                            --upload, --encrypt o --enc_sign
    --list_files            Lista todos los ficheros subidos
    --download FILE_ID      Descarga el fichero con id FILE_ID. Hay que especificar
                            el emisor usando --source_id
    --delete_file FILE_ID   Borrar el fichero con id FILE_ID
    --delete_files_all      Borrar todos los ficheros subidos

Firma y cifrado offline:

    --encrypt FILE          Cifra un fichero usando AES256-CBC
    --sign FILE             Firma un fichero usando RSA 2048 y SHA256
    --enc_sign FILE         Firma y cifra un archivo
"""
import signal
import sys
sys.path.insert(0, './src')
import src.securebox_utils as utils
import src.securebox_files as files
import src.securebox_crypto as cripto
import src.securebox_users as users
from termcolor import cprint, colored
from types import SimpleNamespace
import base64
import os.path
import re
import shutil
import io
import argparse
import json
import requests


def signal_handler(sig, frame):
    print('\nHas pulsado CTRL+C! Saliendo...\n')
    sys.exit(0)


def load_securebox_config():
    """Carga el archivo de configuracion de securebox
    y lo convierte en un Namespace.
    """
    with open("securebox_config.json", "r") as fp:
        cfg = json.load(fp)

        mandatory = ['api_key', 'api_base_url', 'rsa_key_size', 'aes_key_size']

        # Comprobamos que contiene todos los valores necesarios para funcionar
        for key in mandatory:
            if key not in cfg:
                print("Fichero de configuración invalido. %s no encontrado" % key)
                sys.exit(1)

        # Convertimos el diccionario a un Namespace
        return SimpleNamespace(api_key=cfg['api_key'], api_base_url=cfg['api_base_url'], rsa_key_size=cfg['rsa_key_size'], aes_key_size=cfg['aes_key_size'])


if __name__ == '__main__':
    print("\n+-----------------------------------------------------------------------+")
    print(r'|      ____ ____ ____ _  _ ____ ____ ___  ____ _  _      __             |')
    print(
        r'|      [__  |___ |    |  | |__/ |___ |__] |  |  \/      /o \_____       |')
    print(r'|      ___] |___ |___ |__| |  \ |___ |__] |__| _/\_     \__/-="="`      |')
    print("+-----------------------------------------------------------------------+\n")

    # Configuramos el parser
    parser = utils.CustomParser(__doc__, prog='securebox_client.py')
    g = parser.add_mutually_exclusive_group()

    # Gestión de usuarios
    g.add_argument('--create_id', nargs='*', type=str)
    g.add_argument('--search_id', type=str)
    g.add_argument('--delete_id', type=str)

    # Subida y descarga de ficheros
    g.add_argument('--upload',  type=str)
    g.add_argument('--download', type=str)
    g.add_argument('--delete_file', type=str)
    g.add_argument('--delete_files_all', action='store_true')
    g.add_argument('--list_files', action='store_true')

    # Cifrar y firmar ficheros
    g.add_argument('--encrypt', type=str)
    g.add_argument('--sign', type=str)
    g.add_argument('--enc_sign',  type=str)

    # Parseamos sin haber registrado flags dependientes de otros
    opts, rem_args = parser.parse_known_args()

    # Si es upload o download, añadimos los argumentos obligatorios de cada uno
    if opts.upload or opts.encrypt or opts.enc_sign:
        parser.add_argument('--dest_id', required=True, type=str)

    if opts.download:
        parser.add_argument('--source_id', required=True, type=str)

    # Volvemos a parsear
    args = parser.parse_args()

    # Registramos el manejador para SIGINT
    signal.signal(signal.SIGINT, signal_handler)

    # Intentamos cargar la configuración del programa
    cfg = load_securebox_config()

    # Dispacher en función de los flags recibidos
    if(args.create_id != None):
        # Caso especial para create_id
        if len(args.create_id) < 2:
            print(
                "securebox_client.py: error: argumento --create_id: Se necesitan al menos dos argumentos")
            parser.print_help()
            sys.exit()
        elif len(args.create_id) > 3:
            print(
                "securebox_client.py: error: argumento --create_id: Demasiados argumentos. Máximo tres esperados")
            parser.print_help()
            sys.exit()
        else:
            users.crear_identidad(cfg, args.create_id)
    elif(args.search_id):
        users.buscar_usuarios(cfg, args.search_id)
    elif(args.delete_id):
        users.borrar_usuario(cfg, args.delete_id)
    elif(args.delete_file):
        files.borrar_fichero(cfg, args.delete_file)
    elif(args.upload):
        files.subir_fichero(cfg, args.upload, args.dest_id)
    elif(args.delete_files_all):
        files.borrar_todos(cfg)
    elif(args.list_files):
        files.listar_ficheros(cfg)
    elif(args.sign):
        files.sign_fichero(cfg, args.sign)
    elif(args.encrypt):
        files.encrypt(cfg, args.encrypt, args.dest_id)
    elif(args.download):
        files.download(cfg, args.download, args.source_id)
    elif(args.enc_sign):
        files.enc_sign(cfg, args.enc_sign, args.dest_id)
    else:
        # si no se recibe ningun flag, mostramos la ayuda y salimos del programa
        parser.print_help()
        sys.exit()
