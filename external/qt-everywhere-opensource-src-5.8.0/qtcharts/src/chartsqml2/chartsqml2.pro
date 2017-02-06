
TARGET = qtchartsqml2
CXX_MODULE = charts

QT += qml quick
QT += charts charts-private
contains(QT_COORD_TYPE, float): DEFINES += QT_QREAL_IS_FLOAT

include($$PWD/designer/designer.pri)

SOURCES += \
    chartsqml2_plugin.cpp \
    declarativechart.cpp \
    declarativexypoint.cpp \
    declarativexyseries.cpp \
    declarativelineseries.cpp \
    declarativesplineseries.cpp \
    declarativeareaseries.cpp \
    declarativescatterseries.cpp \
    declarativepieseries.cpp \
    declarativebarseries.cpp \
    declarativecategoryaxis.cpp \
    declarativemargins.cpp \
    declarativeaxes.cpp \
    declarativepolarchart.cpp \
    declarativeboxplotseries.cpp \
    declarativechartnode.cpp \
    declarativecandlestickseries.cpp

HEADERS += \
    declarativechart.h \
    declarativexypoint.h \
    declarativexyseries.h \
    declarativelineseries.h \
    declarativesplineseries.h \
    declarativeareaseries.h \
    declarativescatterseries.h \
    declarativepieseries.h \
    declarativebarseries.h \
    declarativecategoryaxis.h \
    declarativemargins.h \
    declarativeaxes.h \
    declarativepolarchart.h \
    declarativeboxplotseries.h \
    declarativechartnode.h \
    declarativecandlestickseries.h \
    declarativeabstractrendernode.h

contains(QT_CONFIG, opengl) {
    SOURCES += declarativeopenglrendernode.cpp
    HEADERS += declarativeopenglrendernode.h
}


load(qml_plugin)

win32 {
    QMAKE_TARGET_PRODUCT = "Qt Charts (Qt $$QT_VERSION)"
    QMAKE_TARGET_DESCRIPTION = "Charts QML plugin for Qt."
}
