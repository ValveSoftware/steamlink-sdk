#DEFINES += QT3D_CORE_JOB_TIMING

SOURCES += \
        $$PWD/qabstractaspect.cpp \
        $$PWD/qaspectengine.cpp \
        $$PWD/qaspectfactory.cpp \
        $$PWD/qaspectmanager.cpp \
        $$PWD/qaspectthread.cpp

HEADERS += \
        $$PWD/qabstractaspect.h \
        $$PWD/qaspectengine.h \
        $$PWD/qabstractaspect_p.h \
        $$PWD/qaspectengine_p.h \
        $$PWD/qaspectfactory_p.h \
        $$PWD/qaspectmanager_p.h \
        $$PWD/qaspectthread_p.h

INCLUDEPATH += $$PWD

include($$OUT_PWD/../core/qt3dcore-config.pri)
QT_FOR_CONFIG += 3dcore-private
qtConfig(qt3d-profile-jobs): {
    HEADERS += $$PWD/aspectcommanddebugger_p.h
    SOURCES += $$PWD/aspectcommanddebugger.cpp
}
