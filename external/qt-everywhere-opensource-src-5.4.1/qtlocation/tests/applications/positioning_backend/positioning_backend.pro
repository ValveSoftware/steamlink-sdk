QT       += core gui positioning widgets

TARGET = posbackendtesting
TEMPLATE = app


SOURCES += main.cpp\
        widget.cpp

HEADERS  += widget.h

FORMS    += widget.ui

winrt: WINRT_MANIFEST.capabilities_device += location
