QT += widgets script
RESOURCES += defaultprototypes.qrc
SOURCES += main.cpp prototypes.cpp
HEADERS += prototypes.h

target.path = $$[QT_INSTALL_EXAMPLES]/script/defaultprototypes
INSTALLS += target

maemo5: CONFIG += qt_example
