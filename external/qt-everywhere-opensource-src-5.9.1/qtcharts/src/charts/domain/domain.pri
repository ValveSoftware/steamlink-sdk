#Subdirectiores are defined here, because qt creator doesn't handle nested include(foo.pri) chains very well.

INCLUDEPATH += $$PWD

SOURCES += \
    $$PWD/abstractdomain.cpp \
    $$PWD/polardomain.cpp \
    $$PWD/xydomain.cpp \
    $$PWD/xypolardomain.cpp \
    $$PWD/xlogydomain.cpp \
    $$PWD/xlogypolardomain.cpp \
    $$PWD/logxydomain.cpp \
    $$PWD/logxypolardomain.cpp \
    $$PWD/logxlogydomain.cpp \
    $$PWD/logxlogypolardomain.cpp

PRIVATE_HEADERS += \
    $$PWD/abstractdomain_p.h \
    $$PWD/polardomain_p.h \
    $$PWD/xydomain_p.h \
    $$PWD/xypolardomain_p.h \
    $$PWD/xlogydomain_p.h \
    $$PWD/xlogypolardomain_p.h \
    $$PWD/logxydomain_p.h \
    $$PWD/logxypolardomain_p.h \
    $$PWD/logxlogydomain_p.h \
    $$PWD/logxlogypolardomain_p.h
