QT += widgets declarative

SOURCES += main.cpp \
           lineedit.cpp
HEADERS += lineedit.h
RESOURCES += extended.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/declarative/cppextensions/referenceexamples/extended
INSTALLS += target
