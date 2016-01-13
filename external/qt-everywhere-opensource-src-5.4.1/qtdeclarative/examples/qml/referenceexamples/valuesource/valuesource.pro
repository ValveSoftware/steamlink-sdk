QT += qml

SOURCES += main.cpp \
           person.cpp \
           birthdayparty.cpp \
           happybirthdaysong.cpp
HEADERS += person.h \
           birthdayparty.h \
           happybirthdaysong.h
RESOURCES += valuesource.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/qml/referenceexamples/valuesource
INSTALLS += target
