# -*- coding: utf-8 -*-
"""Módulo usado para la gestion de usuarios
del cliente securebox

"""

from Crypto import Random
from Crypto.Signature import pkcs1_15
from Crypto.Random import get_random_bytes
from Crypto.Util.Padding import pad, unpad
from Crypto.Cipher import AES, PKCS1_OAEP
from Crypto.Hash import SHA1
from Crypto.PublicKey import RSA
from securebox_utils import api_call, format_error
import securebox_crypto as cripto
from securebox_utils import query_yes_no
import json
import getpass
from termcolor import colored, cprint
import os
import sys
sys.path.insert(0, './src')


# Endpoints
REGISTER = 'users/register'
DELETE = 'users/delete'
GET_PUBLIC_KEY = 'users/getPublicKey'
SEARCH = 'users/search'


class User():
    """Clase usuario usada para gestionar la identidad del usuario
    que está usando el cliente securebox
    
    """

    def __init__(self, user_id, name, email, private_key, passphrase=None, alias=None):
        self.user_id = user_id
        self.name = name
        self.email = email
        self.alias = alias
        self.private_key = private_key
        self.__ascii_key = None
        self.passphrase = passphrase

    def serialize(self, file='user_config.json', force=False):
        fp = open(file, "w")

        exported_key = None
        encrypted = False

        if self.passphrase:
            exported_key = self.private_key.export_key(
                passphrase=self.passphrase, pkcs=8, protection='scryptAndAES256-CBC')
            encrypted = True
        else:
            exported_key = self.private_key.export_key()

        data = {
            "user_id": self.user_id,
            "name": self.name,
            "email": self.email,
            "alias": self.alias,
            "private_key": exported_key.decode('utf-8'),
            "encrypted": encrypted
        }

        json.dump(data, fp, indent=4)

        fp.close()

    @staticmethod
    def remove_user_files(file='user_config.json'):
        """Borra el fichero que contiene los datos de usuario
        """
        try:
            os.remove(file)
            return True
        except:
            return False

    @staticmethod
    def check_if_user_exists(file='user_config.json'):
        """Comprueba si existe un fichero con los datos
        de un usuario
        """
        try:
            open(file, 'r').close()
            return True
        except:
            return False

    @staticmethod
    def load_user_from_file(file='user_config.json'):
        """Intenta cargar los datos de usuario desde un fichero
        Si la clave privada del usuario está cifrada, solicitará al usuario
        introducir la contraseña para desbloquear su clave privada.
        """
        with open(file, 'r') as fp:
            data = json.load(fp)
            user_id = data['user_id']
            name = data['name']
            email = data['email']
            alias = data['alias']
            encrypted = data['encrypted']

            private_key = None
            if encrypted:
                password = None
                while not password:
                    password = getpass.getpass(
                        'Introduca su contraseña para desbloquear su clave privada (CTRL+C para salir):')
                    try:
                        private_key = RSA.import_key(
                            data['private_key'], password)
                        print("    Clave privada descrifrada correctamante\n")
                    except:
                        cprint(
                            '    Error: Contraseña incorrecta o fichero de configuración coruptu', 'red')
                        password = None
            else:
                private_key = RSA.import_key(data['private_key'])

            return User(user_id, name, email, private_key, alias=alias)

    @staticmethod
    def creation_wizard(user_id, name, email, private_key, alias=None):
        """Asistente de creación de un usuario
        """

        print('Datos del usuario')
        print('    UserID - %s' % colored(user_id, 'green'))
        print('    Name   - %s' % colored(name, 'green'))
        print('    E-mail - %s' % colored(email, 'green'))
        if alias:
            print('    Alias  - %s' % colored(alias, 'green'))

        print("\n    Por motivos de seguridad, la clave privada necesita ser cifrada")
        print("antes de ser escrita en disco. A continuación se le solicitará una")
        print("contraseña que se usara para el cifrado de la clave privada. Esta ")
        print("contraseña le será solicitada en todas las operaciones que")
        print("necesiten descifrar su clave privada")

        password = getpass.getpass(
            '\nIntroduzca una contraseña (vacio si no se quiere usar una contraseña):')

        if len(password) == 0:
            cprint("Warning: No cifrar la clave privada puede ser inseguro!", 'yellow')

        return User(user_id, name, email, private_key, password, alias)


