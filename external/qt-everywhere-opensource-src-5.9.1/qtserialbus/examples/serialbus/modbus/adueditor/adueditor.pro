TEMPLATE = app
CONFIG += c++11
INCLUDEPATH += .
TARGET = adueditor
QT += serialbus serialport widgets
QT += serialbus-private core-private

FORMS += interface.ui
SOURCES += main.cpp mainwindow.cpp modbustcpclient.cpp
HEADERS += mainwindow.h plaintextedit.h modbustcpclient.h modbustcpclient_p.h

target.path = $$[QT_INSTALL_EXAMPLES]/serialbus/modbus/adueditor
INSTALLS += target
