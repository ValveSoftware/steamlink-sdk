TEMPLATE = app
CONFIG+=testcase
TARGET=tst_qgeoareamonitor

SOURCES += tst_qgeoareamonitor.cpp \
           logfilepositionsource.cpp

HEADERS += logfilepositionsource.h

OTHER_FILES += *.txt

QT += positioning testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