def crear_identidad(cfg, params):
    """Registra una nueva identidad en el servidor securebox

    ARGS:
        cfg: diccionario con la configuración del cliente
        params: lista con los parametros necesarios para la creación de la 
        identidad: nombre, correo y opcionalmente un alias.
    """
    alias = params[2] if len(params) > 2 else None

    print("Solicitando creación de una nueva identidad en SecureBox.\n")

    if User.check_if_user_exists():
        res = query_yes_no(
            "Ya existe una identidad registrada. Sus datos seran sobrescritos (CTRL+C para salir). Continuar?")
        if not res:
            print("Saliendo...")
            sys.exit(0)

    # Generamos el par de claves RSA
    print('-> Generando par de claves RSA de %d bits...' %
          cfg.rsa_key_size, flush=True, end='')
    public_key, private_key = cripto.generate_rsa_key_pair(cfg.rsa_key_size)
    print(colored('OK', color='green'))

    exported_public_key = public_key.exportKey().decode('utf-8')

    # Registramos la clave pública
    print('-> Registrando clave pública...', flush=True, end='')
    res, status = api_call(cfg.api_base_url, cfg.api_key, REGISTER,
                           args={
                               'nombre': params[0],
                               'email': params[1],
                               'publicKey': exported_public_key
                           })

    creation_ts = res['ts']
    if status != 200:
        print('Error en el registro:\n\t%s' % format_error(res), end='')
        sys.exit()

    print(colored('OK', color='green'))

    # Buscamos el ID del usuario (aka nuestro NIA). Estaría muy bien que lo
    # devolviese la propia llamada al registro, pero como no lo hace, lo
    # tenemos que buscar nosotros.
    # Normalmente la llamada a SEARCH con nuestro correo debería devolver un
    # único resultado pero como en este caso cualquiera puede introducir
    # cualquier correo y la gente es muy troll hay que usar también el
    # timestamp del registro para estar 100% seguros(0 por lo menos
    # minimizar probabilidad de multiples resultados).

    print('-> Buscando ID de usuario asignado...', end='')

    res, status = api_call(cfg.api_base_url, cfg.api_key, SEARCH, args={
        'data_search': params[1]})

    if status != 200:
        print('Error buscando el ID de usuario asignado:\n\t%s' %
              format_error(res), end='')
        sys.exit()

    # eliminar coincidencias parciales y comprobar que el usuario se ha creado en
    # el mismo milisegundo (creo que esto debería ser más que suficiente para
    # asegurar unicidad)
    user_id = list(filter(lambda x: x['nombre'] ==
                          params[0] and x['email'] == params[1] and
                          abs(creation_ts - float(x['ts'])) < 0.001, res))[0]['userID']

    print(colored('OK', 'green'))

    print("-> Guardando datos locales...\n")

    user = User.creation_wizard(
        user_id, params[0], params[1], private_key, alias=alias)
    user.serialize()

    print('Identidad con ID#{} creada correctamente'.format(user_id))


def buscar_usuarios(cfg, data):
    """Busca y lista usuarios registrados

    ARGS:
        cfg: diccionario con la configuración del cliente
        data: cadena con la que se realiza la búsqueda
    """
    print("Buscando usuario '%s' en el servidor..." % data, flush=True, end='')
    res, status = api_call(cfg.api_base_url, cfg.api_key,
                           SEARCH, args={'data_search': data})
    if status != 200:
        print('Error buscando usuarios:\n\t%s' % format_error(res), end='')
        sys.exit()

    cprint('OK', 'green')

    # Imprimimos usuarios (si hay resultados)
    if not res:
        print('No se ha encontrado ningun usuario')
    else:
        print("%d usuarios encontrados:" % len(res))
        for idx, user in enumerate(res):
            print('[{}] {}, {}, ID: {}'.format(
                idx + 1, user['nombre'], user['email'], user['userID']))
        print()


def borrar_usuario(cfg, userid):
    """Borra la identidad actual

    ARGS:
        cfg: diccionario con la configuración del cliente
        user_id: ID del usuario registrado. Tiene que estar vinculado al token
        de la API para poder realizar el borrado correctamente.
    """
    print('Solicitando borrado de la identidad #%s...' %
          userid, flush=True, end='')

    # Realizamos la llamada a la api
    res, status = api_call(cfg.api_base_url, cfg.api_key,
                           DELETE, args={'userID': userid})

    if not User.check_if_user_exists():
        print("\nError: No se ha encontrado una identidad. Registra un nuevo usuario usando --create_id")
        print("     ./securebox_client.py --create_id alice alice@example.com\n")
        sys.exit(0)

    user = User.load_user_from_file()

    # Comprobamos que el id introducido es el nuestro
    if user.user_id != userid:
        print("\nError: Solo puedes borrar tu propia identidad. Tu id=%s, id introducido=%s\n" % (
            user.user_id, userid))
        sys.exit(0)

    if status != 200:
        print('Error eliminando usuario:\n\t%s' % format_error(res), end='')
        sys.exit()
    else:
        print(colored('OK', color='green'))

    # Borramos fichero local
    if User.check_if_user_exists():
        print("Deleting local file...", end='')
        if User.remove_user_files():
            cprint('OK', 'green')
        else:
            cprint('ERROR', 'red')

    print('Identidad con ID#%s borrada correctamente' % userid)
