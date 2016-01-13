TARGET     = QtDeclarative
QT         = core-private gui-private widgets-private script-private
qtHaveModule(xmlpatterns): QT_PRIVATE = xmlpatterns
else: DEFINES += QT_NO_XMLPATTERNS

MODULE=declarative
MODULE_PLUGIN_TYPES = \
    qml1tooling

ANDROID_BUNDLED_FILES = \
    imports

load(qt_module)

DEFINES   += QT_NO_URL_CAST_FROM_STRING

win32-msvc*|win32-icc:QMAKE_LFLAGS += /BASE:0x66000000
solaris-cc*:QMAKE_CXXFLAGS_RELEASE -= -O2

exists("qdeclarative_enable_gcov") {
    QMAKE_CXXFLAGS = -fprofile-arcs -ftest-coverage -fno-elide-constructors
    LIBS_PRIVATE += -lgcov
}

#modules
include(qml/qml.pri)
include(util/util.pri)
include(graphicsitems/graphicsitems.pri)
include(debugger/debugger.pri)

HEADERS += \
    qtdeclarativeglobal.h \
    qtdeclarativeglobal_p.h

linux-g++-maemo:DEFINES += QDECLARATIVEVIEW_NOBACKGROUND

DEFINES += QT_NO_OPENTYPE

blackberry: {
    DEFINES   += CUSTOM_DECLARATIVE_DEBUG_TRACE_INSTANCE
}
