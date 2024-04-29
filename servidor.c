//
// Created by linux-lex on 29/02/24.
//
#include "servidor.h"
#include "communications.h"
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
    // Inicializamos peticion y variables
    // struct peticion p;
    int contador = 0;
    // Inicializamos los hilos
    pthread_attr_t attr;
    pthread_t thid[MAX];
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    // Inicializamos mutex y variable condicion
    pthread_mutex_init(&sync_mutex, NULL);
    pthread_mutex_init(&almacen_mutex, NULL);
    pthread_cond_init(&sync_cond, NULL);
    // Abrimos socket servidor
    // convertir argumento a numero
    sd = serverSocket(port_number, SOCK_STREAM);
    if (sd < 0) {
        perror("SERVER: Error en serverSocket");
        return 0;
    }
    // impirmir mensaje init
    char machine[MAX];
    int err = gethostname(machine, MAX);
    if (err == -1){
        fprintf(stderr, "Error: no se pudo obtener el nombre del host.\n");
        return -1;
    }
    printf("init server %s:%d\n", machine, port_number);
    // Bucle infinito
    while (1) {
        // aceptar cliente
        sc = serverAccept(sd);
        if (sc < 0) {
            perror("Error en serverAccept");
            return -1;
        }
        
        // crear hilo
        if (pthread_create(&thid[contador], &attr, tratar_peticion, (void *) &sc) != 0) {
            perror("Error en servidor. Pthread_create");
            return -1;
        }
        // Hacemos lock al mutex hasta que se copie la peticion en el hilo
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
    pthread_cond_signal(&sync_cond);
    pthread_mutex_unlock(&sync_mutex);
    char operation[MAX];
    int resp;
    // recibir mensaje de codigo de operacion
    int ret = readLine(sc_local, operation, sizeof(char)*MAX);
    if (ret < 0){
        perror("Error en recepcion");
        closeSocket(sc_local);
        return NULL;
    }
    int num_op = str2int(operation);
    // En funcion de la operacion especificada en la peticion hacemos una u otra operacion
    switch (num_op){
    case 0:
        resp = s_register(sc_local);
        break;
    case 1:
        resp = s_unregister(sc_local);
        break;
    case 2:
        resp = s_connect(sc_local);
        break;
    case 3:
        resp = s_publish(sc_local);
        break;
    case 4:
        resp = s_delete(sc_local);
        break;
    case 5:
        resp = s_list_users(sc_local);
        break;
    // case 6:
    //     resp = s_list_content(int sc_local);
    //     break;
    case 7:
        resp = s_disconnect(sc_local);
        break;
    // case 8:
    //     resp = s_get_file(sc_local);
    //     break;
    default:
        resp = -1;
        break;
    }

    // devolvemos la respuesta si no es list users o list connect
    if (num_op != 5 || num_op != 6){
        char resp_str[2];
    sprintf(resp_str, "%d", resp);
    ret = writeLine(sc_local, resp_str);
    if (ret == -1) {
        perror("Error en envio");
        closeSocket(sc_local);
        exit(-1);
    }
    }
    
    
    closeSocket(sc_local);
    return NULL;
}

int s_register(int sc_local){
    pthread_mutex_lock(&almacen_mutex);
    // recibir el usuario
    char username[MAX];
    int ret = readLine(sc_local, username, sizeof(char) * MAX);
    if (ret < 0){
        perror("Error en recepcion");
        closeSocket(sc_local);
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }
    printf("register from %s\n", username);
    // bucle para saber si usuario esta registrado
    for (int i = 0; i < n_elementos; i++){
        if (strcmp(almacen->cliente, username) == 0){
            pthread_mutex_unlock(&almacen_mutex);
            return 1;
        }
    }
    // comprobar el tamanio de almacen
    if (n_elementos == max_tuplas){
        // duplicar tamanio de almacen
        almacen = realloc(almacen, 2 * max_tuplas * sizeof(struct tupla));
        max_tuplas = max_tuplas * 2;
    }
    // si no esta registrado se lo registra
    struct tupla insert;
    memset(&insert, 0, sizeof(struct tupla));
    strcpy(insert.cliente, username);
    almacen[n_elementos] = insert;
    n_elementos++;
    
    pthread_mutex_unlock(&almacen_mutex);
    return 0;
}

