//
// Created by linux-lex on 29/02/24.
//
#include "servidor.h"
#include "communications.h"
#include "operaciones.h"
#include <mqueue.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>



// mutex para copiar la peticion recibida a una variable local
pthread_mutex_t sync_mutex;
bool sync_copied = false;
pthread_cond_t sync_cond;
// sockets del servidor
int sd, sc;
// mutex para hacer peticiones concurrentes atómicas
pthread_mutex_t almacen_mutex;
// Almacen dinámico de tuplas
struct tupla* almacen = NULL;
// numero de elementos en el array
int n_elementos = 0;
// elmentos maximos del array
int max_tuplas = 50;

int main (int argc, char *argv[]){
    // Tratamiento de errores para los parametros del servidor
    if (argc < 3){
        fprintf(stderr, "Error: se necesitan mas argumentos en linea de comandos\n");
        return -1;
    }
    if (strcmp("-p", argv[1])!=0){
        fprintf(stderr, "Error: el argumento 1 tiene que ser: -p\n");
        return -1;
    }
    char *ending_char;
    int port_number = strtol(argv[2], &ending_char, 10);
    if ('\0' != *ending_char)
    {
        fprintf(stderr, "Error: el argumento 2 debe ser un numero de puerto.\n");
        return -1;
    }
    if (port_number < 1024 || port_number > 65535)
    {
        fprintf(stderr, "Error: el puerto tiene que estar en el rango 1024-65535.\n");
        return -1;
    }
    // Inicializamos el almacén
    almacen = (struct tupla*)malloc(max_tuplas*sizeof(struct tupla));
    // signal para cerrar servidor
    signal(SIGINT, close_server);
    // cargamos los datos del almacen
    if (-1 == load()) {
        fprintf(stderr, "Error en servidor al cargar el almacen del archivo binary\n");
        return -1;
    }
    // Inicialización de variables e hilos
    int contador = 0;
    pthread_attr_t attr;
    pthread_t thid[MAX_STR];
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    // Inicialización de mutex y variable condicion
    pthread_mutex_init(&sync_mutex, NULL);
    pthread_mutex_init(&almacen_mutex, NULL);
    pthread_cond_init(&sync_cond, NULL);
    // Abrimos el socket del servidor
    sd = serverSocket(port_number, SOCK_STREAM);
    if (sd < 0) {
        perror("SERVER: Error en serverSocket");
        return 0;
    }
    // Obtenemos el host e imprimimos el mensaje e inicialización del servidor
    char machine[MAX_STR];
    int err = gethostname(machine, MAX_STR);
    if (err == -1){
        fprintf(stderr, "Error: no se pudo obtener el nombre del host.\n");
        return -1;
    }
    printf("init server %s:%d\n", machine, port_number);
    // Bucle infinito para tratar peticiones de los clientes
    while (1) {
        // Aceptar un cliente
        sc = serverAccept(sd);
        if (sc < 0) {
            perror("Error en serverAccept");
            return -1;
        }
        // Crear hilo para tratar la petición del cliente
        if (pthread_create(&thid[contador], &attr, tratar_peticion, (void *) &sc) != 0) {
            perror("Error en servidor. Pthread_create");
            return -1;
        }
        // Esperamos hasta que la operacion se haya copiado satisfactoriamente en el hilo
        pthread_mutex_lock(&sync_mutex) ;
        while (sync_copied == false) {
            pthread_cond_wait(&sync_cond, &sync_mutex) ;
        }
        sync_copied = false;
        pthread_mutex_unlock(&sync_mutex) ;
        // Contador para los hilos
        contador++;
    }
    
    return 0;
}

