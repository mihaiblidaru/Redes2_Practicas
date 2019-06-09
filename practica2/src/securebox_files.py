# -*- coding: utf-8 -*-
"""Modulo de gestión de ficheros del cliente
   securebox
"""


import sys
import os
from termcolor import colored, cprint
import getpass
import json
from securebox_utils import query_yes_no
import securebox_crypto as cripto
from securebox_utils import api_call, format_error
from Crypto.PublicKey import RSA
from Crypto.Hash import SHA1
from Crypto.Cipher import AES, PKCS1_OAEP
from Crypto.Util.Padding import pad, unpad
from Crypto.Random import get_random_bytes
from Crypto.Signature import pkcs1_15
from Crypto import Random
from securebox_utils import api_call, Payload, find_unused_filename, ask_for_filename_if_needed
from securebox_users import User
import securebox_crypto as cripto
import requests
import base64
import re

LIST_FILES = 'files/list'
UPLOAD_FILE = 'files/upload'
DELETE_FILE = '/files/delete'
DOWNLOAD_FILE = '/files/download'
GET_PUBLIC_KEY = 'users/getPublicKey'


def get_public_key(cfg, userId):
    """Obtiene la clave pública del un usuario

    ARGS:
        cfg: diccionario con la configuración del cliente
        userId: id del usuario del que se quiere obtener su clave pública

    RETURN:
        La clave pública del usuario con id userId. Si ocurre algun error,
        el programa termina.
    """
    res, status = api_call(cfg.api_base_url, cfg.api_key,
                           GET_PUBLIC_KEY, args={'userID': userId})

    if(status != 200):
        print('Error obteniendo clave publica:\n\t%s\n' %
              format_error(res), end='')
        sys.exit()

    try:
        public_key = RSA.importKey(res['publicKey'])
        return public_key
    except:
        print('Invalid public key')
        sys.exit()


def subir_fichero(cfg, file, dest_id) -> None:
    """Sube un fichero firmado y cifrado al servidor Securebox

    ARGS:
        cfg: diccionario con la configuración del cliente
        file: nombre del archivo a enviar
        dest_id: id del usuario al que se quiere enviar un fichero

    """
    print("Solicitado envio de fichero a SecureBox\n")

    # Comprobamos que existe una identidad registrada
    if not User.check_if_user_exists():
        print("Error: No se ha encontrado una identidad. Registra un nuevo usuario usando --create_id")
        print("     ./securebox_client.py --create_id alice alice@example.com\n")
        sys.exit(0)

    # Abrir el fichero a subir
    try:
        fp = open(file, 'rb')
    except Exception as e:
        print("   %s\n" % str(e))
        sys.exit(1)

    # firmamos y ciframos
    enc_data = enc_sign_aux(cfg, fp, dest_id)

    # Submimos el fichero
    print("-> Subiendo fichero a servidor...", flush="True", end='')
    in_memory_file = Payload(fp.name, initial_bytes=enc_data)
    in_memory_file.seek(0)

    res, status = api_call(cfg.api_base_url, cfg.api_key, UPLOAD_FILE, files={
        'ufile': in_memory_file})

    # Comprobamos que se ha enviado correctamente
    if(status != 200):
        print('Error subiendo fichero:\n\t%s' %
              format_error(res), end='')
        sys.exit()
    print(colored('OK', color='green'))
    print('Fichero "{}" subido correctamente ({} bytes). ID fichero: {}\n'.format(
        file, res['file_size'], res['file_id']))


