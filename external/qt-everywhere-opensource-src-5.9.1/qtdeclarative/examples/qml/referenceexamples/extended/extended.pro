QT += qml widgets

SOURCES += main.cpp \
           lineedit.cpp
HEADERS += lineedit.h
RESOURCES += extended.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/qml/referenceexamples/extended
INSTALLS += target
