CXX_MODULE = qml
TARGET  = qwaylandcompositorplugin
TARGETPATH = QtWayland/Compositor
IMPORT_VERSION = 1.0

HEADERS += \
    qwaylandmousetracker_p.h

SOURCES += \
    qwaylandquickcompositorplugin.cpp \
    qwaylandmousetracker.cpp

COMPOSITOR_QML_FILES += \
    WaylandOutputWindow.qml \
    WaylandCursorItem.qml \
    qmldir

DEFINES += QT_WAYLAND_COMPOSITOR_QUICK

# Create the resource file
GENERATED_RESOURCE_FILE = $$OUT_PWD/compositor.qrc

RESOURCE_CONTENT = \
    "<RCC>" \
    "<qresource prefix=\"/QtWayland/Compositor\">"

for(resourcefile, COMPOSITOR_QML_FILES) {
    resourcefileabsolutepath = $$absolute_path($$resourcefile)
    relativepath_in = $$relative_path($$resourcefileabsolutepath, $$_PRO_FILE_PWD_)
    relativepath_out = $$relative_path($$resourcefileabsolutepath, $$OUT_PWD)
    RESOURCE_CONTENT += "<file alias=\"$$relativepath_in\">$$relativepath_out</file>"
}

RESOURCE_CONTENT += \
    "</qresource>" \
    "</RCC>"

write_file($$GENERATED_RESOURCE_FILE, RESOURCE_CONTENT)|error("Aborting.")

RESOURCES += $$GENERATED_RESOURCE_FILE

# In case of a debug build, deploy the QML files too
CONFIG(debug, debug|release): QML_FILES += $$COMPOSITOR_QML_FILES

QT += quick-private qml-private gui-private core-private waylandcompositor waylandcompositor-private

load(qml_plugin)
