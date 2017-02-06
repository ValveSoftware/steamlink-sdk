QT += qml quick widgets

SOURCES += main.cpp \
    splineeditor.cpp \
    mainwindow.cpp \
    segmentproperties.cpp

RESOURCES = $$PWD/resources.qrc

HEADERS += \
    splineeditor.h \
    mainwindow.h \
    segmentproperties.h

FORMS += \
    properties.ui \
    pane.ui \
    import.ui

load(qt_app)
