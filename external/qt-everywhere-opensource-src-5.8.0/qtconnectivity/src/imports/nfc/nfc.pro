TARGETPATH = QtNfc

QT = core qml nfc

# Input
HEADERS += \
    #qdeclarativenearfieldsocket_p.h \
    qdeclarativenearfield_p.h \
    qdeclarativendeffilter_p.h \
    qdeclarativendeftextrecord_p.h \
    qdeclarativendefurirecord_p.h \
    qdeclarativendefmimerecord_p.h

SOURCES += plugin.cpp \
    #qdeclarativenearfieldsocket.cpp \
    qdeclarativenearfield.cpp \
    qdeclarativendeffilter.cpp \
    qdeclarativendeftextrecord.cpp \
    qdeclarativendefurirecord.cpp \
    qdeclarativendefmimerecord.cpp

load(qml_plugin)
