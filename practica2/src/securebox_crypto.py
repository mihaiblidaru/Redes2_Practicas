# -*- coding: utf-8 -*-
"""Modulo con todas las funciones de criptografia
usadas en el cliente securebox.

"""
from Crypto.Signature import pkcs1_15
from Crypto.Random import get_random_bytes
from Crypto.Util.Padding import pad, unpad
from Crypto.Cipher import AES, PKCS1_OAEP
from Crypto.Hash import SHA256
from Crypto.PublicKey import RSA


def generate_rsa_key_pair(rsa_key_size):
    """Genera un par de claves publica-privada de 2048 bits
    para usar con RSA

    ARGUMENTOS:
        rsa_key_size: tamaño de la clave RSA en bytes

    RETURN:
        tupla clave publica, clave privada
    """
    key = RSA.generate(rsa_key_size)
    return key.publickey(), key


def rsa_encrypt(public_key, data):
    """Cifra datos con RSA

    ARGUMENTOS:
        public_key: clave pública usada para descrifrar
        data: datos a cifrar. La longitud de los datos tiene que 
        ser menor que la longitud de la clave RSA.

    RETURN:
        Datos cifrados con RSA usando la clave pública proporcionada.

    """

    cipher_rsa = PKCS1_OAEP.new(public_key)
    return cipher_rsa.encrypt(data)


def rsa_decrypt(private_key, data):
    """Descifra datos con RSA

    ARGUMENTOS:
        public_key: clave pública usada para descrifrar
        data: datos a descifrar. La longitud de los datos tiene que 
        ser menor que la longitud de la clave RSA.

    RETURN:
        Datos descifrados.

    """
    cipher_rsa = PKCS1_OAEP.new(private_key)
    return cipher_rsa.decrypt(data)


def encrypt_data(data, aes_key_size):
    """Cifra datos usando AES-CBC usando una clave aleatoria
    del tamaño indicado por aes_key_size
    
    ARGUMENTOS:
        data: bytes a cifrar con aes
        aes_key_size: tamaño de la clave AES en bytes
    
    RETURN:
        iv, clave de sesión, datos cifrados

    """

    key = get_random_bytes(aes_key_size)

    # tamanio bloque AES = 16 bytes
    iv = get_random_bytes(AES.block_size)

    enc = AES.new(key, AES.MODE_CBC, iv)
    # Escribe el bloque cifrado en la salida
    return iv, key, enc.encrypt(pad(data, AES.block_size))


def decrypt_data(enc_data, iv, session_key):
    """Descrifra datos cifrados con AES-CBC

    ARGUMENTOS:
        enc_data: datos cifrados
        iv: vector de inicialización
        session_key: clave de sesión usada para descifrar los datos
    
    RETURN:
        Los datos descrifrados
    
    """

    dec = AES.new(session_key, AES.MODE_CBC, iv)
    # descifrar y quitar relleno
    return unpad(dec.decrypt(enc_data), AES.block_size)


def get_hash_sha265(data):
    """Calcula el hash de los datos usando
    el algoritmo SHA256

    ARGUMENTOS:
        data: datos de los que se quiere calcular el hash

    RETURN:
        el hash SHA256 de los datos obtenidos

    """
    h = SHA256.new()
    h.update(data)
    return h


def sign_data(private_key, data):
    """Firma los datos usando la clave privada proporcionada
    y devuelve la firma.

    ARGUMENTOS:
        private_key: clave privada usada para firmar
        data: datos a firmar

    RETURN:
        Devuelve la firma generada.

    """

    # Calculamos el hash del fichero
    sha256_hash = get_hash_sha265(data)

    signObj = pkcs1_15.new(private_key)

    # Calculamos y devolvemos la firma
    return signObj.sign(sha256_hash)


def verify_signature(public_key, signature, data):
    """Verifica la firma de un mensaje.

    ARGUMENTOS:
        public_key: clave pública del emisor del mensaje
        signature: firma del mensaje
        data: el mensaje que se quiere verificar
        
    RETURN:
        True

    """

    try:
        signObj = pkcs1_15.new(public_key)
        # Calculamos el hash de los datos primero

        sha256_hash = get_hash_sha265(data)
        # Si no salta ninguna excepción significa que la firma es correcta
        signObj.verify(sha256_hash, signature)
        return True
    except (ValueError, TypeError):
        return False
