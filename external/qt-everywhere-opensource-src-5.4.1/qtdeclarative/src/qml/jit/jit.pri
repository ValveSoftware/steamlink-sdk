include(../../3rdparty/masm/masm-defs.pri)

INCLUDEPATH += $$PWD
INCLUDEPATH += $$OUT_PWD

HEADERS += \
    $$PWD/qv4assembler_p.h \
    $$PWD/qv4regalloc_p.h \
    $$PWD/qv4targetplatform_p.h \
    $$PWD/qv4isel_masm_p.h \
    $$PWD/qv4binop_p.h \
    $$PWD/qv4unop_p.h \
    $$PWD/qv4registerinfo_p.h

SOURCES += \
    $$PWD/qv4assembler.cpp \
    $$PWD/qv4regalloc.cpp \
    $$PWD/qv4isel_masm.cpp \
    $$PWD/qv4binop.cpp \
    $$PWD/qv4unop.cpp \

include(../../3rdparty/masm/masm.pri)
