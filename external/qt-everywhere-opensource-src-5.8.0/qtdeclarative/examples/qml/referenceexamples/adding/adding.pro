QT = core qml

SOURCES += main.cpp \
           person.cpp
HEADERS += person.h
RESOURCES += adding.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/qml/referenceexamples/adding
INSTALLS += target
