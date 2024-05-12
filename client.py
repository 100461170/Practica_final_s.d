from enum import Enum
import argparse
import socket
import threading
import sys
import zeep



class client :

    # ******************** TYPES *********************
    # *
    # * @brief Return codes for the protocol methods
    class RC(Enum) :
        OK = 0
        ERROR = 1
        USER_ERROR = 2
    # ****************** ATTRIBUTES ******************
    _server = None
    _port = -1
    _web_port = None
    # contiene el socket de la conexion cliente-servidor
    _socket_client = None
    # contiene el socket del cliente conectado
    _socket_connect = None
    # contiene el usuario del cliente conectado
    _username = None
    # contiene el thread del usuario conectado
    _thread = None
    # ******************** METHODS *******************


    @staticmethod
    def  register(user):
        """
        La funcion register permite a un usuario registrarse en el sistema
        Args:
            user: nombre de usuario
        Returns:
            - 0 si todo es correcto
            - 1 si el usuario ya esta registrado
            - 2 en caso de error
        """
        # obtener el tiempo de la operacion
        wsdl_url = f"http://localhost:{client._web_port}/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        time_date = soap.service.get_date_time()
        time_date = ''.join(time_date)
        # mandar el codigo de operacion
        client.send_message("register", client._socket_client)
        # mandar la hora
        client.send_message(time_date, client._socket_client)        
        # mandar el usuario
        client.send_message(user, client._socket_client)     
        # recibir la respuesta del servidor
        message = int(client.readString(client._socket_client))
        if message == 0:
            print("c> REGISTER OK")
            return client.RC.OK
        elif message == 1:
            print("c> USERNAME IN USE")
            return client.RC.USER_ERROR
        else:
            print("c> REGISTER FAIL")
            return client.RC.ERROR


    @staticmethod
    def  unregister(user) :
        """
        La funcion unregister permite a un usuario cancelar su registro en el sistema
        Args:
            user: nombre de usuario
        Returns:
            - 0 si todo es correcto
            - 1 si el usuario no esta registrado
            - 2 en caso de error
        """
        # obtener el tiempo de la operacion
        wsdl_url = f"http://localhost:{client._web_port}/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        time_date = soap.service.get_date_time()
        time_date = ''.join(time_date)
        # mandar el codigo de operacion
        client.send_message("unregister", client._socket_client)
        # mandar la hora
        client.send_message(time_date, client._socket_client)
        # mandar el usuario
        client.send_message(user, client._socket_client)
        # recibir la respuesta
        message = int(client.readString(client._socket_client))
        if message == 0:
            print("c> UNREGISTER OK")
            return client.RC.OK
        elif message == 1:
            print("c> USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        else:
            print("c> UNREGISTER FAIL")
            return client.RC.ERROR

    
    @staticmethod
    def  connect(user) :
        """
        La funcion connect permite a un usuario conectarse al sistema
        Args:
            user: nombre de usuario
        Returns:
            - 0 si todo es correcto
            - 1 si el usuario no existe o ya esta conectado
            - 2 en caso de error
        """
        # comprobar si usuario esta conectado
        if client._username != None:
            print("c> CLIENT ALREADY CONNECTED")
            return client.RC.USER_ERROR
        # obtener el tiempo de la operacion
        wsdl_url = f"http://localhost:{client._web_port}/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        time_date = soap.service.get_date_time()
        time_date = ''.join(time_date)
        # crear una tupla ip, puerto, al pasar 0 como parametro se usara un puerto libre 
        connect_addr = (client._server, 0)
        # Se crea el socket necesario para las transferencias de archivos entre clientes
        client._socket_connect = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client._socket_connect.bind(connect_addr)
        # crear un hilo para escuchar las peticiones de otros clientes
        ip, port = client._socket_connect.getsockname()
        client._thread = threading.Thread(target=client.listen)
        # mandar codigo de operacion
        client.send_message("connect", client._socket_client)
        # mandar la hora
        client.send_message(time_date, client._socket_client)
        # mandar nombre de usuario
        username = user
        client.send_message(user, client._socket_client)
        # mandar puerto
        client.send_message(str(port), client._socket_client)
        # recibir la respuesta
        message = int(client.readString(client._socket_client))
        if message == 0:
            print("c> CONNECT OK")
            # Si todo ha ido bien se arranca el hilo de escucha
            client._username = username
            client._thread.start()
            return client.RC.OK
        elif message == 1:
            print("c> CONNECT FAIL, USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif message == 2:
            print("c> USER ALREADY CONNECTED")
            return client.RC.USER_ERROR
        else:
            print("c> CONNECT FAIL")
            return client.RC.ERROR


    
    @staticmethod
    def  disconnect(user) :
        """
        La funcion disconnect permite a un usuario desonectarse al sistema
        Args:
            user: nombre de usuario
        Returns:
            - 0 si todo es correcto
            - 1 si el usuario no existe o no esta conectado
            - 2 en caso de error
        """
        # comprobar si el cliente de esta sesion esta conectado al sistema. Si ya lo esta
        # no se permitira una nueva conexion, ya que solo puede haber un usuario conectado
        # por terminal
        if user != client._username:
            print("c> DISCONNECT FAIL / USER NOT CONNECTED")
            return client.RC.USER_ERROR    
        # obtener el tiempo de la operacion
        wsdl_url = f"http://localhost:{client._web_port}/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        time_date = soap.service.get_date_time()
        time_date = ''.join(time_date)
        # mandar codigo de operacion
        client.send_message("disconnect", client._socket_client)
        # mandar la hora
        client.send_message(time_date, client._socket_client)
        # mandar nombre de usuario
        client.send_message(user, client._socket_client)
        # recibir la respuesta
        message = int(client.readString(client._socket_client))
        if message == 0:
            # obtener IP y addr del socket para la transferencia de archivos
            ip, port = client._socket_connect.getsockname()
            # se crea un socket para cerrar el hilo
            disconnect_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            # Se manda un mensaje de desconexion al hilo de escucha
            disconnect_socket.connect((ip, port))
            client.send_message("end", disconnect_socket)
            client._thread.join()
            client._username = None
            print("c> DISCONNECT OK")
            return client.RC.OK
        elif message == 1:
            print("c> DISCONNECT FAIL / USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif message == 2:
            print("c> DISCONNECT FAIL / USER NOT CONNECTED")
            return client.RC.USER_ERROR
        else:
            print("c> DISCONNECT FAIL")
            return client.RC.ERROR

    @staticmethod
    def  publish(fileName,  description):
        """
        La funcion publish permite a un usuario publicar un fichero en el sistema
        Args:
            fileName: nombre del fichero
            description: descripcion del fichero
        Returns:
            - 0 si todo es correcto
            - 1 si el usuario no existe, ya esta conectado o el contenido ya existe
            - 2 en caso de error
        """
        # comprobar si el cliente de esta sesion esta conectado al sistema. Si ya lo esta
        # no se permitira una nueva conexion, ya que solo puede haber un usuario conectado
        # por terminal
        if client._username == None:
            print("c> PUBLISH FAIL, CLIENT NOT CONNECTED")
            return client.RC.USER_ERROR
        # obtener el tiempo de la operacion
        wsdl_url = f"http://localhost:{client._web_port}/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        time_date = soap.service.get_date_time()
        time_date = ''.join(time_date)
        # mandar codigo de operacion
        client.send_message("publish", client._socket_client)
        # mandar la hora
        client.send_message(time_date, client._socket_client)
        # mandar nombre de usuario
        client.send_message(client._username, client._socket_client)
        # mandar nombre de archivo
        client.send_message(fileName, client._socket_client)
        # mandar descripcion de archivo
        client.send_message(description, client._socket_client)
        # recibir la respuesta
        message = int(client.readString(client._socket_client))
        if message == 0:
            print("c> PUBLISH OK")
            return client.RC.OK
        elif message == 1:
            print("c> PUBLISH FAIL, USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif message == 2:
            print("c> PUBLISH FAIL, USER NOT CONNECTED")
            return client.RC.USER_ERROR
        elif message == 3:
            print("c> PUBLISH FAIL, CONTENT ALREADY PUBLISHED")
            return client.RC.USER_ERROR
        else:
            print("c> PUBLISH FAIL")
            return client.RC.ERROR

    @staticmethod
    def  delete(fileName) :
        """
        La funcion delete permite a un usuario eliminar un fichero publicado en el sistema
        Args:
            fileName: nombre del fichero
        Returns:
            - 0 si todo es correcto
            - 1 si el usuario no existe o ya esta conectado o el contenido no existe
            - 2 en caso de error
        """
        # comprobar si el cliente de esta sesion esta conectado al sistema. Si ya lo esta
        # no se permitira una nueva conexion, ya que solo puede haber un usuario conectado
        # por terminal
        if client._username == None:
            print("c> DELETE FAIL, CLIENT NOT CONNECTED")
            return client.RC.USER_ERROR
        # obtener el tiempo de la operacion
        wsdl_url = f"http://localhost:{client._web_port}/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        time_date = soap.service.get_date_time()
        time_date = ''.join(time_date)
        # mandar codigo de operacion
        client.send_message("delete", client._socket_client)
        # mandar la hora
        client.send_message(time_date, client._socket_client)
        # mandar nombre de usuario
        client.send_message(client._username, client._socket_client)
        # mandar nombre de archivo
        client.send_message(fileName, client._socket_client)
        # recibir la respuesta
        message = int(client.readString(client._socket_client))
        if message == 0:
            print("c> DELETE OK")
            return client.RC.OK
        elif message == 1:
            print("c> DELETE FAIL, USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif message == 2:
            print("c> DELETE FAIL, USER NOT CONNECTED")
            return client.RC.USER_ERROR
        elif message == 3:
            print("c> DELETE FAIL, CONTENT NOT PUBLISHED")
            return client.RC.USER_ERROR
        else:
            print("c> DELETE FAIL")
            return client.RC.ERROR


    @staticmethod
    def  listusers() :
        """
        La funcion list_users permite a un usuario listar el numero de usuarios conectados en el sistema
        Returns:
            - 0 si todo es correcto
            - 1 si el usuario no existe o no esta conectado
            - 2 en caso de error
        """
        # comprobar si el cliente de esta sesion esta conectado al sistema. Si ya lo esta
        # no se permitira una nueva conexion, ya que solo puede haber un usuario conectado
        # por terminal
        if client._username == None:
            print("c> LIST_USERS FAIL , CLIENT NOT CONNECTED")
            return client.RC.USER_ERROR
        # obtener el tiempo de la operacion
        wsdl_url = f"http://localhost:{client._web_port}/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        time_date = soap.service.get_date_time()
        time_date = ''.join(time_date)
        # todo: IMPORTANTE CAMBIAR ESTA FUNCION PARA ENVIAR EL NOMBRE DE USUARIO
        # mandar codigo de operacion
        client.send_message("list_users", client._socket_client)
        # mandar la hora
        client.send_message(time_date, client._socket_client)
        # recibir la respuesta
        message = int(client.readString(client._socket_client))        
        if message == 0:
            print("c> LIST_USERS OK")
            # recibir numero de usuarios
            num_users = int(client.readString(client._socket_client))
            # recibir la respuesta
            for i in range(num_users):
                message = []
                for j in range(3):
                    message.append(client.readString(client._socket_client))
                # Imprimir los usuarios en el shell
                print(message[0], message[1], message[2])
            return client.RC.OK         
        elif message == 1:
            print("c> LIST_USERS FAIL , USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif message == 2:
            print("c> LIST_USERS FAIL , USER NOT CONNECTED")
            return client.RC.USER_ERROR
        else:
            print("c> LIST_USERS FAIL")
            return client.RC.ERROR


    @staticmethod
    def  listcontent(user) :
        """
        La funcion listcontent permite a un usuario mirar el contenido de otro usuario conectado en el sistema
        Args:
            user: nombre de usuario del que se quieren listar los contenidos
        Returns:
            - 0 si todo es correcto
            - 1 si el usuario no existe o no esta conectado o el usuario a buscar no existe
            - 2 en caso de error
        """
        # comprobar si el cliente de esta sesion esta conectado al sistema. Si ya lo esta
        # no se permitira una nueva conexion, ya que solo puede haber un usuario conectado
        # por terminal
        if client._username == None:
            print("c> LIST_CONTENT FAIL , CLIENT NOT CONNECTED")
            return client.RC.USER_ERROR
        # obtener el tiempo de la operacion
        wsdl_url = f"http://localhost:{client._web_port}/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        time_date = soap.service.get_date_time()
        time_date = ''.join(time_date)
        # mandar codigo de operacion
        client.send_message("list_content", client._socket_client)
        # mandar la hora
        client.send_message(time_date, client._socket_client)
        # mandar nombre de usuario de quien realiza la operacion
        client.send_message(client._username, client._socket_client)
        # mandar nombre de usuario
        client.send_message(user, client._socket_client)
        # recibir respuesta
        message = int(client.readString(client._socket_client))
        if message == 0:
            print("c> LIST_CONTENT OK")
            # recibir numero de usuarios
            num_files = int(client.readString(client._socket_client))
            for i in range(num_files):
                # recibir la respuesta
                message = []
                for j in range(2):
                    message.append(client.readString(client._socket_client))
                # imprimir los datos en la shell
                print(message[0], message[1])
            return client.RC.OK    
        elif message == 1:
            print("c> LIST_CONTENT FAIL , USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif message == 2:
            print("c> LIST_CONTENT FAIL , USER NOT CONNECTED")
            return client.RC.USER_ERROR
        elif message == 3:
            print("c> LIST_CONTENT FAIL , REMOTE USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        else:
            print("c> LIST_CONTENT FAIL")
            return client.RC.ERROR

    @staticmethod
    def  getfile(user,  remote_FileName,  local_FileName) :
        """
        La funcion get_file permite a un usuario recibir un archivo de otro cliente
        Args:
            user: nombre de usuario del que cogeremos el fichero
            remote_FileName: ruta del fichero a descargar
            local_FileName: ruta donde se almacenará el fichero
        Returns:
            - 0 si todo es correcto
            - 1 si el usuario no existe o no esta conectado o el usurio remoto no existe o su contenido no existe
            - 2 en caso de error
        """
        # comprobar si el cliente de esta sesion esta conectado al sistema. Si ya lo esta
        # no se permitira una nueva conexion, ya que solo puede haber un usuario conectado
        # por terminal
        if client._username == None:
            print("c> GET_FILE , USER NOT CONNECTED")
            return client.RC.USER_ERROR
        # obtener el tiempo de la operacion
        wsdl_url = f"http://localhost:{client._web_port}/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        time_date = soap.service.get_date_time()
        time_date = ''.join(time_date)
        # primero enviamos una serie de informacion al servidor para comprobar
        # que todo es correcto y recibir la ip y el puerto del usuario remoto
        # enviar la cadena de operacion
        client.send_message("get_file", client._socket_client)
        # mandar la hora
        client.send_message(time_date, client._socket_client)
        # enviar el nombre del cliente
        client.send_message(user, client._socket_client)
        # enviar el nombre del archivo remoto
        client.send_message(remote_FileName, client._socket_client)
        # recibir respuesta
        message = int(client.readString(client._socket_client))
        if message == 0:
            # recibimos el puerto y la ip del usuario remoto
            message = client.readString(client._socket_client)
            ip, port = message.split(' ')
            port = int(port)
            # conectarse al socket del otro usuario 
            get_file_sc = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            get_file_sc.connect((ip, port))
            # mandar el nombre del archivo
            client.send_message(remote_FileName, get_file_sc)
            # leer el valor de retorno
            ret = int(client.readString(get_file_sc))
            if ret == 1:
                print("c> GET_FILE FAIL / FILE NOT EXIST")
                return client.RC.USER_ERROR
            if ret == 2:
                print("c> GET_FILE FAIL")
                return client.RC.ERROR
            # leer el archivo de respuesta en binario
            message = client.readString_binary(get_file_sc)
            # escribir todo el archivo en local
            try:
                with open(local_FileName, "wb") as f:
                    f.write(message)
                    f.close()
            # tratar el error
            except Exception:
                print("c> GET_FILE FAIL")
                return client.RC.ERROR
            # cerrar el socket
            get_file_sc.close()
            print("GET_FILE OK")
        elif message == 1:
            print("c> GET_FILE FAIL / FILE NOT EXIST")
            return client.RC.USER_ERROR
        else:
            print("c> GET_FILE FAIL")
            return client.RC.ERROR
        
    @staticmethod
    def listen():
        """
        La funcion listen se utiliza en los hilos de escucha para mandar un archivo ante una peticion
        Returns:
            - 0 si todo es correcto
            - 1 si el usuario no existe o ya esta conectado o el contenido no existe
            - 2 en caso de error
        """
        # escuchamos las peticiones
        client._socket_connect.listen()
        while client._socket_connect:
            # aceptar conexion
            conn, address = client._socket_connect.accept()
            # recibir mensaje
            message = client.readString(conn)
            # cerramos el hilo si recibimos un end
            if message == "end":
                conn.close()
                client._socket_connect.shutdown(socket.SHUT_RDWR)
                client._socket_connect.close()
                break
            # intentamos abrir el archivo pedido en binario
            str_archivo = b""
            try:
                with open(message, "rb") as f:
                    # leemos el archivo
                    str_archivo += f.read()
                # enviar contenido del archivo
                client.send_message("0", conn)
            # devolvemos 1 si no se encuentra
            except FileNotFoundError:
                client.send_message("1", conn)
            # y devolvemos 2 si hay algun error
            except Exception:
                client.send_message("2", conn)
            conn.sendall(str_archivo)
            conn.close()
        
    # *
    # **
    # * @brief Command interpreter for the client. It calls the protocol functions.
    @staticmethod
    def shell():
        while (True) :
            # Crear el socket y almacenar los datos del servidor
            client._socket_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sever_addres = (client._server, client._port)
            try :
                command = input("c> ")
                line = command.split(" ")
                if (len(line) > 0):
                    line[0] = line[0].upper()
                    if (line[0]=="REGISTER") :
                        if (len(line) == 2) :
                            # Conectarse al servidor   
                            client._socket_client.connect(sever_addres)
                            client.register(line[1])
                            # Cerrar la conexión 
                            client._socket_client.close()
                        else :
                            print("Syntax error. Usage: REGISTER <userName>")

                    elif(line[0]=="UNREGISTER") :
                        if (len(line) == 2) :
                            # Conectarse al servidor
                            client._socket_client.connect(sever_addres)
                            client.unregister(line[1])
                            # Cerrar la conexión
                            client._socket_client.close()
                        else :
                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif(line[0]=="CONNECT") :
                        if (len(line) == 2) :
                            # Conectarse al servidor
                            client._socket_client.connect(sever_addres)
                            client.connect(line[1])
                            # Cerrar la conexión
                            client._socket_client.close()
                            
                        else :
                            print("Syntax error. Usage: CONNECT <userName>")
                    
                    elif(line[0]=="PUBLISH") :
                        if (len(line) >= 3) :
                            #  Eliminar las primeras dos palabras
                            description = ' '.join(line[2:])
                            # Conectarse al servidor
                            client._socket_client.connect(sever_addres)
                            client.publish(line[1], description)
                            # Cerrar la conexión
                            client._socket_client.close()
                        else :
                            print("Syntax error. Usage: PUBLISH <fileName> <description>")

                    elif(line[0]=="DELETE") :
                        if (len(line) == 2) :
                            # Conectarse al servidor
                            client._socket_client.connect(sever_addres)
                            client.delete(line[1])
                            # Cerrar la conexión
                            client._socket_client.close()
                        else :
                            print("Syntax error. Usage: DELETE <fileName>")

                    elif(line[0]=="LIST_USERS") :
                        if (len(line) == 1) :
                            # Conectarse al servidor
                            client._socket_client.connect(sever_addres)
                            client.listusers()
                            # Cerrar la conexión
                            client._socket_client.close()
                        else :
                            print("Syntax error. Use: LIST_USERS")

                    elif(line[0]=="LIST_CONTENT") :
                        if (len(line) == 2) :
                            # Conectarse al servidor
                            client._socket_client.connect(sever_addres)
                            client.listcontent(line[1])
                            # Cerrar la conexión
                            client._socket_client.close()
                        else :
                            print("Syntax error. Usage: LIST_CONTENT <userName>")

                    elif(line[0]=="DISCONNECT") :
                        if (len(line) == 2) :
                            # Conectarse al servidor
                            client._socket_client.connect(sever_addres)
                            client.disconnect(line[1])
                            # Cerrar la conexión
                            client._socket_client.close()
                        else :
                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif(line[0]=="GET_FILE") :
                        if (len(line) == 4) :
                            # Conectarse al servidor
                            client._socket_client.connect(sever_addres)
                            client.getfile(line[1], line[2], line[3])
                            # Cerrar la conexión
                            client._socket_client.close()
                        else :
                            print("Syntax error. Usage: GET_FILE <userName> <remote_fileName> <local_fileName>")

                    elif(line[0]=="QUIT") :
                        if (len(line) == 1) :
                            # Al hace QUIT se realiza un disconnect
                            if client._username != None:
                                # Conectarse al servidor
                                client._socket_client.connect(sever_addres)
                                client.disconnect(client._username)
                                # Cerrar la conexión
                                client._socket_client.close()
                            break
                        else :
                            print("Syntax error. Use: QUIT")
                    else :
                        print("Error: command " + line[0] + " not valid.")
            except Exception as e:
                print("Exception: " + str(e))
                

    # *
    # * @brief Prints program usage
    @staticmethod
    def usage() :
        print("Usage: python3 client.py -s <server> -p <port>")


    # *
    # * @brief Parses program execution arguments
    @staticmethod
    def  parseArguments(argv) :
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        parser.add_argument('-ws', type=int, required=True, help='Server Port')
        args = parser.parse_args()


        if (args.s is None):
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False

        if ((args.p < 1024) or (args.p > 65535)):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535")
            return False
        
        if ((args.ws < 1024) or (args.ws > 65535)):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535")
        
        client._server = args.s
        client._port = args.p
        client._web_port = args.ws

        return True
    
    def readString(sock):
        a = ''
        while True:
            msg = sock.recv(1)
            if (msg == b'\0'):
                break
            a += msg.decode()
        return(a)
    
    def readString_binary(sock):
        a = b""
        while True:
            msg = sock.recv(1)
            if not msg:
                break
            a += msg
        return(a)

    def send_message(message, socket):
        message+= "\0"
        message = message.encode("utf-8")
        socket.sendall(message)
        return message

    # ******************** MAIN *********************
    @staticmethod
    def main(argv) :
        # si no se llama bien al cliente imprimimos como se usa
        if (not client.parseArguments(argv)) :
            client.usage()
            return
        # llamamos a la shell
        client.shell()
        print("+++ FINISHED +++")
    

if __name__=="__main__":
    client.main([])