HEADERS +=  \
    $$PWD/qbitfield_p.h \
    $$PWD/qintrusivelist_p.h \
    $$PWD/qpodvector_p.h \
    $$PWD/qhashedstring_p.h \
    $$PWD/qqmlrefcount_p.h \
    $$PWD/qqmlpool_p.h \
    $$PWD/qfieldlist_p.h \
    $$PWD/qhashfield_p.h \
    $$PWD/qqmlthread_p.h \
    $$PWD/qfinitestack_p.h \
    $$PWD/qrecursionwatcher_p.h \
    $$PWD/qdeletewatcher_p.h \
    $$PWD/qrecyclepool_p.h \
    $$PWD/qflagpointer_p.h \
    $$PWD/qqmltrace_p.h \
    $$PWD/qpointervaluepair_p.h \
    $$PWD/qlazilyallocated_p.h \

SOURCES += \
    $$PWD/qintrusivelist.cpp \
    $$PWD/qhashedstring.cpp \
    $$PWD/qqmlpool.cpp \
    $$PWD/qqmlthread.cpp \
    $$PWD/qqmltrace.cpp \

# mirrors logic in $$QT_SOURCE_TREE/config.tests/unix/clock-gettime/clock-gettime.pri
# clock_gettime() is implemented in librt on these systems
contains(QT_CONFIG, clock-gettime):linux-*|hpux-*|solaris-*:LIBS_PRIVATE *= -lrt
