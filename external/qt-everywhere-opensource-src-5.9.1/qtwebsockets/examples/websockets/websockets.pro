TEMPLATE = subdirs
QT_FOR_CONFIG += network

SUBDIRS = echoclient \
          echoserver \
          simplechat

qtHaveModule(quick) {
SUBDIRS += qmlwebsocketclient \
           qmlwebsocketserver
}

qtConfig(ssl) {
SUBDIRS +=  \
            sslechoserver \
            sslechoclient
}
