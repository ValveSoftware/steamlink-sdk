SOURCES += main.cpp

RESOURCES += \
    clientapp.qrc

QT += remoteobjects quick

contains(QT_CONFIG, c++11): CONFIG += c++11

target.path = $$[QT_INSTALL_EXAMPLES]/remoteobjects/clientapp
INSTALLS += target
