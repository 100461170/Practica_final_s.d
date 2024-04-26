//
// Created by linux-lex on 29/02/24.
//
#include "communications.h"

#ifndef EJERCICIO2_DISTRIBUIDOS_SERVIDOR_H
#define EJERCICIO2_DISTRIBUIDOS_SERVIDOR_H

void * tratar_peticion(void *p);
int s_init();
int s_set_value(int sc_local);
int s_get_value(int sc_local);
int s_modify_value(int sc_local);
int s_delete_key(int sc_local);
int s_exist(int sc_local);
int load();
int write_back();
void close_server();

#endif //EJERCICIO2_DISTRIBUIDOS_SERVIDOR_H
