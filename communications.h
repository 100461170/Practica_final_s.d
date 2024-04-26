//
// Created by linux-lex on 1/03/24.
//

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#ifndef EJERCICIO2_DISTRIBUIDOS_STRUCTURES_H
#define EJERCICIO2_DISTRIBUIDOS_STRUCTURES_H



#define MAX 256
#define MAX_VECTOR 32

//structures used 


struct tupla {
    int clave;
    char valor1[MAX];
    int valor2_N;
    double valor2_value[MAX_VECTOR];
};

int serverSocket(int port, int type);
int serverAccept(int sd);
int clientSocket(char *remote, int port);
int closeSocket(int sd);

int sendMessage(int socket, char *buffer, int len);
int recvMessage(int socket, char *buffer, int len);

ssize_t writeLine(int fd, char *buffer);
ssize_t readLine(int fd, char *buffer, size_t n);

#endif //EJERCICIO2_DISTRIBUIDOS_STRUCTURES_H