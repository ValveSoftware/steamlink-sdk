CONFIG -= qt
CONFIG += link_pkgconfig

PKGCONFIG += \
        libpulse \
        libpulse-mainloop-glib

SOURCES = pulseaudio.cpp
