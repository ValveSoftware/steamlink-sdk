TEMPLATE = lib
CONFIG += plugin
QT += qml quick

DESTDIR = ImageProvider
TARGET  = qmlimageproviderplugin

SOURCES += imageprovider.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/quick/imageprovider/ImageProvider
qml.files = ImageProvider/qmldir
qml.path = $$[QT_INSTALL_EXAMPLES]/quick/imageprovider/ImageProvider
INSTALLS = target qml
