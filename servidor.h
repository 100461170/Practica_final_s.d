//
// Created by linux-lex on 29/02/24.
//
#include "communications.h"
#include "operaciones.h"

#ifndef EJERCICIO2_DISTRIBUIDOS_SERVIDOR_H
#define EJERCICIO2_DISTRIBUIDOS_SERVIDOR_H

void *tratar_peticion(void *p);
int s_register(int sc_local, operation_log *op_log);
int s_unregister(int sc_local, operation_log *op_log);
int s_connect(int sc_local, operation_log *op_log);
int s_publish(int sc_local, operation_log *op_log);
int s_delete(int sc_local, operation_log *op_log);
int s_list_users(int sc_local, operation_log *op_log);
int s_list_content(int sc_local, operation_log *op_log);
int s_disconnect(int sc_local, operation_log *op_log);
int s_get_file(int sc_local, operation_log *op_log);
int load();
int write_back();
void close_server();
int str2int(char *function);

#endif //EJERCICIO2_DISTRIBUIDOS_SERVIDOR_H
