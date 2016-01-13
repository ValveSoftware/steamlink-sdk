TEMPLATE = app

QT += quick qml

SOURCES += main.cpp window.cpp
HEADERS += window.h

RESOURCES += rendercontrol.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/rendercontrol
INSTALLS += target
