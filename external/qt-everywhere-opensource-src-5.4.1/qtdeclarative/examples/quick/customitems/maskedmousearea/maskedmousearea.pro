TEMPLATE = app

QT += quick qml

HEADERS += maskedmousearea.h

SOURCES += main.cpp \
           maskedmousearea.cpp

RESOURCES += maskedmousearea.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/customitems/maskedmousearea
INSTALLS += target