void * tratar_peticion (void* pp){
    // Adquirimos el mutex para copiar la peticion pasada por parametro
    pthread_mutex_lock(&sync_mutex);
    int sc_local = * (int*) pp;
    sync_copied = true;
    char operation[MAX_STR] = "";
    pthread_cond_signal(&sync_cond);
    pthread_mutex_unlock(&sync_mutex);
    int resp;
    // Creamos el host RPC
    char *host;
    host = getenv("IP_TUPLAS");
    // Comprobamos que la variable de entorno está bien definida
    if (host == NULL){
        fprintf(stderr, "Variable de entorno IP_TUPLA no definida.\n");
        return NULL;
    }
    // TODO: comentar esto
    CLIENT *clnt;
    enum clnt_stat retval_1;
    int result_1;
    struct operation_log op_log;
    memset(&op_log, 0, sizeof(struct operation_log));
    // Iniciamos el servidor RPC
    clnt = clnt_create(host, RPC, RPCVER, "tcp");
    if (clnt == NULL) {
        clnt_pcreateerror(host);
        return NULL;
    }
    // Recibimos la operación del cliente
    int ret = readLine(sc_local, operation, sizeof(char)*MAX_STR);
    if (ret < 0){
        perror("Error en recepcion");
        closeSocket(sc_local);
        return NULL;
    }
    // por si recibe una cadena vacia, necesario para tratar ciertos errores
    if (strcmp(operation, "") == 0){
        printf("Se obtuvo una operacion vacia.\n");
        closeSocket(sc_local);
        return NULL;
    }
    // recibimos la fecha y hora de la operacion
    char fecha_hora[MAX_STR];
    ret = readLine(sc_local, fecha_hora, sizeof(char) * MAX_STR);
    if (ret < 0){
        perror("Error en recepcion");
        closeSocket(sc_local);
        return NULL;
    }
    // copiamos la operacion y la fecha a la estructura RPC
    strcpy(op_log.operation, operation);
    strcpy(op_log.date_time, fecha_hora);
    // Transformamos la operacion a un entero
    int num_op = str2int(operation);
    // En funcion de la operacion especificada en la peticion hacemos una u otra operacion
    switch (num_op){
    case 0:
        resp = s_register(sc_local, &op_log);
        break;
    case 1:
        resp = s_unregister(sc_local, &op_log);
        break;
    case 2:
        resp = s_connect(sc_local, &op_log);
        break;
    case 3:
        resp = s_publish(sc_local, &op_log);
        break;
    case 4:
        resp = s_delete(sc_local, &op_log);
        break;
    case 5:
        resp = s_list_users(sc_local);
        break;
    case 6:
        resp = s_list_content(sc_local, &op_log);
        break;
    case 7:
        resp = s_disconnect(sc_local, &op_log);
        break;
    case 8:
        resp = s_get_file(sc_local, &op_log);
        break;
    default:
        resp = -1;
        break;
    }
    // TODO: comentar esto
    if (num_op != 5 && num_op != 6 && num_op != 8){
        char resp_str[2];
        sprintf(resp_str, "%d", resp);
        ret = writeLine(sc_local, resp_str);
        if (ret == -1) {
            perror("Error en envio");
            closeSocket(sc_local);
            exit(-1);
        }
    }
    // mandar mensaje RPC
    retval_1 = send_op_log_1(op_log, &result_1, clnt);
    if (retval_1 != RPC_SUCCESS){
        clnt_perror(clnt, "call failed");
    }
    // Cerramos la conexion con el cliente
    closeSocket(sc_local);
    return NULL;
}

int s_register(int sc_local, operation_log *op_log){
    /* Operacion register. Esta operacion se encarga de registrar un usuario
    en el sistema. La funcion devuelve:
        - 0: si la operacion es satisfactoria
        - 1: si el usuario ya esta registrado
        - 2: si ocurre algun otro error    */
    pthread_mutex_lock(&almacen_mutex);
    // recibimos el nombre del usuario
    char username[MAX_STR];
    int ret = readLine(sc_local, username, sizeof(char) * MAX_STR);
    // En caso de error devolvemos 2
    if (ret < 0){
        perror("Error en recepcion");
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }
    strcpy(op_log->username, username);
    // Comprobamos si el usuario ya está registrado iterando en el almacén
    // Si no lo está devolvemos 1
    for (int i = 0; i < n_elementos; i++){
        if (strcmp(almacen->cliente, username) == 0){
            pthread_mutex_unlock(&almacen_mutex);
            return 1;
        }
    }
    // comprobamos que el almacen dinámico tenga espacio, si no lo duplicamos
    if (n_elementos == max_tuplas){
        // duplicar tamanio de almacen
        almacen = realloc(almacen, 2 * max_tuplas * sizeof(struct tupla));
        max_tuplas = max_tuplas * 2;
    }
    // Registrar al usuario en el sistema insertandolo en el almacen
    struct tupla insert;
    memset(&insert, 0, sizeof(struct tupla));
    strcpy(insert.cliente, username);
    almacen[n_elementos] = insert;
    n_elementos++;
    pthread_mutex_unlock(&almacen_mutex);
    return 0;
}

