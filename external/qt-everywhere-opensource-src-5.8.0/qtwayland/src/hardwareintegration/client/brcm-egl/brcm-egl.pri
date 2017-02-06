INCLUDEPATH += $$PWD

QMAKE_USE += egl wayland-client
QT += egl_support-private

LIBS_PRIVATE += $$QMAKE_LIBS_DYNLOAD

SOURCES += $$PWD/qwaylandbrcmeglintegration.cpp \
           $$PWD/qwaylandbrcmglcontext.cpp \
           $$PWD/qwaylandbrcmeglwindow.cpp

HEADERS += $$PWD/qwaylandbrcmeglintegration.h \
           $$PWD/qwaylandbrcmglcontext.h \
           $$PWD/qwaylandbrcmeglwindow.h

CONFIG += wayland-scanner
WAYLANDCLIENTSOURCES += $$PWD/../../../extensions/brcm.xml
