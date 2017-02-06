TEMPLATE = app

QT += qml scxml
CONFIG += c++11

SOURCES += mediaplayer-qml-cppdatamodel.cpp \
    thedatamodel.cpp

HEADERS += thedatamodel.h

RESOURCES += mediaplayer-qml-cppdatamodel.qrc

STATECHARTS = mediaplayer-cppdatamodel.scxml

target.path = $$[QT_INSTALL_EXAMPLES]/scxml/mediaplayer-qml-cppdatamodel
INSTALLS += target