int s_unregister(int sc_local, operation_log *op_log){
    /* Funcion unregister. Esta funcion se encarga de eliminar a un usuario
    ya registrado. La funcion devuelve:
        - 0: si la operacion es satisfactoria
        - 1: si el usuario no esta registrado
        - 2: si ocurre algun otro error*/
    pthread_mutex_lock(&almacen_mutex);
    // recibir el nombre del usuario
    char username[MAX_STR];
    int ret = readLine(sc_local, username, sizeof(char) * MAX_STR);
    // En caso de error devolvemos 2
    if (ret < 0){
        perror("Error en recepcion");
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }
    strcpy(op_log->username, username);
    // Miramos si el usuario está registrado
    for (int i = 0; i < n_elementos; i++){
        if (strcmp(almacen[i].cliente, username) == 0){
            // Si el cliente está registrado pero está conectado no podemos eliminarlo y
            // devolveremos 2
            if (almacen[i].connected > 0){
                pthread_mutex_unlock(&almacen_mutex);
                return 2;
            }
            // si esta registrado y no conectado lo eliminamos del almacen
            // copiar ultimo elemento del almacen al indice
            almacen[i] = almacen[n_elementos-1];
            // borrar ultimo elemento del almacen
            struct tupla tupla_vacia;
            memset(&tupla_vacia, 0, sizeof(struct tupla));
            almacen[n_elementos-1] = tupla_vacia;
            // bajar el numero de elementos
            n_elementos--;
            pthread_mutex_unlock(&almacen_mutex);
            // devolvemos 0
            return 0;
        }
    }    
    pthread_mutex_unlock(&almacen_mutex);
    // si el usuario no estaba registrado devolvemos 1
    return 1;
}
int s_connect(int sc_local, operation_log *op_log){
    /* Funcion connect. Esta funcion permite al usuario conectarse al sistema
    de manera que pueda realizar operaciones en el servidor. La funcion devuelve:
        - 0: si la operacion es satisfactoria
        - 1: si el usuario no esta registrado
        - 2: si el usuario ya esta conectado
        - 3: en cualquier otro caso de error*/
    pthread_mutex_lock(&almacen_mutex);
    // obtener el nombre de usuario
    char username[MAX_STR];
    int ret = readLine(sc_local, username, sizeof(char) * MAX_STR);
    // en caso de error devolvemos 3
    if (ret < 0){
        perror("Error en recepcion");
        pthread_mutex_unlock(&almacen_mutex);
        return 3;
    }
    strcpy(op_log->username, username);
    // obtener puerto del cliente
    char puerto_str[MAX_STR];
    ret = readLine(sc_local, puerto_str, sizeof(char) * MAX_STR);
    // en caso de error devolvemos 3
    if (ret < 0){
        perror("Error en recepcion");
        pthread_mutex_unlock(&almacen_mutex);
        return 3;
    }
    int port_number = strtol(puerto_str, NULL, 10);
    
    // comprobamos si el usuario esta registrado
    int existe = 1;         // valor a devolver en el caso de que no existiese
    int index;
    for (int i = 0; i < n_elementos; i++){
        if (strcmp(almacen[i].cliente, username) == 0){
            existe = 0;
            index = i;
        }
    }
    if (existe > 0){
        pthread_mutex_unlock(&almacen_mutex);
        return existe;
    }
    // comporbar si el cliente ya esta conectado, en cuyo caso devolveremos 2
    if (almacen[index].connected > 0){
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }
    // obtener la direccion ip del cliente con getpeername
    char ip[MAX_STR];
    struct sockaddr_in client_addr;
    socklen_t size;
    size = sizeof(client_addr);
    getpeername(sc_local, (struct sockaddr *)&client_addr, (socklen_t *)&size);
    strcpy(ip, inet_ntoa(client_addr.sin_addr));
    // Actualizar el estado de conexion del usuario en el sistema, guardando puerto e ip y devolver 0
    almacen[index].connected = 1;
    almacen[index].puerto = port_number;
    strcpy(almacen[index].ip, ip);
    pthread_mutex_unlock(&almacen_mutex);
    return 0;
}

