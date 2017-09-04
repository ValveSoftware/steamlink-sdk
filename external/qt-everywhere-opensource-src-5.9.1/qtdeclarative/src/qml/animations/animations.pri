INCLUDEPATH += $$PWD

HEADERS += \
        $$PWD/qabstractanimationjob_p.h \
        $$PWD/qanimationgroupjob_p.h \
        $$PWD/qsequentialanimationgroupjob_p.h \
        $$PWD/qparallelanimationgroupjob_p.h \
        $$PWD/qcontinuinganimationgroupjob_p.h \
        $$PWD/qpauseanimationjob_p.h \
        $$PWD/qanimationjobutil_p.h

SOURCES += \
        $$PWD/qabstractanimationjob.cpp \
        $$PWD/qanimationgroupjob.cpp \
        $$PWD/qsequentialanimationgroupjob.cpp \
        $$PWD/qparallelanimationgroupjob.cpp \
        $$PWD/qcontinuinganimationgroupjob.cpp \
        $$PWD/qpauseanimationjob.cpp
