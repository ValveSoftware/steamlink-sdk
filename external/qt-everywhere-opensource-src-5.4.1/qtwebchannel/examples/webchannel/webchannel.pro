TEMPLATE = subdirs
qtHaveModule(websockets) {
    SUBDIRS += chatserver-cpp \
               chatclient-qml
    qtHaveModule(widgets) {
        SUBDIRS += standalone
    }
}

SUBDIRS += nodejs \
           chatclient-html