int s_publish(int sc_local, operation_log *op_log){
    /* Funcion publish. Esta funcion permite a un usuario publicar un fichero
    en el sistema. La funcion devuelve:
        - 0: si la operacion es satisfactoria
        - 1: si el usuario no esta registrado
        - 2: si el usuario no esta conectado
        - 3: si el fichero ya esta publicado
        - 4: si hay algun error */
    pthread_mutex_lock(&almacen_mutex);
    // obtener nombre de usuario
    char username[MAX_STR];
    int ret = readLine(sc_local, username, sizeof(char) * MAX_STR);
    // en caso de error devolvemos 4
    if (ret < 0){
        perror("Error en recepcion");
        pthread_mutex_unlock(&almacen_mutex);
        return 4;
    }
    // obtener el nombre del archivo que quiere publicar el cliente
    char filename[MAX_STR];
    ret = readLine(sc_local, filename, sizeof(char) * MAX_STR);
    // devolver 4 en caso de error
    if (ret < 0){
        perror("Error en recepcion");
        pthread_mutex_unlock(&almacen_mutex);
        return 4;
    }
    strcpy(op_log->username, username);
    strcpy(op_log->file_name, filename);
    // obtener la descripcion del archivo
    char description[MAX_STR];
    ret = readLine(sc_local, description, sizeof(char) * MAX_STR);
    // devolver 4 en caso de error
    if (ret < 0){
        perror("Error en recepcion");
        pthread_mutex_unlock(&almacen_mutex);
        return 4;
    }
    // comprobar si existe el usuario
    int existe = 1;         // valor a devolver en el caso de que no existiese
    int index;
    for (int i = 0; i < n_elementos; i++){
        if (strcmp(almacen[i].cliente, username) == 0){
            existe = 0;
            index = i;
        }
    }
    if (existe > 0){
        pthread_mutex_unlock(&almacen_mutex);
        return existe;
    }
    // comprobar si el cliente no esta conectado y devolver 2 si no lo está
    if (almacen[index].connected == 0){
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }
    int file_counter = almacen[index].file_count;
    // comprobar si el contenido ya ha sido publicado y devolver 3 si es asi
    for (int i = 0; i < file_counter; i++){
        if (strcmp(almacen[index].files[i].name, filename) == 0){
            pthread_mutex_unlock(&almacen_mutex);
            return 3;
        }
    }
    // insertar el contenido en el almacen y devolver 0
    strcpy(almacen[index].files[file_counter].name, filename);
    strcpy(almacen[index].files[file_counter].descr, description);
    almacen[index].file_count++;
    pthread_mutex_unlock(&almacen_mutex);
    return 0;
}
int s_delete(int sc_local, operation_log *op_log){
    /* Operacion delete. Esta operacion permite eliminar un fichero publicado. 
    La funcion devuelve:
        - 0: si la operacion es satisfactoria
        - 1: si el usuario no esta registrado
        - 2: si el usuario no esta conectado
        - 3: si el fichero no existe
        - 4: si hay cualquier error */
    pthread_mutex_lock(&almacen_mutex);
    // obtener nombre de usuario
    char username[MAX_STR];
    int ret = readLine(sc_local, username, sizeof(char) * MAX_STR);
    // devolvemos 4 en caso de error
    if (ret < 0){
        perror("Error en recepcion");
        pthread_mutex_unlock(&almacen_mutex);
        return 4;
    }
    // obtener nombre del archivo
    char filename[MAX_STR];
    ret = readLine(sc_local, filename, sizeof(char) * MAX_STR);
    // devolver 4 en caso de error
    if (ret < 0){
        perror("Error en recepcion");
        pthread_mutex_unlock(&almacen_mutex);
        return 4;
    }
    strcpy(op_log->username, username);
    strcpy(op_log->file_name, filename);
    // comprobar si existe el usuario iterando en el almacen
    int existe = 1;         // valor a devolver en el caso de que no existiese
    int index;
    for (int i = 0; i < n_elementos; i++){
        if (strcmp(almacen[i].cliente, username) == 0){
            existe = 0;
            index = i;
        }
    }
    if (existe > 0){
        pthread_mutex_unlock(&almacen_mutex);
        return existe;
    }
    // comprobar si el cliente esta conectado, si no lo esta devolvemos 2
    if (almacen[index].connected == 0){
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }
    // comprobar si el contenido ha sido publicado, si no lo esta devolvemos 3
    // almacenaremos el indice del fichero al buscarlo para poder eliminarlo luego
    int file_counter = almacen[index].file_count;
    int no_existe_fichero = 3;
    int indice_fichero = 0;
    for (int i = 0; i < file_counter; i++){
        if (strcmp(almacen[index].files[i].name, filename) == 0){
            no_existe_fichero = 0;
            indice_fichero = i;
        }
    }
    if (no_existe_fichero > 0){
        pthread_mutex_unlock(&almacen_mutex);
        return no_existe_fichero;
    }
    // Borrar el fichero del almacen y devolver 0
    struct file file_vacio;
    memset(&file_vacio, 0, sizeof(struct file));
    // todo: comentar como funciona el borrar un fichero
    almacen[index].files[indice_fichero] = almacen[index].files[file_counter-1];
    almacen[index].files[file_counter-1] = file_vacio;
    almacen[index].file_count--;
    pthread_mutex_unlock(&almacen_mutex);
    return 0;
}

