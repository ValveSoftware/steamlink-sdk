TEMPLATE = app
QT += qml quick waylandclient-private
CONFIG += wayland-scanner

WAYLANDCLIENTSOURCES += ../protocol/custom.xml

SOURCES += main.cpp \
    ../client-common/customextension.cpp

HEADERS += \
    ../client-common/customextension.h

RESOURCES += qml.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/wayland/custom-extension/qml-client
INSTALLS += target


