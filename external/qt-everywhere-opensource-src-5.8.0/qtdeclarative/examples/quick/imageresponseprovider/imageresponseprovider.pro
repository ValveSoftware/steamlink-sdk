TEMPLATE = lib
CONFIG += plugin
QT += qml quick

DESTDIR = ImageResponseProviderCore
TARGET  = qmlimageresponseproviderplugin

SOURCES += imageresponseprovider.cpp

EXAMPLE_FILES = imageresponseprovider-example.qml

target.path = $$[QT_INSTALL_EXAMPLES]/quick/imageresponseprovider/ImageResponseProviderCore
qml.files = ImageResponseProviderCore/qmldir
qml.path = $$[QT_INSTALL_EXAMPLES]/quick/imageresponseprovider/ImageResponseProviderCore
INSTALLS = target qml

CONFIG += install_ok  # Do not cargo-cult this!