def download(cfg, file_id, source_id):
    """Desgarga un fichero del servidor securebox

    ARGS:
        cfg: diccionario con la configuración del cliente
        file_id: ID del fichero que se quiere descargar
        source_id:  ID del usuario que ha enviado el fichero
    """
    if not User.check_if_user_exists():
        print("Error: No se ha encontrado una identidad. Registra un nuevo usuario usando --create_id")
        print("     ./securebox_client.py --create_id alice alice@example.com\n")
        sys.exit(0)

    # descargarmos el fichero
    print("Descargando fichero de SecureBox...", flush="True", end='')
    url = '{}/{}'.format(cfg.api_base_url, DOWNLOAD_FILE)
    headers = {'Authorization': 'Bearer ' + cfg.api_key}

    r = requests.post(url, json={'file_id': file_id}, headers=headers)

    if r.status_code != 200:
        print('\nError descargando fichero:\n\t%s' %
              format_error(json.loads(r.text)), end='')
        sys.exit()

    print(colored('OK', color='green'))
    print("-> %d bytes descargados correctamente" % len(r.content))

     
    iv = r.content[:16]
    enc_key = r.content[16:272]
    enc_data = r.content[272:]

    user = User.load_user_from_file()
    
    print("-> Descifrando fichero...", flush="True", end='')
    session_key = cripto.rsa_decrypt(user.private_key, enc_key)
    dec_data = cripto.decrypt_data(enc_data, iv, session_key)
    print(colored('OK', color='green'))

    # Cargar la clave pública del origen
    print("-> Recuperando clave pública de ID %s..." %
          source_id, flush="True", end='')
    public_key = get_public_key(cfg, source_id)
    print(colored('OK', color='green'))

    print("-> Verificando firma...", flush="True", end='')
    if cripto.verify_signature(public_key, dec_data[:256], dec_data[256:]):
        cprint('OK', color='green')
    else:
        cprint('ERROR', color='red')
        sys.exit()

    # Obtenemos el nombre del archivo
    d = r.headers['content-disposition']
    filename = re.findall("filename=(.+)", d)[0][1:-1]

    # Comprobamos que el nombre esté libre
    filename = ask_for_filename_if_needed(filename)

    fp = open(filename, "wb")
    fp.write(dec_data[256:])
    fp.close()

    print("Fichero '%s' descargado y verificado correctamente\n " % filename)


def borrar_fichero(cfg, file_id):
    """ Solicita el borrado de un fichero identificado
    por in id dado.

    ARGS:
        cfg: configuración del cliente
        file_id: id del fichero que se quiere borrar

    """
    print('Solicitando borrado del fichero #%s...' %
          file_id, flush=True, end='')
    res, status = api_call(cfg.api_base_url, cfg.api_key,
                           DELETE_FILE, args={'file_id': file_id})

    if status != 200:
        print('\nError eliminando fichero:\n\t%s' % format_error(res), end='')
        sys.exit()
    else:
        print(colored('OK', color='green'))
        print('Fichero con ID#%s borrado correctamente' % res['file_id'])


def borrar_todos(cfg) -> None:
    """Borra todos los archivos subidos

    ARGS:
        cfg: configuración del cliente

    """
    # Solicitamos la lista de todos los ficheros
    print('Solicitando lista de ficheros...', flush=True, end='')
    res, status = api_call(cfg.api_base_url, cfg.api_key, LIST_FILES)

    if status != 200:
        print('Error solicitando lista de ficheros:\n\t%s' %
              format_error(res), end='')
        sys.exit()
    else:
        print(colored('OK', color='green'))
        if res['num_files'] == 0:
            print('No se ha encontrado ningun fichero')
        else:
            # Borramos uno por uno
            for f in res['files_list']:
                borrar_fichero(cfg, f['fileID'])


def listar_ficheros(cfg):
    """Obtiene e imprime la lista de ficheros
    propios subidos al sevidor de Securebox

    ARGS:
        cfg: configuración del cliente

    """
    print('Solicitando lista de ficheros...', flush=True, end='')

    res, status = api_call(cfg.api_base_url, cfg.api_key, LIST_FILES)

    if status != 200:
        print('Error solicitando lista de ficheros:\n\t%s' %
              format_error(res), end='')
        sys.exit()
    else:
        print(colored('OK', color='green'))
        if res['num_files'] == 0:
            print('No se ha encontrado ningun fichero')
        else:
            for idx, f in enumerate(res['files_list']):
                print('[{}] Id: {} -> {}'.format(idx +
                                                 1, f['fileID'], f['fileName']))


