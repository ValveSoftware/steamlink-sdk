SOURCES = libmng.cpp
CONFIG -= qt dylib
mac:CONFIG -= app_bundle
win32:CONFIG += console
unix|mingw: LIBS += -lmng
else:win32: LIBS += libmng.lib
