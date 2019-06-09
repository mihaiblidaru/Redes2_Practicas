# -*- coding: utf-8 -*-
# import the library
from appJar import gui
from PIL import Image, ImageTk
import numpy as np
import cv2
import socket
import threading
import time
import queue
import os
from typing import List
import re
from src.server import Server
from src.user import User


class UDPControl():
    def __init__(self, gui, port=6000):
        """Inicializa la instancia de la clase de control del flujo de video
        por UDP. Crea los dos sockets udp y la cola de envio
        """
        self.udp_port_dest = None
        self.addr_dest = None
        self.gui = gui
        self.socket_in = socket.socket(socket.AF_INET,  # Internet
                                       socket.SOCK_DGRAM)  # UDP
        self.socket_in.setblocking(False)
        self.socket_in.settimeout(0.2)

        while port < 65535:
            try:
                self.socket_in.bind(('0.0.0.0', port))
                self.port = port
                break
            except:
                port += 1

        if port == 65536:
            raise Exception(
                "Error reservando puerto para la conexión UDP")
        self.listen_port = port

        self.socket_out = socket.socket(socket.AF_INET,  # Internet
                                        socket.SOCK_DGRAM)  # UDP

        self.sending_video = False
        self.frame_count = 0
        self.fps = 30
        self.stop_threads = False

        self.queue_in = queue.Queue()

    def empezar_videollamada(self):
        """ Crea los dos hilos de recepción y envio de video
        """
        t = threading.Thread(target=self.send_videmy_addro)
        t.daemon = True
        t.start()
        t = threading.Thread(target=self.recive_video)
        t.daemon = True
        t.start()

    def send_videmy_addro(self):
        """ Hilo que se encarga de enviar video. Coge un frame de la cola de envio
        y lo envia por el socket UDP
        """
        while True:
            try:
                img = self.queue_in.get(timeout=0.2)
                self.frame_count += 1
                timestamp = str(time.time())
                MESSAGE = str(self.frame_count)+"#"+timestamp + \
                    "#"+"640x480"+"#"+str(self.fps)+"#"
                MESSAGE = MESSAGE.encode("utf-8") + img

                self.socket_out.sendto(
                    MESSAGE, (self.addr_dest, self.udp_port_dest))

            except:
                pass

            # Como es no bloqueante podemos acabar los hilos
            if self.stop_threads:
                return

    def recive_video(self):
        """ Hilo que se encarga de recibir video. Lee un datagrama del socket UDP
        lo descomprime y lo muestra en la aplicación
        """

        while True:
            try:
                data, _ = self.socket_in.recvfrom(80000)
                img = data.split(b"#", 4)[4]

                # Descompresión de los datos, una vez recibidos
                decimg = cv2.imdecode(np.frombuffer(img, np.uint8), 1)
                frame_compuesto = decimg

                # Conversión de formato para su uso en el GUI
                cv2_im = cv2.cvtColor(frame_compuesto, cv2.COLOR_BGR2RGB)
                img_tk = ImageTk.PhotoImage(Image.fromarray(cv2_im))

                # Lo mostramos en el GUI
                self.gui.app.setImageSize("video", 640, 360)
                self.gui.app.setImageData("video", img_tk, fmt='PhotoImage')

            except socket.timeout:
                pass

            if self.stop_threads:
                return
