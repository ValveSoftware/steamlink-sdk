TEMPLATE = subdirs

SUBDIRS = \
    qwebsocketcorsauthenticator

qtConfig(private_tests): SUBDIRS += \
   websocketprotocol \
   dataprocessor \
   websocketframe \
   handshakerequest \
   handshakeresponse \
   qdefaultmaskgenerator

SUBDIRS += \
    qwebsocket \
    qwebsocketserver