int s_list_users(int sc_local){
    /* Funcion list_users. Esta funcion permite listar los usuarios conectados
    en el sistema. La funcion devuelve:
        - 0: si la operacion es satisfactoria
        - 1: si el usuario no esta registrado
        - 2: si el usuario no esta conectado
        - 3: en cualquier otro caso de error*/
    pthread_mutex_lock(&almacen_mutex);
    // comprobar si hay clientes registrados en el sistema
    int hay_clientes = 1;
    if (n_elementos == 0){
        hay_clientes = 0;
    }
    // todo: comentar esto
    if (hay_clientes == 0){
        int ret = writeLine(sc_local, "1");
        if (ret == -1) {
            pthread_mutex_unlock(&almacen_mutex);
            writeLine(sc_local, "3");
            return 3;
        }
    }
    // Miramos el número de clientes conectados
    int num_conectados = 0;
    for (int i = 0; i < n_elementos; i++){
        if (almacen[i].connected == 1){
            num_conectados++;
        }
    }
    // todo: comentar esto
    if (num_conectados == 0){
        int ret = writeLine(sc_local, "2");
        if (ret == -1) {
            pthread_mutex_unlock(&almacen_mutex);
            writeLine(sc_local, "3");
            return 3;
        }
    }
    // todo: comentar esto
    int ret = writeLine(sc_local, "0");
    if (ret == -1){
        pthread_mutex_unlock(&almacen_mutex);
        writeLine(sc_local, "3");
        return 3;
    }

    // mandar al cliente el número de clientes conectados
    char connected_str[MAX_STR];
    sprintf(connected_str, "%d", num_conectados);
    ret = writeLine(sc_local, connected_str);
    // en caso de error devolveremos 3
    if (ret == -1) {
            pthread_mutex_unlock(&almacen_mutex);
            return 3;
    }

    // mandamos la informacion de cada cliente conectado, en concreto
    // su ip, puerto y nombre
    for (int i = 0; i < n_elementos; i++){
        if (almacen[i].connected == 1){
            char cliente[MAX_SIZE];
            char ip[MAX_SIZE];
            char puerto[MAX_SIZE];
            sprintf(cliente, "%s", almacen[i].cliente);
            sprintf(ip, "%s", almacen[i].ip);
            sprintf(puerto, "%d", almacen[i].puerto);
            // todo: manejo de errores aqui
            int ret = writeLine(sc_local, cliente);
            ret = writeLine(sc_local, ip);
            ret = writeLine(sc_local, puerto);
            // en caso de error devolveremos 3
            if (ret == -1) {
                pthread_mutex_unlock(&almacen_mutex);
                writeLine(sc_local, "3");
                return 3;
            }
        }
    }
    // si todo ha ido bien devolvemos 0
    pthread_mutex_unlock(&almacen_mutex);
    return 0;
}
int s_list_content(int sc_local, operation_log *op_log){
    /* Funcion list_content. Esta funcion lista el contenido publicado de
    un usuario dado. La funcion devuelve:
        - 0: si la operacion es satisfactoria
        - 1: si el usuario no esta registrado
        - 2: si el usuario no esta conectado
        - 3: si el usuario cuya informacion se quiere conocer no existe
        - 4: si hay cualquier error */
    pthread_mutex_lock(&almacen_mutex);
    // recibir el nombre de usuario que hace la operacion
    char operating_user[MAX_STR];
    int ret = readLine(sc_local, operating_user, sizeof(char) * MAX_STR);
    // devolvemos 4 en caso de error
    if (ret < 0) {
        perror("Error en recepcion");
        writeLine(sc_local, "4");
        pthread_mutex_unlock(&almacen_mutex);
        return 4;
    }
    // recibimos el nombre de usuario cuyo contenido queremos conocer
    char username[MAX_STR];
    ret = readLine(sc_local, username, sizeof(char) * MAX_STR);
    // devolvemos 4 en caso de error
    if (ret < 0){
        perror("Error en recepcion");
        writeLine(sc_local, "4");
        pthread_mutex_unlock(&almacen_mutex);
        return 4;
    }
    strcpy(op_log->username, username);
    // comprobar si existe el usuario que manda la operación
    int existe = 1; // valor a devolver en el caso de que no existiese
    int index;  // para poder acceder al usuario que manda la operacion rapidamente
    for (int i = 0; i < n_elementos; i++){
        if (strcmp(almacen[i].cliente, operating_user) == 0){
            existe = 0;
            index = i;
        }
    }
    if (existe > 0){
        // si no existe devolvemos 1
        int ret = writeLine(sc_local, "1");
        // devolvemos 4 en caso de error
        if (ret == -1) {
            writeLine(sc_local, "4");
            pthread_mutex_unlock(&almacen_mutex);
            return 4;
        }
    }
    // comprobar si el usuario que realiza la operacion esta conectado
    // devolveremos 2 si no lo está y 4 en caso de error
    if (almacen[index].connected == 0){
        int ret = writeLine(sc_local, "2");
        if (ret == -1) {
            writeLine(sc_local, "4");
            pthread_mutex_unlock(&almacen_mutex);
            return 4;
        }
    }
    // comprobar si existe el usuario del que queremos saber el contenido
    int existe2 = 3; // valor a devolver en el caso de que no existiese
    int index2; // para poder acceder luego al contenido del usuario buscado
    for (int i = 0; i < n_elementos; i++){
        if (strcmp(almacen[i].cliente, username) == 0){
            existe2 = 0;
            index2 = i;
        }
    }
    if (existe2 > 0){
        int ret = writeLine(sc_local, "3");
        // devolvemos 4 en caso de error
        if (ret == -1){
            writeLine(sc_local, "4");
            pthread_mutex_unlock(&almacen_mutex);
            return 4;
        }
    }
    // todo: comentar esto
    ret = writeLine(sc_local, "0");
        if (ret == -1){
            writeLine(sc_local, "4");
            pthread_mutex_unlock(&almacen_mutex);
            return 4;
        }
    // mandamos primero el número de ficheros del usuario para que el cliente
    // sepa cuantos debe recibir
    int file_count = almacen[index2].file_count; // numero de ficheros del usuario
    char file_count_str[MAX_STR];
    sprintf(file_count_str, "%d", file_count);
    ret = writeLine(sc_local, file_count_str);
    // devolvemos 4 en caso de error
    if (ret == -1){
        writeLine(sc_local, "4");
        pthread_mutex_unlock(&almacen_mutex);
        return 4;
    }
    // enviamos ahora los archivos, es decir, cada nombre y descripcion
    for (int i = 0; i < file_count; i++){
        char name[MAX_SIZE];
        char descr[MAX_SIZE];
        sprintf(name, "%s", almacen[index2].files[i].name);
        sprintf(descr, "\"%s\"", almacen[index2].files[i].descr);
        // todo: comprobacion de errores aqui
        int ret = writeLine(sc_local, name);
        ret = writeLine(sc_local, descr);
        // en caso de error devolveremos 4
        if (ret == -1){
            writeLine(sc_local, "4");
            pthread_mutex_unlock(&almacen_mutex);
            return 4;
        }
    }
    // si todo ha ido bien devolvemos 0
    pthread_mutex_unlock(&almacen_mutex);
    return 0;
}

int s_disconnect(int sc_local, operation_log *op_log){
    /* Funcion disconnect. Esta funcion permite al usuario desconectarse del sistema.
    La funcion devuelve:
        - 0: si la operacion es satisfactoria
        - 1: si el usuario no esta registrado
        - 2: si el usuario no esta conectado
        - 3: en cualquier otro caso de error */
    pthread_mutex_lock(&almacen_mutex);
    // Recibimos el nombre de usuario
    char username[MAX_STR];
    int ret = readLine(sc_local, username, sizeof(char) * MAX_STR);
    // Devolvemos 3 en caso de error en la recepcion
    if (ret < 0){
        perror("Error en recepcion");
        pthread_mutex_unlock(&almacen_mutex);
        return 3;
    }
    strcpy(op_log->username, username);
    // comprobar si existe el usuario
    int existe = 1;         // valor a devolver en el caso de que no existiese
    int index;
    for (int i = 0; i < n_elementos; i++){
        if (strcmp(almacen[i].cliente, username) == 0){
            existe = 0;
            index = i;
        }
    }
    if (existe > 0){
        pthread_mutex_unlock(&almacen_mutex);
        return existe;
    }
    // comprobar si el cliente esta conectado, si no lo esta devolvemos 2
    if (almacen[index].connected == 0){
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }
    // Si esta conectado, lo desconectamos cambiando el valor de connected a 0
    almacen[index].connected = 0;
    pthread_mutex_unlock(&almacen_mutex);
    return 0;
}

