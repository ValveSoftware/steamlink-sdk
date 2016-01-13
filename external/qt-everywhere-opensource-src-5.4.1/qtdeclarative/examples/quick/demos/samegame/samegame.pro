TEMPLATE = app

QT += qml quick sql
SOURCES += main.cpp
RESOURCES += samegame.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/demos/samegame
INSTALLS += target

QTPLUGIN += qsqlite
