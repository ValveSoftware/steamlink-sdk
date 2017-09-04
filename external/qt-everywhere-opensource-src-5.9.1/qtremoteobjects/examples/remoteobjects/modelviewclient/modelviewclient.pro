SOURCES = main.cpp

CONFIG   -= app_bundle

# install
target.path = $$[QT_INSTALL_EXAMPLES]/remoteobjects/modelviewclient
INSTALLS += target

QT += widgets remoteobjects
