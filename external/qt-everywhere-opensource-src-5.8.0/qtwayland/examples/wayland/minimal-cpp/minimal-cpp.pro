QT += gui gui-private core-private waylandcompositor waylandcompositor-private

LIBS += -L ../../lib

HEADERS += \
    compositor.h \
    window.h

SOURCES += main.cpp \
    compositor.cpp \
    window.cpp

# to make QtWaylandCompositor/... style includes working without installing
INCLUDEPATH += $$PWD/../../include


target.path = $$[QT_INSTALL_EXAMPLES]/wayland/minimal-cpp
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS minimal-cpp.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/wayland/minimal-cpp
INSTALLS += target sources
