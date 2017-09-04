INCLUDEPATH += $$PWD

QMAKE_USE += egl wayland-client libdl
QT += egl_support-private

SOURCES += $$PWD/qwaylandbrcmeglintegration.cpp \
           $$PWD/qwaylandbrcmglcontext.cpp \
           $$PWD/qwaylandbrcmeglwindow.cpp

HEADERS += $$PWD/qwaylandbrcmeglintegration.h \
           $$PWD/qwaylandbrcmglcontext.h \
           $$PWD/qwaylandbrcmeglwindow.h

CONFIG += wayland-scanner
WAYLANDCLIENTSOURCES += $$PWD/../../../extensions/brcm.xml