int s_unregister(int sc_local){
    pthread_mutex_lock(&almacen_mutex);
    // recibir el usuario
    char username[MAX];
    int ret = readLine(sc_local, username, sizeof(char) * MAX);
    if (ret < 0){
        perror("Error en recepcion");
        closeSocket(sc_local);
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }
    printf("unregister from %s\n", username);
    // bucle para saber si usuario esta registrado
    for (int i = 0; i < n_elementos; i++){
        if (strcmp(almacen[i].cliente, username) == 0){
                        // esto funciona si el orden de las tuplas no importa. Sino hay que cambiarlo
            // copiar ultimo elemento del almacen al indice
            almacen[i] = almacen[n_elementos-1];
            // borrar ultimo elemento del almacen
            struct tupla tupla_vacia;
            memset(&tupla_vacia, 0, sizeof(struct tupla));
            almacen[n_elementos-1] = tupla_vacia;
            // bajar el numero de elementos
            n_elementos--;
            pthread_mutex_unlock(&almacen_mutex);
            return 0;
        }
    }    
    pthread_mutex_unlock(&almacen_mutex);
    return 1;
}
int s_connect(int sc_local){
    pthread_mutex_lock(&almacen_mutex);
    // obtener nombre de usuario
    char username[MAX];
    int ret = readLine(sc_local, username, sizeof(char) * MAX);
    if (ret < 0){
        perror("Error en recepcion");
        closeSocket(sc_local);
        pthread_mutex_unlock(&almacen_mutex);
        return 3;
    }
    // obtener puerto
    char puerto_str[MAX];
    ret = readLine(sc_local, puerto_str, sizeof(char) * MAX);
    if (ret < 0){
        perror("Error en recepcion");
        closeSocket(sc_local);
        pthread_mutex_unlock(&almacen_mutex);
        return 3;
    }
    int port_number = strtol(puerto_str, NULL, 10);
    // obtener nombre de usuario
    char ip[MAX];
    ret = readLine(sc_local, ip, sizeof(char) * MAX);
    if (ret < 0){
        perror("Error en recepcion");
        closeSocket(sc_local);
        pthread_mutex_unlock(&almacen_mutex);
        return 3;
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
    // comporbar si el cliente ya esta conectado
    if (almacen[index].conected > 0){
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }
    // copiar valores
    almacen[index].conected = 1;
    almacen[index].puerto = port_number;
    strcpy(almacen[index].ip, ip);
    pthread_mutex_unlock(&almacen_mutex);
    return 0;
}

int s_publish(int sc_local){
    pthread_mutex_lock(&almacen_mutex);
    // obtener nombre de usuario
    char username[MAX];
    int ret = readLine(sc_local, username, sizeof(char) * MAX);
    if (ret < 0){
        perror("Error en recepcion");
        closeSocket(sc_local);
        pthread_mutex_unlock(&almacen_mutex);
        return 4;
    }
    // obtener nombre archivo
    char filename[MAX];
    ret = readLine(sc_local, filename, sizeof(char) * MAX);
    if (ret < 0){
        perror("Error en recepcion");
        closeSocket(sc_local);
        pthread_mutex_unlock(&almacen_mutex);
        return 4;
    }
    // obtener descripcion del archivo
    char description[MAX];
    ret = readLine(sc_local, description, sizeof(char) * MAX);
    if (ret < 0){
        perror("Error en recepcion");
        closeSocket(sc_local);
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
    // comprobar si el cliente no esta conectado
    if (almacen[index].conected == 0){
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }
    int file_counter = almacen[index].file_count;
    // comprobar si el contenido ya ha sido publicado
    for (int i = 0; i < file_counter; i++){
        if (strcmp(almacen[index].files[i].name, filename) == 0){
            pthread_mutex_unlock(&almacen_mutex);
            return 3;
        }
    }
    // insertar el contenido
    strcpy(almacen[index].files[file_counter].name, filename);
    strcpy(almacen[index].files[file_counter].descr, description);
    almacen[index].file_count++;
    pthread_mutex_unlock(&almacen_mutex);
    return 0;
}
int s_delete(int sc_local){
    pthread_mutex_lock(&almacen_mutex);
    // obtener nombre de usuario
    char username[MAX];
    int ret = readLine(sc_local, username, sizeof(char) * MAX);
    if (ret < 0){
        perror("Error en recepcion");
        closeSocket(sc_local);
        pthread_mutex_unlock(&almacen_mutex);
        return 4;
    }
    // obtener nombre archivo
    char filename[MAX];
    ret = readLine(sc_local, filename, sizeof(char) * MAX);
    if (ret < 0){
        perror("Error en recepcion");
        closeSocket(sc_local);
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
    // comprobar si el cliente no esta conectado
    if (almacen[index].conected == 0){
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }
    // comprobar si el contenido no ha sido publicado
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
    // borrar fichero
    struct file file_vacio;
    memset(&file_vacio, 0, sizeof(struct file));
    almacen[index].files[indice_fichero] = almacen[index].files[file_counter-1];
    almacen[index].files[file_counter-1] = file_vacio;
    almacen[index].file_count--;
    pthread_mutex_unlock(&almacen_mutex);
    return 0;
}
int s_list_users(int sc_local){
    pthread_mutex_lock(&almacen_mutex);
    // comprobar si hay clientes
    int hay_clientes = 0;
    for (int i = 0; i < n_elementos; i++){
        if (strcmp(almacen[i].cliente, "") != 0){
            hay_clientes = 1;
        }
    }
    if (hay_clientes == 0){
        int ret = writeLine(sc_local, "1\0");
        if (ret == -1) {
            pthread_mutex_unlock(&almacen_mutex);
            return 3;
        }
    }
    // sacar el numero de clientes conectados
    int num_conectados = 0;
    for (int i = 0; i < n_elementos; i++){
        if (almacen[i].conected == 1){
            num_conectados++;
        }
    }
    // si no hay ninguno se manda un dos
    if (num_conectados == 0){
        int ret = writeLine(sc_local, "2\0");
        if (ret == -1) {
            pthread_mutex_unlock(&almacen_mutex);
            return 3;
        }
    }
    // mandar los clientes conectados
    // mandar 0
    int ret = writeLine(sc_local, "0\0");
    if (ret == -1){
        pthread_mutex_unlock(&almacen_mutex);
        return 3;
    }
    // mandar numero de clientes
    char connected_str[MAX];
    sprintf(connected_str, "%d", num_conectados); 
    ret = writeLine(sc_local, connected_str);
        if (ret == -1) {
            pthread_mutex_unlock(&almacen_mutex);
            return 3;
    }
    // mandar info de cada cliente
    for (int i = 0; i < n_elementos; i++){
        if (almacen[i].conected == 1){
            char envio[MAX_SIZE];
            sprintf(envio, "%s %s %d\n", almacen[i].cliente, almacen[i].ip, almacen[i].puerto);
            int ret = writeLine(sc_local, envio);
            if (ret == -1) {
                pthread_mutex_unlock(&almacen_mutex);
                return 3;
            }
        }
    }
    pthread_mutex_unlock(&almacen_mutex);
    return 0;
}

int s_disconnect(int sc_local){
    pthread_mutex_lock(&almacen_mutex);
    char username[MAX];
    int ret = readLine(sc_local, username, sizeof(char) * MAX);
    if (ret < 0){
        perror("Error en recepcion");
        closeSocket(sc_local);
        pthread_mutex_unlock(&almacen_mutex);
        return 3;
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
    // comprobar si el cliente no esta conectado
    if (almacen[index].conected == 0){
        pthread_mutex_unlock(&almacen_mutex);
        return 2;
    }
    almacen[index].conected = 0;
    pthread_mutex_unlock(&almacen_mutex);
    return 0;
}

        /* int s_init() {
            // mutex lock
            pthread_mutex_lock(&almacen_mutex);
            free(almacen);
            almacen = NULL;
            almacen = (struct tupla *)malloc(max_tuplas * sizeof(struct tupla));
            // poner a 0 todos los elementos del almacen
            size_t elementos = max_tuplas * sizeof(struct tupla);
            memset(almacen, 0, elementos);
            pthread_mutex_unlock(&almacen_mutex);
            return 0;
        }

        int s_set_value(int sc_local){
            // bloquear el mutex
            pthread_mutex_lock(&almacen_mutex);
            // conseguir elementos
            int ret;
            char c_key[MAX];
            char valor1[MAX];
            char c_valor2_N[MAX];
            char c_valor2_value[MAX_SIZE];
            // recibir key
            ret = recvMessage(sc_local, c_key, sizeof(char) * MAX);
            if (ret == -1) {
                perror("Error en recibo");
                pthread_mutex_unlock(&almacen_mutex);
                return -1;
            }

            // recibir value1
            ret = recvMessage(sc_local, valor1, sizeof(char) * MAX);
            if (ret == -1) {
                perror("Error en recibo");
                pthread_mutex_unlock(&almacen_mutex);
                return -1;
            }
            // recibir N_value2
            ret = recvMessage(sc_local, c_valor2_N, sizeof(char) * MAX);
            if (ret == -1) {
                perror("Error en recibo");
                pthread_mutex_unlock(&almacen_mutex);
                return -1;
            }
            // convertir tipos
            // key
            char *ending_char;
            int key = strtol(c_key, &ending_char, 10);
            // N_value2
            int valor2_N = strtol(c_valor2_N, &ending_char, 10);
            // recibir V_value2
            double valor2_value[MAX_SIZE];
            for (int i = 0; i < valor2_N; i++){
                ret = recvMessage(sc_local, c_valor2_value, sizeof(char) * MAX);
                if (ret == -1){
                    perror("Error en recibo");
                    pthread_mutex_unlock(&almacen_mutex);
                    return -1;
                }
                valor2_value[i] = strtold(c_valor2_value, &ending_char);
            }

            // comprobar la existencia de key
            // iterar por el almacen
            for (int i = 0; i < n_elementos; i++){
                if (almacen[i].clave == key){
                    fprintf(stderr, "Error: Ya existe la key en el almacen. \n");
                    pthread_mutex_unlock(&almacen_mutex);
                    return -1;
                }
            }
            // comprobar el tamanio de almacen
            if (n_elementos == max_tuplas){
                // duplicar tamanio de almacen
                almacen = realloc(almacen, 2 * max_tuplas * sizeof(struct tupla));
                max_tuplas = max_tuplas * 2;
            }
            // crear tupla de insercion
            struct tupla insertar;
            insertar.clave = key;
            insertar.valor2_N = valor2_N;
            strcpy(insertar.valor1, valor1);
            // copiar vector
            for (int i = 0; i < valor2_N; i++) {
                insertar.valor2_value[i] = valor2_value[i];
            }
            // agregar a almacen

            almacen[n_elementos] = insertar;
            n_elementos++;
            // desbloquear mutex
            pthread_mutex_unlock(&almacen_mutex);
            // devolver valor
            return 0;
        }

        int s_get_value(int sc_local){
            // iterar por el almacen
            pthread_mutex_lock(&almacen_mutex);

            // conseguir elementos
            int ret;
            char c_key[MAX];
            char valor1[MAX];
            int valor2_N;
            double valor2_value[MAX_SIZE];
            // recibir key
            ret = recvMessage(sc_local, c_key, sizeof(char) * MAX);
            if (ret == -1) {
                perror("Error en recibo");
                pthread_mutex_unlock(&almacen_mutex);
                return -1;
            }
            // convertir tipos
            // key
            char *ending_char;
            int key = strtol(c_key, &ending_char, 10);
            // iterar por almacen
            int existe = -1;
            for (int i = 0; i < n_elementos; i++){
                if (almacen[i].clave == key){
                    existe = 0;
                    // copiar informacion
                    strcpy(valor1, almacen[i].valor1);
                        valor2_N = almacen[i].valor2_N;
                    for (int j = 0; j<almacen[i].valor2_N; j++){
                        valor2_value[j] = almacen[i].valor2_value[j];
                    }
                }
            }
            char resp_str[MAX];
            sprintf(resp_str, "%d", existe);
            ret = sendMessage(sc_local, resp_str, sizeof(resp_str));
            if (ret == -1) {
                perror("Error en envio");
                pthread_mutex_unlock(&almacen_mutex);
                return -1;
            }
            if (existe != -1){
                // enviar elementos
                // mandar value1
                ret = sendMessage(sc_local, valor1, sizeof(valor1));
                if (ret == -1) {
                    perror("Error en envio");
                    pthread_mutex_unlock(&almacen_mutex);
                    return -1;
                }
                // mandar N_value2
                char c_N_value2[MAX];
                sprintf(c_N_value2, "%d", valor2_N);
                ret = sendMessage(sc_local, c_N_value2, sizeof(c_N_value2));
                if (ret == -1) {
                    perror("Error en envio");
                    pthread_mutex_unlock(&almacen_mutex);
                    return -1;
                }
                // mandar V_value2
                for (int i = 0; i< valor2_N; i++){
                    char vector_string[MAX];
                    snprintf(vector_string, MAX, "%f", valor2_value[i]);
                    ret = sendMessage(sc_local, vector_string, sizeof(vector_string));
                    if (ret == -1){
                        perror("Error en envio");
                        pthread_mutex_unlock(&almacen_mutex);
                        return -1;
                    }
                }
            }


            pthread_mutex_unlock(&almacen_mutex);
            // devolver valor
            return existe;
        }

        int s_modify_value(int sc_local){
            pthread_mutex_lock(&almacen_mutex);
            // conseguir elementos
            int ret;
            char c_key[MAX];
            char valor1[MAX];
            char c_valor2_N[MAX];
            char c_valor2_value[MAX_SIZE];
            // recibir key
            ret = recvMessage(sc_local, c_key, sizeof(char) * MAX);
            if (ret == -1) {
                perror("Error en recibo");
                pthread_mutex_unlock(&almacen_mutex);
                return -1;
            }

            // recibir value1
            ret = recvMessage(sc_local, valor1, sizeof(char) * MAX);
            if (ret == -1) {
                perror("Error en recibo");
                pthread_mutex_unlock(&almacen_mutex);
                return -1;
            }
            // recibir N_value2
            ret = recvMessage(sc_local, c_valor2_N, sizeof(char));
            if (ret == -1) {
                perror("Error en recibo");
                pthread_mutex_unlock(&almacen_mutex);
                return -1;
            }
            // convertir tipos
            // key
            char *ending_char;
            int key = strtol(c_key, &ending_char, 10);
            // N_value2
            int valor2_N = strtol(c_valor2_N, &ending_char, 10);
            // recibir V_value2
            double valor2_value[MAX_SIZE];
            for (int i = 0; i < valor2_N; i++){
                ret = recvMessage(sc_local, c_valor2_value, sizeof(char) * MAX);
                if (ret == -1){
                    perror("Error en recibo");
                    pthread_mutex_unlock(&almacen_mutex);
                    return -1;
                }
                valor2_value[i] = strtold(c_valor2_value, &ending_char);
            }
            // iterar por almacen
            int existe = -1;
            for (int i = 0; i < n_elementos; i++){
                if (almacen[i].clave == key){
                    existe = 0;
                    // modificar valores
                    strcpy(almacen[i].valor1, valor1);
                    almacen[i].valor2_N = valor2_N;
                    // igualar el vector a 0
                    memset(almacen[i].valor2_value, 0, 32*sizeof(double));
                    // copiar parametro a vector
                    for (int j = 0; j<valor2_N; j++){
                        almacen[i].valor2_value[j] = valor2_value[j];
                    }
                }
            }
            pthread_mutex_unlock(&almacen_mutex);
            return existe;
        }

        int s_delete_key(int sc_local) {
            pthread_mutex_lock(&almacen_mutex);
            // conseguir elementos
            int ret;
            char c_key[MAX];
            // recibir key
            ret = recvMessage(sc_local, c_key, sizeof(char) * MAX);
            if (ret == -1) {
                perror("Error en recibo");
                pthread_mutex_unlock(&almacen_mutex);
                return -1;
            }
            // convertir tipos
            // key
            char *ending_char;
            int key = strtol(c_key, &ending_char, 10);
            int existe = -1;
            // iterar por el almacen
            for (int i = 0; i < n_elementos; i++){
                if (almacen[i].clave == key){
                    existe = 0;
                    // esto funciona si el orden de las tuplas no importa. Sino hay que cambiarlo
                    // copiar ultimo elemento del almacen al indice
                    almacen[i] = almacen[n_elementos-1];
                    // borrar ultimo elemento del almacen
                    struct tupla tupla_vacia;
                    memset(&tupla_vacia, 0, sizeof(struct tupla));
                    almacen[n_elementos-1] = tupla_vacia;
                    // bajar el numero de elementos
                    n_elementos--;
                }
            }

            pthread_mutex_unlock(&almacen_mutex);
            return existe;
        }

        int s_exist(int sc_local) {
            pthread_mutex_lock(&almacen_mutex);
            // conseguir elementos
            int ret;
            char c_key[MAX];
            // recibir key
            ret = recvMessage(sc_local, c_key, sizeof(char) * MAX);
            if (ret == -1) {
                perror("Error en recibo");
                pthread_mutex_unlock(&almacen_mutex);
                return -1;
            }
            // convertir tipos
            // key
            char *ending_char;
            int key = strtol(c_key, &ending_char, 10);
            int existe = 0;
            // iterar por el almacen
            for (int i = 0; i<n_elementos; i++){
                if (almacen[i].clave == key){
                    existe = 1;
                }
            }
            pthread_mutex_unlock(&almacen_mutex);
            // devolver existencia
            return existe;
        } */

        void close_server()
        {
            // hacer el free y salir
            printf("\n Closing server \n");
            write_back();
            free(almacen);
            almacen = NULL;
            closeSocket(sd);
            exit(0);
        }

        int load()
        {
            // obtener directorio
            char cwd[MAX];
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
    char file[MAX];
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
    // cerrar colas servidor
    char file[MAX];
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
    const char *functions[] = {"register", "unregister", "connect", "publish", "delete",
                                "list_users", "list_connect", "disconnect", "get_file"};
    for (long unsigned int i= 0; i < sizeof(functions); i++){
        if (strcmp(op, functions[i])==0){
            return i;
        }
    }
    return -1;
}