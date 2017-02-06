CXX_MODULE = qml
TARGET = quick3dextrasplugin
TARGETPATH = Qt3D/Extras
IMPORT_VERSION = 2.0

QT += core-private qml qml-private quick quick-private 3dcore 3dcore-private 3dquick 3dquick-private 3dextras 3dlogic

# Qt3D is free of Q_FOREACH - make sure it stays that way:
DEFINES += QT_NO_FOREACH

HEADERS += \
    qt3dquick3dextrasplugin.h

SOURCES += \
    qt3dquick3dextrasplugin.cpp

load(qml_plugin)

include(./defaults/defaults.pri)

OTHER_FILES += \
    qmldir \
    $$QML_FILES

# Create a resource file for qml files that need to be registered by the plugin
GENERATED_RESOURCE_FILE = $$OUT_PWD/defaults.qrc
INCLUDED_RESOURCE_FILES = $$QML_FILES
RESOURCE_CONTENT = \
    "<RCC>" \
    "<qresource prefix=\"/qt-project.org/imports/Qt3D/Extras/\">"

for(resourcefile, INCLUDED_RESOURCE_FILES) {
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
