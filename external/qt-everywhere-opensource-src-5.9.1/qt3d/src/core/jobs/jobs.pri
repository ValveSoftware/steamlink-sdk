
SOURCES += \
    $$PWD/qaspectjob.cpp \
    $$PWD/qaspectjobmanager.cpp \
    $$PWD/qabstractaspectjobmanager.cpp \
    $$PWD/qthreadpooler.cpp \
    $$PWD/task.cpp \
    $$PWD/dependencyhandler.cpp

HEADERS += \
    $$PWD/qaspectjob.h \
    $$PWD/qaspectjob_p.h \
    $$PWD/qaspectjobproviderinterface_p.h \
    $$PWD/qaspectjobmanager_p.h \
    $$PWD/qabstractaspectjobmanager_p.h \
    $$PWD/dependencyhandler_p.h \
    $$PWD/task_p.h \
    $$PWD/qthreadpooler_p.h

INCLUDEPATH += $$PWD

#DEFINES += QT3DCORE_ASPECT_JOB_DEBUG
