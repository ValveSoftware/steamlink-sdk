QT += qml quick

SOURCES += main.cpp \
           dataobject.cpp
HEADERS += dataobject.h
RESOURCES += objectlistmodel.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/models/objectlistmodel
INSTALLS += target
