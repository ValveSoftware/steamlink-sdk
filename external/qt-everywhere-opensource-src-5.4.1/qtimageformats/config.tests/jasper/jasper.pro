SOURCES = jasper.cpp
CONFIG -= qt dylib
mac:CONFIG -= app_bundle
win32:CONFIG += console
msvc: LIBS += libjasper.lib
else: LIBS += -ljasper
