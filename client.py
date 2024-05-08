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
    def  register(user) :
        # obtener el tiempo de la operacion
        wsdl_url = "http://localhost:8000/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        result = soap.service.get_date_time()
        result = ''.join(result)
        # mandar el codigo de operacion
        message = b'register\0'
        client._socket_client.sendall(message)
        # mandar la hora
        result += '\0'
        result = result.encode('utf-8')
        client._socket_client.sendall(result)
        # mandar el usuario
        cadena = user + '\0'
        message = cadena.encode("UTF-8")
        client._socket_client.sendall(message)
        # recibir la respuesta
        message = client._socket_client.recv(1)
        message = int(message.decode('utf-8'))
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
        # obtener el tiempo de la operacion
        wsdl_url = "http://localhost:8000/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        result = soap.service.get_date_time()
        result = ''.join(result)
        # mandar el codigo de operacion
        message = b'unregister\0'
        client._socket_client.sendall(message)
        # mandar la hora
        result += '\0'
        result = result.encode('utf-8')
        client._socket_client.sendall(result)
        # mandar el usuario
        username = user + '\0'
        message = username.encode("UTF-8")
        client._socket_client.sendall(message)
        # recibir la respuesta
        message = client._socket_client.recv(1)
        message = int(message.decode('utf-8'))
        if message == 0:
            # confirmar si es el usuario conectado
            if username == client._username:
                client._username = None
                # matar el hilo
                client._socket_connect.shutdown(socket.SHUT_RDWR)
                client._socket_connect.close()
                client._thread.join()
            
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
        # obtener el tiempo de la operacion
        wsdl_url = "http://localhost:8000/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        result = soap.service.get_date_time()
        result = ''.join(result)
        # comprobar si usuario esta conectado
        if client._username != None:
            print("c> CLIENT ALREADY CONNECTED")
            return client.RC.USER_ERROR
        # crear una tupla ip, puerto 0-> usar puerto libre 
        connect_addr = (client._server, 0)
        # crear socket
        client._socket_connect = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client._socket_connect.bind(connect_addr)
        # crear hilo
        ip, port = client._socket_connect.getsockname()
        client._thread = threading.Thread(target=client.listen, args=(user, ip, port))
        # mandar codigo de operacion
        message = b'connect\0'
        client._socket_client.sendall(message)
        # mandar la hora
        result += '\0'
        result = result.encode('utf-8')
        client._socket_client.sendall(result)
        # mandar nombre de usuario
        cadena = user + '\0'
        username = cadena
        message = cadena.encode("UTF-8")
        client._socket_client.sendall(message)
        # mandar puerto
        cadena = str(port) + '\0'
        message = cadena.encode("UTF-8")
        client._socket_client.sendall(message)
        # recibir la respuesta
        message = client._socket_client.recv(1)
        message = int(message.decode('utf-8'))
        if message == 0:
            print("c> CONNECT OK")
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
        # obtener el tiempo de la operacion
        wsdl_url = "http://localhost:8000/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        result = soap.service.get_date_time()
        result = ''.join(result)
        # mandar codigo de operacion
        message = b'disconnect\0'
        client._socket_client.sendall(message)
        # mandar la hora
        result += '\0'
        result = result.encode('utf-8')
        client._socket_client.sendall(result)
        # mandar nombre de usuario
        cadena = user + '\0'
        message = cadena.encode("UTF-8")
        client._socket_client.sendall(message)
        # recibir la respuesta
        message = client._socket_client.recv(1)
        message = int(message.decode('utf-8'))
        if message == 0:
            client._socket_connect.shutdown(socket.SHUT_RDWR)
            client._socket_connect.close()
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
    def  publish(fileName,  description) :
        # obtener el tiempo de la operacion
        wsdl_url = "http://localhost:8000/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        result = soap.service.get_date_time()
        result = ''.join(result)
        # comprobar si el usuario no esta conectado
        if client._username == None:
            print("c> PUBLISH FAIL, CLIENT NOT CONNECTED")
            return client.RC.USER_ERROR
        # mandar codigo de operacion
        message = b'publish\0'
        client._socket_client.sendall(message)
        # mandar la hora
        result += '\0'
        result = result.encode('utf-8')
        client._socket_client.sendall(result)
        # mandar nombre de usuario
        cadena = client._username
        message = cadena.encode("UTF-8")
        client._socket_client.sendall(message)
        # mandar nombre de archivo
        cadena = fileName + '\0'
        message = cadena.encode("UTF-8")
        client._socket_client.sendall(message)
        # mandar descripcion de archivo
        cadena = description + '\0'
        message = cadena.encode("UTF-8")
        client._socket_client.sendall(message)
        
        # recibir la respuesta
        message = client._socket_client.recv(1)
        message = int(message.decode('utf-8'))
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
        # obtener el tiempo de la operacion
        wsdl_url = "http://localhost:8000/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        result = soap.service.get_date_time()
        result = ''.join(result)
        # comprobar si el usuario no esta conectado
        if client._username == None:
            print("c> DELETE FAIL, CLIENT NOT CONNECTED")
            return client.RC.USER_ERROR
        # mandar codigo de operacion
        message = b'delete\0'
        client._socket_client.sendall(message)
        # mandar la hora
        result += '\0'
        result = result.encode('utf-8')
        client._socket_client.sendall(result)
        # mandar nombre de usuario
        cadena = client._username
        message = cadena.encode("UTF-8")
        client._socket_client.sendall(message)
        # mandar nombre de archivo
        cadena = fileName + '\0'
        message = cadena.encode("UTF-8")
        client._socket_client.sendall(message)
        
        # recibir la respuesta
        message = client._socket_client.recv(1)
        message = int(message.decode('utf-8'))
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
        # obtener el tiempo de la operacion
        wsdl_url = "http://localhost:8000/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        result = soap.service.get_date_time()
        result = ''.join(result)
        # comprobar si el usuario no esta conectado
        if client._username == None:
            print("c> LIST_USERS FAIL , CLIENT NOT CONNECTED")
            return client.RC.USER_ERROR
        # mandar codigo de operacion
        message = b'list_users\0'
        client._socket_client.sendall(message)
        # mandar la hora
        result += '\0'
        result = result.encode('utf-8')
        client._socket_client.sendall(result)
        # recibir la respuesta
        message = client._socket_client.recv(1)
        message = int(message.decode('utf-8'))
        if message == 0:
            print("c> LIST_USERS OK")
            # recibir numero de usuarios
            message = client._socket_client.recv(2)
            message = message.decode('utf-8')
            # recibir la respuesta
            message = client._socket_client.recv(1024)
            message = message.decode('utf-8')
            print(message[:-4]) # quitar el \n y \0
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
        # obtener el tiempo de la operacion
        wsdl_url = "http://localhost:8000/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        result = soap.service.get_date_time()
        result = ''.join(result)
        # comprobar si el usuario no esta conectado
        if client._username == None:
            print("c> LIST_CONTENT FAIL , CLIENT NOT CONNECTED")
            return client.RC.USER_ERROR
        # mandar codigo de operacion
        message = b'list_content\0'
        client._socket_client.sendall(message)
        # mandar la hora
        result += '\0'
        result = result.encode('utf-8')
        client._socket_client.sendall(result)
        # mandar nombre de usuario de quien realiza la operacion
        message = client._username.encode('utf-8')
        client._socket_client.sendall(message)
        # mandar nombre de usuario
        cadena = user + '\0'
        message = cadena.encode('utf-8')
        client._socket_client.sendall(message)
        # recibir respuesta
        message = client._socket_client.recv(1)
        message = int(message.decode('utf-8'))
        if message == 0:
            print("c> LIST_CONTENT OK")
            # recibir numero de usuarios
            message = client._socket_client.recv(2)
            message = message.decode('utf-8')
            # recibir la respuesta
            message = client._socket_client.recv(2048)
            message = message.decode('utf-8')
            print(message[:-2]) # quitar el \n y \0
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
        # obtener el tiempo de la operacion
        wsdl_url = "http://localhost:8000/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        result = soap.service.get_date_time()
        result = ''.join(result)
        # comprobar que el usuario esta registrado y conectado
        if client._username == None:
            print("c> GET_FILE , USER NOT CONNECTED")
            return client.RC.USER_ERROR
        # enviar la cadena de operacion
        message = b'get_file\0'
        client._socket_client.sendall(message)
        # mandar la hora
        result += '\0'
        result = result.encode('utf-8')
        client._socket_client.sendall(result)
        # enviar el nombre del cliente
        cadena = user + '\0'
        message = cadena.encode('utf-8')
        client._socket_client.sendall(message)
        # enviar el nombre del archivo remoto
        cadena = remote_FileName + '\0'
        message = cadena.encode('utf-8')
        client._socket_client.sendall(message)
        # recibir respuesta
        message = client._socket_client.recv(1)
        message = int(message.decode('utf-8'))
        if message == 0:
            message = client._socket_client.recv(1024)
            message = message.decode('utf-8')
            ip, port = message.split(' ')
            ip = ip.split('\0')[1]
            port = int(port[:-1])
            # conectarse al socket de estas caracteristicas
            get_file_sc = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            get_file_sc.connect((ip, port))
            # mandar el nombre del archivo
            cadena = remote_FileName + '\0'
            message = cadena.encode('utf-8')
            get_file_sc.sendall(message)
            # leer todo el archivo de respuesta
            message = ''
            while True:
                msg = get_file_sc.recv(1)
                if (msg == b'\0'):
                    break
                message += msg.decode()
            # escribir todo el archivo en local
            with open(local_FileName, "w+") as f:
                f.write(message)
                f.close()
            # cerrar socket
            get_file_sc.close()
            print("GET_FILE OK")
        elif message == 1:
            print("c> GET_FILE FAIL / FILE NOT EXIST")
            return client.RC.USER_ERROR
        else:
            print("c> GET_FILE FAIL")
            return client.RC.ERROR
        
    @staticmethod
    def listen(user, ip, port):
        try:
            client._socket_connect.listen()
            while client._socket_connect:
                # aceptar conexion
                conn, address = client._socket_connect.accept()
                # recibir mensaje
                message = conn.recv(1024)
                message = message.decode('utf-8')
                # abir archivo pedido
                str_archivo = ""
                with open(message[:-1], "r") as f:
                    str_archivo += f.read()
                # escribir contenido del archivo
                str_archivo += '\0'
                str_archivo = str_archivo.encode("utf-8")
                conn.send(str_archivo)
                conn.close()
                
        except Exception:
            pass
        # return client.RC.OK

    # *
    # **
    # * @brief Command interpreter for the client. It calls the protocol functions.
    @staticmethod
    def shell():
        while (True) :
            client._socket_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sever_addres = (client._server, client._port)
            try :
                command = input("c> ")
                line = command.split(" ")
                if (len(line) > 0):

                    line[0] = line[0].upper()

                    if (line[0]=="REGISTER") :
                        if (len(line) == 2) :   
                            client._socket_client.connect(sever_addres)
                            client.register(line[1])
                            client._socket_client.close()
                        else :
                            print("Syntax error. Usage: REGISTER <userName>")

                    elif(line[0]=="UNREGISTER") :
                        if (len(line) == 2) :
                            client._socket_client.connect(sever_addres)
                            client.unregister(line[1])
                            client._socket_client.close()
                        else :
                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif(line[0]=="CONNECT") :
                        if (len(line) == 2) :
                            client._socket_client.connect(sever_addres)
                            client.connect(line[1])
                            client._socket_client.close()
                            
                        else :
                            print("Syntax error. Usage: CONNECT <userName>")
                    
                    elif(line[0]=="PUBLISH") :
                        if (len(line) >= 3) :
                            #  Remove first two words
                            description = ' '.join(line[2:])
                            client._socket_client.connect(sever_addres)
                            client.publish(line[1], description)
                            client._socket_client.close()
                        else :
                            print("Syntax error. Usage: PUBLISH <user> <fileName> <description>")

                    elif(line[0]=="DELETE") :
                        if (len(line) == 2) :
                            client._socket_client.connect(sever_addres)
                            client.delete(line[1])
                            client._socket_client.close()
                        else :
                            print("Syntax error. Usage: DELETE <user> <fileName>")

                    elif(line[0]=="LIST_USERS") :
                        if (len(line) == 1) :
                            client._socket_client.connect(sever_addres)
                            client.listusers()
                            client._socket_client.close()
                        else :
                            print("Syntax error. Use: LIST_USERS")

                    elif(line[0]=="LIST_CONTENT") :
                        if (len(line) == 2) :
                            client._socket_client.connect(sever_addres)
                            client.listcontent(line[1])
                            client._socket_client.close()
                        else :
                            print("Syntax error. Usage: LIST_CONTENT <userName>")

                    elif(line[0]=="DISCONNECT") :
                        if (len(line) == 2) :
                            client._socket_client.connect(sever_addres)
                            client.disconnect(line[1])
                            client._socket_client.close()
                        else :
                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif(line[0]=="GET_FILE") :
                        if (len(line) == 4) :
                            client._socket_client.connect(sever_addres)
                            client.getfile(line[1], line[2], line[3])
                            client._socket_client.close()
                        else :
                            print("Syntax error. Usage: GET_FILE <userName> <remote_fileName> <local_fileName>")

                    elif(line[0]=="QUIT") :
                        if (len(line) == 1) :
                            """TODO: ask if this is correct."""
                            if client._username != None:
                                client._socket_client.connect(sever_addres)
                                client.disconnect(client._username)
                                client._thread.join()
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
        args = parser.parse_args()


        if (args.s is None):
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False

        if ((args.p < 1024) or (args.p > 65535)):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535");
            return False
        
        client._server = args.s
        client._port = args.p

        return True


    # ******************** MAIN *********************
    @staticmethod
    def main(argv) :
        
        if (not client.parseArguments(argv)) :
            client.usage()
            return
        client.shell()
        print("+++ FINISHED +++")
    

if __name__=="__main__":
    client.main([])