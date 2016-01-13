QT += declarative

SOURCES += main.cpp \
           person.cpp
HEADERS += person.h
RESOURCES += adding.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/declarative/cppextensions/referenceexamples/adding
INSTALLS += target
