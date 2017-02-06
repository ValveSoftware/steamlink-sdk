INCLUDEPATH += $$PWD
INCLUDEPATH += $$OUT_PWD

!qmldevtools_build {
SOURCES += \
    $$PWD/qv4mm.cpp \

HEADERS += \
    $$PWD/qv4mm_p.h

}

HEADERS += \
    $$PWD/qv4heap_p.h
