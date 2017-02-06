TARGET = QtQuickWidgets

QT = core-private gui-private qml-private quick-private widgets-private

DEFINES   += QT_NO_URL_CAST_FROM_STRING QT_NO_INTEGER_EVENT_COORDINATES QT_NO_FOREACH

HEADERS += \
    qquickwidget.h \
    qquickwidget_p.h \
    qtquickwidgetsglobal.h

SOURCES += \
    qquickwidget.cpp

load(qt_module)
