QT += core-private gui-private quick-private compositor-private

LIBS += -lwayland-server

SOURCES += \
    main.cpp \
    serverbufferitem.cpp
HEADERS += \
    serverbufferitem.h \

OTHER_FILES = \
    qml/main.qml \
    images/background.jpg \

RESOURCES += compositor.qrc

CONFIG +=wayland-scanner
WAYLANDSERVERSOURCES += ../share-buffer.xml

CONFIG += link_pkgconfig
PKGCONFIG += wayland-server

DEFINES += QT_COMPOSITOR_QUICK
