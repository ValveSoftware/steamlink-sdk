SOURCES = libwebp.cpp
CONFIG -= qt dylib
mac:CONFIG -= app_bundle
win32:CONFIG += console
unix|mingw: LIBS += -lwebp
else:win32: LIBS += libwebp.lib
