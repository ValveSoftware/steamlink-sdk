TEMPLATE = app
TARGET = screenshots
QT += quick widgets

DEFINES += SNIPPETS_DIR=\\\"$$PWD/../../../src/imports/controls/doc/snippets\\\"
message($$DEFINES)

SOURCES += \
    screenshots.cpp

RESOURCES += \
    screenshots.qml \
    qtquickcontrols2.conf
