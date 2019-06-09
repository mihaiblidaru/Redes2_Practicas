#!/usr/bin/python3
# -*- coding: utf-8 -*-
# import the library
from appJar import gui
from PIL import Image, ImageTk
import numpy as np
import cv2
import time
import sys
import queue
import os
from typing import List
from src.server import Server
from src.user import User
from src.control import Control
from src.udp import UDPControl
import netifaces


SERVER_NAME = 'vega.ii.uam.es'
SERVER_PORT = 8000

class VideoClient(object):

    def __init__(self, window_size, user_file=None):
        # Creamos una variable que contenga el GUI principal
        self.app = gui("Redes2 - P2P", "340x400", handleArgs=False)
        self.my_addr = None
        self.sending_video = False
        self.video_path = None

        # Creamos la interfaz, intentamos obtener la IP
        self.crear_gui()

        try:
            self.server = Server(
                self.my_addr, SERVER_NAME, SERVER_PORT, debug=True)
        except Exception as e:
            self.app.errorBox("Connection Error", str(e))
            self.app.stop()
            sys.exit(-1)

        # Intentamos cargar la configuración del usuario
        _user = User.load_from_file()

        if _user:
            self.control = Control(self, _user.port)
        else:
            self.control = Control(self)

        if _user:
            # Intentamos volver a registrar a usuario con los mismos datos
            # Sobre todo para actualizar la ip y el puerto
            _user.port = self.control.port

            self.user = self.server.register_u(_user)

            if self.user:
                # Si todo va bien, iniciamos sesion automaticamente
                self.user.save_to_file()
                self.cambiar_estado("Default")
                self.app.setStatusbar("User: %s" % self.user.name, field=0)

            else:
                # Si no pedimos que el usuario se registre otra vez
                self.cambiar_estado("Registro")

        else:
            # Si no se carga, mostramos el login
            self.cambiar_estado("Registro")

        # Cada 33 ms ejecutamos capturaVideo = 33 fps
        self.app.setPollTime(33)
        self.app.registerEvent(self.capturaVideo)

        self.sending_video = False
        self.use_webcam = True

    def callback_calling(self, udp_port_dest=None, addr_dest=None):
        """ Función que se ejecuta cuando se recibe una llamada. Pregunta al usuario
        si quiere aceptar la llamada o no
        ARGS:
            udp_port_dest: puerto udp destino del usuario que está realizando la llamada
            addr_dest: dirección ip del usuario que está realizando la llamada        
        """
        response = self.app.questionBox(
            "LLamada Entrante", "Llamada entrante de {}. Aceptar?".format(self.control.src_nick))
        if response == "yes":
            self.udp_control = UDPControl(self)
            self.control.call_accepted(self.user.name, self.udp_control.port)
            self.udp_control.udp_port_dest = udp_port_dest
            self.udp_control.addr_dest = addr_dest
            self.udp_control.stop_threads = False
            self.udp_control.empezar_videollamada()
            self.app.hideFrame("BotonesDefault")
            self.app.showFrame("BotonesLlamada")

            self.cap = cv2.VideoCapture(0)
            self.sending_video = True
            self.app.setPollTime(33)

        else:
            self.control.call_denied()

    def callback_accepted_or_denied(self, respuesta, udp_port_dest=None, addr_dest=None):
        """ Función que se ejecuta cuando un usuario contesta una llamada. La respuesta
        recibida puede ser CALL_ACCEPTED o CALL_DENIED. Si el usuario ha aceptado la llamada
        se empieza a mandar video

        ARGS:
            respuesta: 1 si es CALL_ACCEPTED, cualquier otra cosa si es CALL_DENIED
            udp_port_dest: puerto destino del otro extremo de la llamada
            addr_dest: dirección ip del otro extremo de la llamda
        """
        if respuesta == 1:
            self.app.hideSubWindow("LlamandoA")
            self.app.hideFrame("BotonesDefault")
            self.app.showFrame("BotonesLlamada")
            self.udp_control.udp_port_dest = udp_port_dest
            self.udp_control.addr_dest = addr_dest

            self.sending_video = True
            self.udp_control.stop_threads = False
            self.udp_control.empezar_videollamada()
            self.app.setPollTime(33)
            self.app.infoBox("Llamada aceptada", "A conversar!!")

        else:
            self.app.hideSubWindow("LlamandoA")
            self.app.infoBox("Llamada denegada", "Lo siento, no ha podido ser")

    def callback_connection_closed_unexpectedly(self):
        """ Callback que se invoca cuando la conexión se cierra
        inesperadamente,
        """
        self.app.errorBox(
            "Connection Error", "Conexión cerrada. Puede que el otro extremo haya cerrado la conexión sin señalizar.")
        # Hacemos las mismas acciones que en el caso de que nos cuelguen
        # Para intentar dejar la aplicación en un estado consistente
        self.callback_call_end()

    def callback_call_pause(self):
        """ Callback que se invoca cuando el otro extremo quiere pausar la llamada
        """
        self.sending_video = False
        self.udp_control.queue_in.queue.clear()

    def callback_call_busy(self, nick):
        """Callback que se invoca cuando se recibe una llamada mientras ya hay una
        llamada en curso. Muestra un mensaje con el nombre del usuario que ha realizado
        la segunda llamada
        """
        self.app.infoBox("Llamada Perdida", "Llamada Perdida de %s" % nick)

    def callback_call_resume(self):
        """Callback que se invoca cuando el otro extremo quiere reanudar la llamada
        """
        self.sending_video = True

    def seleccionar_ip(self, btn):
        """Callback que se ejecuta cuando el usuario hace click en alguna ip al inicial la aplicación
        """
        self.my_addr = btn
        self.app.hideSubWindow("SeleccionarIp")

    def crear_gui(self):
        """Crea todos los componentes de la gui
        """

        # Crea la ventana de seleción de direcciones ip
        self.app.startSubWindow(
            'SeleccionarIp', "Seleccionar Dirección IP", True, True)
        interfaces = netifaces.interfaces()

        self.app.addLabel("Selecciona IP", "Selecciona IP")
        self.app.setSticky("ew")

        for iface in interfaces:
            addrs = netifaces.ifaddresses(iface)
            if netifaces.AF_INET in addrs:
                ip = addrs[netifaces.AF_INET][0]['addr']
                self.app.addNamedButton(
                    "%s - %s" % (iface, addrs[netifaces.AF_INET][0]['addr']), ip, self.seleccionar_ip)

        self.app.stopSubWindow()

        self.app.showSubWindow("SeleccionarIp")

        if not self.my_addr:
            self.app.errorBox("Error", "No has seleccionado ninguna IP")
            sys.exit(1)

        # Pantalla de registro
        self.app.setSticky("nw")
        self.app.setGuiPadding(20, 20)
        self.app.setSticky("nw")
        self.app.startFrame("Registro", row=0, column=3)
        self.app.setPadding([5, 20])
        self.app.addImage("logo", "imgs/logo.png", row=0, column=0, colspan=2)
        self.app.setPadding([3, 7])

        self.app.addLabel("_Usuario", "Usuario", 1, 0)
        self.app.setLabelAlign("_Usuario", "left")
        self.app.addEntry("Username", 1, 1)
        self.app.addLabel("_Contrasena", "Contraseña", 2, 0)
        self.app.addSecretEntry("Password", 2, 1)
        self.app.setEntrySubmitFunction("Username", self.registrarse)
        self.app.setEntrySubmitFunction("Password", self.registrarse)

        self.app.startFrame("_anonBtn1", row=3, column=0, colspan=2)
        self.app.setSticky("we")
        self.app.addNamedButton("Login", "_loginBtn", self.registrarse, 1, 0)
        self.app.addNamedButton("Salir", "_salirBtn", self.app.stop, 1, 1)

        self.app.stopFrame()
        self.app.stopFrame()
        # Fin pantalla de registro

        # interfaz general
        self.app.setStretch("none")
        self.app.setSticky("ns")
        self.app.startFrame("LEFT", row=0, column=0)
        self.app.setStretch("COLUMN")
        self.app.addLabel("__Usuarios", "Usuarios Registrados")
        self.app.addListBox("users_list")
        self.app.addButton("Actualizar", self.listar)
        self.app.addButton("Logout", self.logout)

        self.app.addButton("Salir", self.app.stop)
        self.app.stopFrame()

        self.app.setFrameWidth("LEFT", 100)
        self.app.setFrameHeight("LEFT", 200)

        self.app.startFrame("RIGHT", row=0, column=1)
        self.app.addLabel("title", "Cliente Multimedia P2P - Redes2 ")
        self.app.addImage("video", "imgs/webcam.gif")

        self.app.startFrame("BotonesDefault")
        self.app.addButtons(["Conectar", "Conectar con usuario seleccionado"], [
                            self.conectar, self.conectar_con])

        self.app.addLabelOptionBox("Fuente Video", ["Webcam", "Fichero"])
        self.app.addNamedButton(
            "SelectVideo", "Seleccionar Video", self.seleccionar_video)
        self.app.setOptionBoxChangeFunction(
            "Fuente Video", self.cambiar_fuente_video)
        self.app.hideButton("Seleccionar Video")

        # self.app.hideButton("Pausar")
        self.app.stopFrame()
        self.app.startFrame("BotonesLlamada")
        self.app.addButtons(["Pausar", "Reanudar", "Colgar"], [
            self.pausar, self.reanudar, self.colgar])
        self.app.stopFrame()
        self.app.hideFrame("BotonesLlamada")

        self.app.stopFrame()

        # Ventana llamando
        self.app.startSubWindow("LlamandoA")
        self.app.addLabel("popup", "")
        self.app.stopSubWindow()
        self.app.hideSubWindow("LlamandoA")

        # Añadir statusbasr
        self.app.addStatusbar(fields=2)

        self.app.setStopFunction(self.quit)

    def cambiar_estado(self, estado):
        """ Cambia el estado de la intefaz gráfica: Registro o Default
        Solo cambia la dimensión de la ventana y los marcos mostrados.
        """
        if estado == "Registro":
            self.app.showFrame("Registro")
            self.app.hideFrame("LEFT")
            self.app.hideFrame("RIGHT")
            self.app.setGeom("340x420")
            self.estado = estado
        elif estado == "Default":
            self.app.setGeom("800x590")
            self.listar(None)
            self.app.hideFrame("Registro")
            self.app.showFrame("LEFT")
            self.app.showFrame("RIGHT")
            self.estado = estado

    def registrarse(self, btn=None):
        """Función que se invoca cuando un usuario intenta registrarse.
        Realiza las validaciones necesarias de los campos usuario y contraseña y si
        los datos son validos envia la petición al servidor
        """

        nick = self.app.getEntry("Username")
        password = self.app.getEntry("Password")

        if len(nick) == 0:
            return self.app.errorBox("Error Validación", "Nick no valido")
        elif len(password) == 0:
            return self.app.errorBox("Error Validación", "Password no valido")

        user = self.server.register(
            nick, self.control.port, password)
        if user:
            user.save_to_file()
            self.cambiar_estado("Default")
            self.user = user
            self.app.setStatusbar("User: %s" % self.user.name, field=0)
        else:
            self.app.errorBox("Error Registro", "Contraseña incorrecta")

    def cambiar_fuente_video(self, obj):
        """ Función que se invoca cuando hay un cambio en el despelgable Fuente Video
        En función del valor elegido, guarda que fuente de video enviar al realizar una llamada
        """
        valor = self.app.getOptionBox(obj)
        if valor == "Webcam":
            self.app.hideButton("Seleccionar Video")
            self.use_webcam = True
        else:
            self.app.showButton("Seleccionar Video")
            self.use_webcam = False

    def seleccionar_video(self, btn=None):
        """ Muestra el buscador de archivos para poder seleccionar un video mp4
        que será enviado posteriormente al realizar una llamada
        """
        self.video_path = self.app.openBox("Seleccionar Video", None, fileTypes=[
                                           ('video', '*.mp4')], asFile=False)

    def logout(self, btn):
        """ Cierra sesión de la aplicación. Borra el archivo que contiene los datos 
        del usuario y cambia el estado de la interfaz a Registro.
        """
        self.cambiar_estado("Registro")
        self.app.setStatusbar("", 0)
        User.delete_user_file()

    def conectar(self, button=None, nick=None):
        """ Funcioón que realiza una llamda a otro usuario
        ARGS:
            button: boton que ha invocado esta función
            nick: nick predeterminado, en cado de que esta función haya sido invocada por la funcion conectar_con

        """
        # Entrada del nick del usuario a conectar
        if not nick:
            nick = self.app.textBox("Conexión",
                                    "Introduce el nick del usuario a buscar")
            if not nick:
                return

        if nick == self.user.name:
            self.app.errorBox("¡Error llamada!",
                              "No te puedes llamar a ti mismo")
        else:

            user = self.server.query(nick)

            if not user:
                self.app.errorBox("Usuario no encontraro",
                                  "Usuario %s no encontrado" % nick)
                return

            self.udp_control = UDPControl(self)
            self.user.udpport = self.udp_control.listen_port

            if self.use_webcam:
                self.cap = cv2.VideoCapture(0)
            else:
                if not self.video_path or len(self.video_path) < 1:
                    self.app.errorBox("ERROR", "Debes seleccionar un video primero")
                    return
                else:
                    self.cap = cv2.VideoCapture(self.video_path)

            # Llamamos
                       
            result = self.control.llamar(self.user, user)
            if result:
                self.app.setLabel("popup", "Llamando a "+nick+"...")
                self.app.showSubWindow("LlamandoA")
            else:
                self.app.errorBox("Usuario no conectado",
                                  "El usuario %s no está conectado" % user.name)
                self.app.hideSubWindow("LlamandoA")
                self.cap.release()


    def conectar_con(self, btn=None):
        """ Intenta realizar una llamada al usuario seleccionado de la lista
        de usuarios de la aplicación
        """
        user = self.app.getListBox("users_list")
        if not user:
            self.app.errorBox(
                "Error", "Debes selecionar un usuario de la lista")
        else:
            self.conectar(nick=user[0])

    def pausar(self, btn=None):
        """ Manda el mensaje CALL_PAUSE al otro extremo de la llamada
        ARGS:
            btn: boton que ha invocado la llamada a esta función
        """
        self.control.call_hold()
        self.sending_video = False
        self.udp_control.queue_in.queue.clear()

    def reanudar(self, btn=None):
        """ Manda el mensaje CALL_RESUME al otro extremo de la llamada

        ARGS:
            btn: boton que ha invocado la llamada a esta función
        """
        self.control.call_resume()
        self.sending_video = True

    def colgar(self, btn):
        """ Manda el mensaje CALL_END al otro extremo de la llamada,
            detiene el envio de video, cierra los hilos que ya no
            son necesarios y la captura de video.

            ARGS:
                btn: Boton que ha invocado la llamada a esta función
        """
        self.control.call_end(self.user.name)
        self.sending_video = False
        self.cap.release()
        self.udp_control.stop_threads = True
        self.app.setImage("video", "imgs/webcam.gif")
        self.app.showFrame("BotonesDefault")
        self.app.hideFrame("BotonesLlamada")

    def callback_call_end(self):
        """ Función que se ejecuta cuando se recibe un mensaje CALL_END por
        parte del otro extremo de la llamada actual
        """
        self.sending_video = False
        self.cap.release()
        self.udp_control.stop_threads = True
        self.app.setImage("video", "imgs/webcam.gif")
        self.app.showFrame("BotonesDefault")
        self.app.hideFrame("BotonesLlamada")

    def listar(self, button=None):
        """ Lista los usuarios registrados del servido

        ARGS:
            button = nombre del boton que ha provocado la llamada de esta función
        """
        users = self.server.list_users()

        if users:
            users = sorted(users, key=lambda x: x.name.lower())
            self.user_list = users
            self.app.clearListBox("users_list")
            self.app.addListItems("users_list", list(
                map(lambda x: x.name, users)), select=False)

        else:
            pass

    def quit(self, btn=None):
        self.control.exit()
        return True

    def start(self):
        self.app.go()

    def capturaVideo(self):
        """ Funcion que se ejecura 30 veces por segundo encargada de 
        capturar fotogramas de la webcam o de un fichero video, comprimirlas
        a JPG y añadirlas a la cola de envio

        """

        if self.sending_video:
            # Capturamos un frame de la cámara o del vídeo
            ret, frame = self.cap.read()

            self.last_frame_small = cv2.resize(frame, (128, 96))
            cv2_im = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            img_tk = ImageTk.PhotoImage(Image.fromarray(cv2_im))

            # Compresión JPG al 50% de resolución (se puede variar)
            encode_param = [cv2.IMWRITE_JPEG_QUALITY, 50]
            result, encimg = cv2.imencode('.jpg', frame, encode_param)

            if result == False:
                print('Error al codificar imagen')
            encimg = encimg.tobytes()
            # Añadimos a lacola
            self.udp_control.queue_in.put(encimg)


if __name__ == '__main__':

    # Si el programa tiene un argumento, considerar el parametro recibido como
    # el nombre donde se encuentra la configuración de usuario.
    if len(sys.argv) == 2:
        User.filename = sys.argv[1]

    vc = VideoClient("823x504")

    # Crear aquí los threads de lectura, de recepción y,
    # en general, todo el código de inicialización que sea necesario
    # ...

    # Lanza el bucle principal del GUI
    # El control ya NO vuelve de esta función, por lo que todas las
    # acciones deberán ser gestionadas desde callbacks y threads
    vc.start()
