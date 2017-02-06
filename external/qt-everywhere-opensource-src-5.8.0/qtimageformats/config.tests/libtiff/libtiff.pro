SOURCES = libtiff.cpp
CONFIG -= qt dylib
mac:CONFIG -= app_bundle
win32:CONFIG += console
unix|mingw: LIBS += -ltiff
else:win32: LIBS += libtiff.lib
