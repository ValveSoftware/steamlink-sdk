contains(QT_CONFIG, no-qml-debug) {
    DEFINES += QT_NO_QML_DEBUGGER
    MODULE_DEFINES += QT_NO_QML_DEBUGGER
} else {
    HEADERS += \
        $$PWD/qqmldebugpluginmanager_p.h \
        $$PWD/qqmldebugservicefactory_p.h

    SOURCES += \
        $$PWD/qqmldebug.cpp \
        $$PWD/qqmldebugconnector.cpp \
        $$PWD/qqmldebugservice.cpp \
        $$PWD/qqmlabstractprofileradapter.cpp \
        $$PWD/qqmlmemoryprofiler.cpp \
        $$PWD/qqmlprofiler.cpp \
        $$PWD/qqmldebugserviceinterfaces.cpp
}

HEADERS += \
    $$PWD/qqmldebugconnector_p.h \
    $$PWD/qqmldebugservice_p.h \
    $$PWD/qqmldebugserviceinterfaces_p.h \
    $$PWD/qqmldebugstatesdelegate_p.h \
    $$PWD/qqmldebug.h \
    $$PWD/qqmlmemoryprofiler_p.h \
    $$PWD/qqmlprofilerdefinitions_p.h \
    $$PWD/qqmlabstractprofileradapter_p.h \
    $$PWD/qqmlprofiler_p.h

INCLUDEPATH += $$PWD
