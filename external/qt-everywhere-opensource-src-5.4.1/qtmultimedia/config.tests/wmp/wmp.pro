CONFIG -= qt
CONFIG += console

SOURCES += main.cpp

LIBS += -lstrmiids -lole32 -lOleaut32
!wince*:LIBS += -luser32 -lgdi32
