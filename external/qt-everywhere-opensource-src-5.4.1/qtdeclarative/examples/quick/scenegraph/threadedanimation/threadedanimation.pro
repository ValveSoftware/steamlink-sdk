QT += qml quick

HEADERS += spinner.h
SOURCES += spinner.cpp main.cpp
RESOURCES += threadedanimation.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/scenegraph/threadedanimation
INSTALLS += target
