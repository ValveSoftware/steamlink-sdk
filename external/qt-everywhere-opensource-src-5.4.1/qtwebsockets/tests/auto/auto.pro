TEMPLATE = subdirs

SUBDIRS = \
    qwebsocketcorsauthenticator

contains(QT_CONFIG, private_tests): SUBDIRS += \
   websocketprotocol \
   dataprocessor \
   websocketframe \
   handshakerequest \
   handshakeresponse \
   qdefaultmaskgenerator

SUBDIRS += \
    qwebsocket \
    qwebsocketserver
