QT       += core

REPC_REPLICA += timemodel.rep
QT += remoteobjects

QT       -= gui

TARGET = CppClient
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += main.cpp

OTHER_FILES += \
    timemodel.rep

target.path = $$[QT_INSTALL_EXAMPLES]/remoteobjects/cppclient
INSTALLS += target
