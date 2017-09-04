TEMPLATE=app
TARGET=sensorgesturessdocsnippet
QT = core sensors
SOURCES+=main.cpp\
    creating.cpp\
    plugin.cpp
HEADERS+=myplugin.h
!win32:*g++*:LIBS+=-rdynamic
