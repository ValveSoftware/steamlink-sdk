QT += widgets

VERSION = $$QT_VERSION

HEADERS += \
           demoapplication.h \
           fluidlauncher.h \
           pictureflow.h \
           slideshow.h

SOURCES += \
           demoapplication.cpp \
           fluidlauncher.cpp \
           main.cpp \
           pictureflow.cpp \
           slideshow.cpp

RESOURCES = fluidlauncher.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/svg/embedded/fluidlauncher
INSTALLS += target

wince*{
    QT += svg

    BUILD_DIR = release
    if(!debug_and_release|build_pass):CONFIG(debug, debug|release) {
        BUILD_DIR = debug
    }

    requires(!isEmpty(_QMAKE_SUPER_CACHE_))
    base = $$dirname(_QMAKE_SUPER_CACHE_)
    baseex = $$base/qtbase/examples
    svgex = $$base/qtsvg/examples
    executables.files = \
        $$svgex/svg/embedded/embeddedsvgviewer/$${BUILD_DIR}/embeddedsvgviewer.exe \
        $$baseex/embedded/styleexample/$${BUILD_DIR}/styleexample.exe \
        $$baseex/painting/deform/$${BUILD_DIR}/deform.exe \
        $$baseex/painting/pathstroke/$${BUILD_DIR}/pathstroke.exe \
        $$baseex/graphicsview/elasticnodes/$${BUILD_DIR}/elasticnodes.exe \
        $$baseex/widgets/wiggly/$${BUILD_DIR}/wiggly.exe \
        $$baseex/painting/concentriccircles/$${BUILD_DIR}/concentriccircles.exe \
        $$baseex/draganddrop/$${BUILD_DIR}/fridgemagnets.exe

    executables.path = .

    files.files = $$PWD/screenshots $$PWD/slides $$PWD/../embeddedsvgviewer/shapes.svg
    files.path = .

    config.files = $$PWD/config_wince/config.xml
    config.path = .

    DEPLOYMENT += config files executables

    DEPLOYMENT_PLUGIN += qgif qjpeg qmng qsvg
}
