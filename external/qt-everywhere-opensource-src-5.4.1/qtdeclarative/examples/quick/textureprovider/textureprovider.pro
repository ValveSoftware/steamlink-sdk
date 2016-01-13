TEMPLATE = app

QT += quick qml

HEADERS += etcprovider.h

SOURCES += main.cpp \
           etcprovider.cpp

RESOURCES += textureprovider.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/textureprovider
INSTALLS += target