int s_get_file(int sc_local, operation_log *op_log){
    /* Funcion get_file. Esta funcion sirve como prerrequisito para que los usuarios puedan transferir archivos
    entre ellos. Esta funcion solo permite al usuario que la llama obtener la ip y puerto del usuario del que quiere
    obtener un fichero y comprobar si el fichero sigue subido en el servidor. La funcion devuelve:
        - 0: si la operacion es satisfactoria
        - 1: si el usuario no esta registrado
        - 2: en cualquier otro caso de error */
    // todo: comentar TODA ESTE FUNCION
    pthread_mutex_lock(&almacen_mutex);
    // obtener el nombre de usuario
    char client_name[MAX_STR];
    int ret = readLine(sc_local, client_name, sizeof(char) * MAX_STR);
    if (ret < 0){
        perror("Error en recepcion");
        writeLine(sc_local, "2");
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }
    strcpy(op_log->username, client_name);
    // obtener el nombre del fichero remoto
    char remote_file[MAX_STR];
    ret = readLine(sc_local, remote_file, sizeof(char) * MAX_STR);
    if (ret < 0){
        perror("Error en recepcion");
        writeLine(sc_local, "2");
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }
    // comprobar que exista el cliente
    int existe = 2;         // valor a devolver en el caso de que no existiese
    int index;
    for (int i = 0; i < n_elementos; i++){
        if (strcmp(almacen[i].cliente, client_name) == 0){
            existe = 0;
            index = i;
        }
    }
    if (existe > 0){
        pthread_mutex_unlock(&almacen_mutex);
        writeLine(sc_local, "2");
        return existe;
    }
    // comprobar que el cliente este conectado
    if (almacen[index].connected == 0){
        writeLine(sc_local, "2");
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }
    // comprobar que el archivo remoto existe
    int existe_archivo = 1;
    int numero_archivos = almacen[index].file_count;
    for (int i = 0; i < numero_archivos; i++){
        if (strcmp(remote_file, almacen[index].files[i].name)==0){
            existe_archivo = 0;
        }
    }
    if (existe_archivo > 0){
        ret = writeLine(sc_local, "1");
        if (ret < 0){
            writeLine(sc_local, "2");
            pthread_mutex_unlock(&almacen_mutex);
            return 2;
        }
        pthread_mutex_unlock(&almacen_mutex);
        return 1;
    }

    // mandar 0
    ret = writeLine(sc_local, "0");
    if (ret < 0){
        writeLine(sc_local, "2");
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }
    // mandar ip y puerto del cliente
    char client_info[MAX_SIZE];
    sprintf(client_info, "%s %d", almacen[index].ip, almacen[index].puerto);
    ret = writeLine(sc_local, client_info);
    if (ret < 0){
        writeLine(sc_local, "2");
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }

    pthread_mutex_unlock(&almacen_mutex);
    return 0;
}


