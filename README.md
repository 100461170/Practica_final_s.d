Compilación y ejecución del sistema
    Pasos previos en Python
        Antes de ejecutar se debe seguir estos pasos para que la parte relativa a python funcione:
            -   Hacer: cd $ cd ssdd_p2_100461170_100472173
            -   Crear un “virtual environment” llamado venv usando el comando: $ python3 -m venv venv
            -   Activar el venv usando el comando: $ source venv/bin/activate
            -   Instalar todos los requisitos del sistema usando: $ pip install -r requirements.txt
        Nota:  Los pasos 1 y 2 (crear un virtual environment) son opcionales, pero recomendables

    Guía de compilación y ejecución del sistema
    Una vez que se hayan realizado los pasos previos se puede seguir con estos pasos para ejecutar una sesión. Esta es la forma más cómoda de ejecutar el servicio.
        -   Abrir cuatro terminales
        -   En todas las terminales hacer cd $ cd ssdd_p2_100461170_100472173

        -   La primera terminal tendrá el servidor del cliente. Se deber usar: 
            -   $ make
            -   $ ./execute_server.sh
            Esto es idéntico a utilizar:  $ export IP_TUPLAS=localhost; ./servidor_cliente -p 3000

        -   En la segunda terminal estará el servidor RPC. El comando es:
            -   $ ./servidor_rpc

        -   En la tercera terminal estará el servicio web. Los comandos son: 
            -   $ source venv/bin/activate 
            -   $ python3 web_service.py -p 8000

        -   En la cuarta terminal estará el cliente en python. Los comandos son:
            -   $ source venv/bin/activate 
            -   $ python3 client.py -s localhost -p 3000 -ws 8000
