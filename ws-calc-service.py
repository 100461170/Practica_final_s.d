import time
from time import gmtime, strftime

from spyne import Application, ServiceBase, Integer, Unicode, rpc, Iterable
from spyne.protocol.soap import Soap11
from spyne.server.wsgi import WsgiApplication


class Time(ServiceBase):

    @rpc(_returns=Iterable(Unicode))
    def get_date_time(ctx):
        time = strftime("%d/%m/%Y %X", gmtime())
        return time
    # @rpc(_returns=String)
    # def get_time(ctx):
    #      return a-b

application = Application(
    services=[Time],
    tns='http://tests.python-zeep.org/',
    in_protocol=Soap11(validator='lxml'),
    out_protocol=Soap11())

application = WsgiApplication(application)

if __name__ == '__main__':
    import logging

    from wsgiref.simple_server import make_server

    logging.basicConfig(level=logging.DEBUG)
    logging.getLogger('spyne.protocol.xml').setLevel(logging.DEBUG)

    logging.info("listening to http://127.0.0.1:8000")
    logging.info("wsdl is at: http://localhost:8000/?wsdl")

    server = make_server('127.0.0.1', 8000, application)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass

