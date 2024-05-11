import time
from time import gmtime, strftime
import argparse

from spyne import Application, ServiceBase, Integer, Unicode, rpc, Iterable
from spyne.protocol.soap import Soap11
from spyne.server.wsgi import WsgiApplication



class Time(ServiceBase):

    @rpc(_returns=Iterable(Unicode))
    def get_date_time(ctx):
        time = strftime("%d/%m/%Y %X", gmtime())
        return time


application = Application(
    services=[Time],
    tns='http://tests.python-zeep.org/',
    in_protocol=Soap11(validator='lxml'),
    out_protocol=Soap11())

application = WsgiApplication(application)


    
if __name__ == '__main__':
    
    import logging

    from wsgiref.simple_server import make_server
    
    parser = argparse.ArgumentParser(description='Optional app description')
    parser.add_argument('-p', type=int, required=True, help='Server Port')
    args = parser.parse_args()
    if ((args.p < 1024) or (args.p > 65535)):
        parser.error("Error: Port must be in the range 1024 <= port <= 65535")
        

    logging.basicConfig(level=logging.DEBUG)
    logging.getLogger('spyne.protocol.xml').setLevel(logging.DEBUG)

    logging.info(f"listening to http://127.0.0.1:{args.p}")
    logging.info(f"wsdl is at: http://localhost:{args.p}/?wsdl")

    server = make_server('127.0.0.1', args.p, application)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass

