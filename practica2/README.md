# Practica 2 - Cliente SecureBox

## Instalación

Antes de usar el cliente hay que instalar las dependencias especificadas en el
archivo **requirements.txt**. El proceso de instalación es muy sencillo; primero hay que descargar el cliente y posteriormente descargar las dependencias. Los pasos a seguir son los siguientes.

Clonar el repositorio:
```
git clone https://vega.ii.uam.es/2312-12-19/practica2.git
```

Instalar las dependencias:
```
cd practica2
pip3 install -r requirements.txt --user

```



## Funcionalidad del cliente

Las opciones de la línea de comandos que soporta el cliente desarrollado son:

### Gestión de usuarios e identidades
**--create_id nombre email [alias]**:	Crea una nueva identidad (par de claves púlica y privada) para un usuario con nombre nombre y correo email, y la registra en SecureBox, para que pueda ser encontrada por otros usuarios. alias es una cadena identificativa opcional.

**--search_id cadena**:	Busca un usuario cuyo nombre o correo electrónico contenga cadena en el repositorio de identidades de SecureBox, y devuelve su ID.

**--delete_id id**:	Borra la identidad con ID id registrada en el sistema. Obviamente, sólo se pueden borrar aquellas identidades creadas por el usuario que realiza la llamada.

### Subida y descarga de ficheros
**--upload fichero**:	Envia un fichero a otro usuario, cuyo ID es especificado con la opción

**--dest_id.**: Por defecto, el archivo se subirá a SecureBox firmado y cifrado con las claves adecuadas para que pueda ser recuperado y verificado por el destinatario.

**--source_id id**:	ID del emisor del fichero.

**--dest_id id**:	ID del receptor del fichero.

**--list_files**:	Lista todos los ficheros pertenecientes al usuario

**--download id_fichero**:	Recupera un fichero con ID id_fichero del sistema (este ID se genera en la llamada a upload, y debe ser comunicado al receptor). Tras ser descargado, debe ser verificada la firma y, después, descifrado el contenido.

**--delete_file id_fichero**:	Borra un fichero del sistema.

**--delete_files_all**: Borra todos los ficheros del sistema.

### Cifrado y firma de ficheros local
**--encrypt**: fichero	Cifra un fichero, de forma que puede ser descifrado por otro usuario, cuyo ID es especificado con la opción --dest_id.

**--sign fichero**:	Firma un fichero.

**--enc_sign fichero**:	Cifra y firma un fichero, combinando funcionalmente las dos opciones anteriores.
