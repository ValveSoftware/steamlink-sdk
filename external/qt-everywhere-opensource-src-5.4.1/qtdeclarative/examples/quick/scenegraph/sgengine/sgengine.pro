QT += quick

HEADERS += window.h
SOURCES += window.cpp main.cpp
RESOURCES += \
    sgengine.qrc \
    ../../shared/shared.qrc

# install
target.path = $$[QT_INSTALL_EXAMPLES]/quick/scenegraph/sgengine
INSTALLS += target
