TEMPLATE = app
TARGET = fdegen
INCLUDEPATH += .

# Input
SOURCES += main.cpp

LIBS += -ldwarf -lelf
