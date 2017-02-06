QT += gui gui-private core-private waylandcompositor waylandcompositor-private

LIBS += -L ../../lib

HEADERS += \
    window.h \
    compositor.h

SOURCES += main.cpp \
    window.cpp \
    compositor.cpp

# to make QtWaylandCompositor/... style includes working without installing
INCLUDEPATH += $$PWD/../../include


RESOURCES += qwindow-compositor.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/wayland/qwindow-compositor
INSTALLS += target