void close_server() {
    /* Funcion para cerrar el servidor */
    // hacer el free del almacen dinamico y salir
    printf("\n Closing server \n");
    write_back();
    free(almacen);
    almacen = NULL;
    closeSocket(sd);
    exit(0);
}

int load() {
    /* Funcion para cargar los datos del almacen estatico al dinamico
    al inicializar el servidor*/
    // obtener directorio
    char cwd[MAX_STR];
    getcwd(cwd, sizeof(cwd));
    // crear directorio si no existe
    strcat(cwd, "/data_structure");
    DIR *dir = opendir(cwd);
    // existe el fichero
    if (dir){
        closedir(dir);
    }
    // no existe
    if (ENOENT == errno){
        mkdir(cwd, 0777);
    }
    // path del archivo
    char file[MAX_STR];
    strcpy(file, cwd);
    strcat(file, "/almacen.txt");
    // abrir descriptor de fichero
    FILE *f = fopen(file, "a+");
    rewind(f);
    // comprobar error al abrir fichero
    if (f == NULL){
        perror("Error en servidor al abrir el fichero");
        return -1;
    }
    // bucle para ir leyendo elementos
    while(fread(&almacen[n_elementos], sizeof(struct tupla), 1, f) == 1){
        // comprobar el tamanio de almacen
        if (n_elementos == max_tuplas-1){
            // duplicar tamanio de almacen
            almacen = realloc(almacen, 2 * max_tuplas * sizeof(struct tupla));
            max_tuplas = max_tuplas * 2;
        }
        n_elementos++;
    }
    fclose(f);
    // eliminar lo que tenemos en el archivo al haberlo pasado al almacen dinamico
    FILE *f_erase = fopen(file, "w");
    // comprobar reescribiendo el fichero
    if (f_erase == NULL){
        perror("Error en servidor al reescribir el fichero");
        return -1;
    }
    fclose(f_erase);
    return 0;
}
int write_back(){
    /* Funcion para volcar los datos del almacen dinamico al fichero estatico
    al cerrar el servidor*/
    // cerrar colas servidor
    char file[MAX_STR];
    getcwd(file, sizeof(file));
    strcat(file, "/data_structure/almacen.txt");
    // abrir descriptor de archivo
    FILE *f = fopen(file, "w");
    // comprobar error al abrir archivo
    if (f == NULL){
        perror("Error en servidor al abrir el fichero");
        return -1;
    }
    // bucle para escribir en archivo
    for (int i=0; i<n_elementos; i++){
        fwrite(&almacen[i], sizeof(struct tupla), 1, f);
    }
    fclose(f);
    return 0;
}

int str2int(char *op){
    // Función para transformar la cadena recibida en un número de operación
    const char *functions[] = {"register", "unregister", "connect", "publish", "delete",
                                "list_users", "list_content", "disconnect", "get_file"};
    for (long unsigned int i= 0; i < sizeof(functions); i++){
        if (strcmp(op, functions[i])==0){
            return i;
        }
    }
    return -1;
}