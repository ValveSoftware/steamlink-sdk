TEMPLATE = lib
# Fake project to make QtCreator happy.

include(core_common.pri)

linking_pri = $$OUT_PWD/$$getConfigDir()/$${TARGET}.pri

!include($$linking_pri) {
    error("Could not find the linking information that gn should have generated.")
}

CHROMIUM_SRC_DIR = $$QTWEBENGINE_ROOT/$$getChromiumSrcDir()
INCLUDEPATH += $$CHROMIUM_SRC_DIR \
               $$OUT_PWD/$$getConfigDir()/gen

SOURCES += $$NINJA_SOURCES
HEADERS += $$NINJA_HEADERS
DEFINES += $$NINJA_DEFINES
