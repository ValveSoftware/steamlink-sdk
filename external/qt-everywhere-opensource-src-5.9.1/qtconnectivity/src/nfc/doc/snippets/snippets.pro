TEMPLATE = app
TARGET = nfc_cppsnippet
QT = core
#! [project modification]
QT += nfc
#! [project modification]

SOURCES += main.cpp \
           doc_src_qtnfc.cpp \
           nfc.cpp \
           foorecord.cpp

HEADERS += foorecord.h
