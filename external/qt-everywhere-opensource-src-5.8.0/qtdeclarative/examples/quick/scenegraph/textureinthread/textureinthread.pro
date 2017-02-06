QT += quick

# To make threaded gl check...
QT += core-private gui-private

HEADERS += threadrenderer.h
SOURCES += threadrenderer.cpp main.cpp

INCLUDEPATH += ../shared
HEADERS += ../shared/logorenderer.h
SOURCES += ../shared/logorenderer.cpp

RESOURCES += textureinthread.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/scenegraph/textureinthread
INSTALLS += target

OTHER_FILES += \
    main.qml \
    error.qml
