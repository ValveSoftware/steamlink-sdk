TEMPLATE = lib
CONFIG += plugin
QT += declarative

DESTDIR = ImageProviderCore
TARGET  = qmlimageproviderplugin

SOURCES += imageprovider.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/declarative/cppextensions/imageprovider/ImageProviderCore
qml.files = imageprovider-example.qml
qml.path = $$[QT_INSTALL_EXAMPLES]/declarative/cppextensions/imageprovider
qml2.files = ImageProviderCore/qmldir
qml2.path = $$[QT_INSTALL_EXAMPLES]/declarative/cppextensions/imageprovider/ImageProviderCore

INSTALLS = qml qml2 target

