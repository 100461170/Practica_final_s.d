CC = gcc
CFLAGS = -g -Wall -Werror -lrt -Wextra -pedantic -std=c11 
c_files = $(wildcard *.c)
object_files = $(c_files:.c=.o)
targets =  servidor 
library = libclaves.so
.PHONY: all clean
all: $(targets) 

# este ejecutable es el servidor
servidor: servidor.o communications.o
	@echo "Compiling $@ ..."
	$(CC)  $^ -o $@ 

# enlazar libreria dinamica con objeto clientes
# este ejecutable es un cliente concurrente
cliente: cliente.o $(library) communications.o
	@echo "Compiling $@ with $^"
	$(CC) -o $@ $< -L. -lclaves -Wl,-rpath=.

# compilar el archivo de pruebas secuenciales
prueba_sec: pruebas_secuenciales.o $(library)
	@echo "Compiling $@ with $^"
	$(CC) -o $@ $< -L. -lclaves -Wl,-rpath=.

# compilar el .c a un objeto .o
claves.o: claves.c
	@echo "Creating dynamic library object from: $< ..."
	$(CC) -c -Wall -Werror -fPIC $< -o $@

#compilar .o en una libreria dinamica .so
$(library): claves.o communications.o
	@echo "Creating dynamic library file from: $< ..."
	$(CC) -shared -o $@ $^
# compilar los archivos c a objetos
%.o: %.c
	@echo "Creating object file from: $< ..."
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(object_files) $(targets) $(library)
