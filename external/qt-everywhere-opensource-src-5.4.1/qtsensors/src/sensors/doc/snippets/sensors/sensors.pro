TEMPLATE=app
TARGET=sensorsdocsnippet
QT = core sensors
SOURCES+=main.cpp\
    creating.cpp\
    start.cpp\
    plugin.cpp
HEADERS+=mybackend.h
!win32:*g++*:LIBS+=-rdynamic
OTHER_FILES += *.qml
