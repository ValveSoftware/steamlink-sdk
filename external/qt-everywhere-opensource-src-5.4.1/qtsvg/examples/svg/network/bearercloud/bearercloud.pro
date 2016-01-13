HEADERS = bearercloud.h \
          cloud.h

SOURCES = main.cpp \
          bearercloud.cpp \
          cloud.cpp

RESOURCES = icons.qrc

TARGET = bearercloud

QT = core gui widgets network svg

CONFIG += console

# install
target.path = $$[QT_INSTALL_EXAMPLES]/svg/network/bearercloud
INSTALLS += target