def sign_fichero(cfg, filename):
    """Firma un fichero en local

    ARGS:
        cfg: namespace con la configuración del servidor
        filename: nombre del fichero a firmars    
    """

    print('Firmando fichero offline')

    if not User.check_if_user_exists():
        print("Error: No se ha encontrado una identidad. Registra un nuevo usuario usando --create_id")
        print("     ./securebox_client.py --create_id alice alice@example.com\n")
        sys.exit(0)

    # abrir el fichero a subir
    try:
        fp = open(filename, 'rb')
    except Exception as e:
        print("   %s\n" % str(e))
        sys.exit(1)

    # cargar nuestro usuario clave privada
    user = User.load_user_from_file()

    # generamos el nombre del fichero donde vamos a guardar el fichero firmado
    outfilename = ask_for_filename_if_needed(filename + ".signed")

    try:
        ofp = open(outfilename, "wb")
    except Exception as e:
        print(str(e))
        sys.exit()

    data = fp.read()
    fp.close()

    # Generamos la firma
    print("-> Firmando fichero...", flush="True", end='')
    signature = cripto.sign_data(user.private_key, data)
    print(colored('OK', color='green'))

    # Guardamos el archivo firmado
    ofp.write(signature)
    ofp.write(data)
    ofp.close()
    print('Fichero firmado escrito correctamente en %s\n' % outfilename)


def encrypt(cfg, filename, dest_id) -> None:
    """Cifra un fichero en local

    ARGS:
        cfg: namespace con la configuración del servidor
        filename: nombre del fichero a cifrar
        dest_id: id del usuario al que se quiere enviar el fichero

    """

    print('Cifrando fichero offline')

    print("-> Recuperando clave pública de ID %s..." %
          dest_id, flush="True", end='')
    public_key = get_public_key(cfg, dest_id)
    cprint('OK', color='green')

    try:
        fp = open(filename, "rb")
    except Exception as e:
        print(str(e))
        sys.exit()

    outfilename = ask_for_filename_if_needed(filename + ".enc")

    try:
        ofp = open(outfilename, "wb")
    except Exception as e:
        print(str(e))
        sys.exit()

    data = fp.read()
    fp.close()

    print("-> Cifrando fichero...", flush="True", end='')
    iv, session_key, enc_data = cripto.encrypt_data(data, cfg.aes_key_size)
    print(colored('OK', color='green'))

    print("-> Cifrando clave de sesión...", flush="True", end='')
    enc_session_key = cripto.rsa_encrypt(public_key, session_key)
    print(colored('OK', color='green'))

    ofp.write(iv)
    ofp.write(enc_session_key)
    ofp.write(enc_data)
    ofp.close()
    print('Fichero cifrado escrito correctamente en %s\n' % outfilename)


def enc_sign_aux(cfg, in_file, dest_id):
    if not User.check_if_user_exists():
        print("Error: No se ha encontrado una identidad. Registra un nuevo usuario usando --create_id")
        print("     ./securebox_client.py --create_id alice alice@example.com\n")
        sys.exit(0)

    # cargar nuestro usuario clave privada
    user = User.load_user_from_file()

    print("-> Recuperando clave pública de ID %s..." %
          dest_id, flush="True", end='')
    public_key = get_public_key(cfg, dest_id)
    print(colored('OK', color='green'))

    data = in_file.read()

    print("-> Firmando fichero...", flush="True", end='')
    signature = cripto.sign_data(user.private_key, data)
    print(colored('OK', color='green'))

    print("-> Cifrando fichero...", flush="True", end='')
    iv, session_key, enc_data = cripto.encrypt_data(
        signature + data, cfg.aes_key_size)
    print(colored('OK', color='green'))

    print("-> Cifrando clave de sesión...", flush="True", end='')
    enc_session_key = cripto.rsa_encrypt(public_key, session_key)
    print(colored('OK', color='green'))

    return iv+enc_session_key+enc_data


def enc_sign(cfg, filename, dest_id) -> None:
    """Firma y cifra un fichero en local

    ARGS:
        cfg: namespace con la configuración del servidor
        filename: nombre del fichero a firmar y cifrar
        dest_id: id del usuario al que se quiere enviar el fichero

    """

    print("Firmando y Cifrando fichero offline\n")

    # Abrir el fichero a firmar/cifrar
    try:
        fp = open(filename, 'rb')
    except Exception as e:
        print("   %s\n" % str(e))
        sys.exit(1)

    # Generamos el nombre del fichero
    outfilename = ask_for_filename_if_needed(filename + ".enc.signed")

    try:
        ofp = open(outfilename, "wb")
    except Exception as e:
        print(str(e))
        sys.exit()

    # Firmamos y ciframos
    data = enc_sign_aux(cfg, fp, dest_id)
    fp.close()

    # Escribimos en disco
    print("-> Escribiendo fichero...", flush="True", end='')
    ofp.write(data)
    ofp.close()

    print('Fichero firmando y cifrado escrito correctamente en %s\n' % outfilename)
