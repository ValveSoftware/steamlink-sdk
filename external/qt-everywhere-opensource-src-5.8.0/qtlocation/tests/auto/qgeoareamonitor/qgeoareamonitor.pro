TEMPLATE = app
CONFIG+=testcase
TARGET=tst_qgeoareamonitor

SOURCES += tst_qgeoareamonitor.cpp \
           logfilepositionsource.cpp

HEADERS += logfilepositionsource.h

OTHER_FILES += *.txt

CONFIG -= app_bundle

QT += positioning testlib
