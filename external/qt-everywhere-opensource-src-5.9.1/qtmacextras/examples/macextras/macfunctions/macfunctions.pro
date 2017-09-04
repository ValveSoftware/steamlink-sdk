TEMPLATE = app
TARGET = macfunctions
DEPENDPATH += .
INCLUDEPATH += .
QT += widgets macextras

# Input
SOURCES += main.cpp

RESOURCES += \
    macfunctions.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/macextras/macfunctions
INSTALLS += target
