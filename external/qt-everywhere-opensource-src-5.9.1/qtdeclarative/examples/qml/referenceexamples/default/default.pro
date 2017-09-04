QT = core qml

SOURCES += main.cpp \
           person.cpp \
           birthdayparty.cpp
HEADERS += person.h \
           birthdayparty.h
RESOURCES += default.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/qml/referenceexamples/default
INSTALLS += target
