TEMPLATE = app
QT += qml quick network

SOURCES += main.cpp
RESOURCES += networkaccessmanagerfactory.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/qml/networkaccessmanagerfactory
INSTALLS = target
