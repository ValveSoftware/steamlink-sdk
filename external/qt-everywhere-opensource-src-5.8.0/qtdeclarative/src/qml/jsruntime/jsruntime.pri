INCLUDEPATH += $$PWD
INCLUDEPATH += $$OUT_PWD

!qmldevtools_build {
SOURCES += \
    $$PWD/qv4engine.cpp \
    $$PWD/qv4context.cpp \
    $$PWD/qv4persistent.cpp \
    $$PWD/qv4lookup.cpp \
    $$PWD/qv4identifier.cpp \
    $$PWD/qv4identifiertable.cpp \
    $$PWD/qv4managed.cpp \
    $$PWD/qv4internalclass.cpp \
    $$PWD/qv4sparsearray.cpp \
    $$PWD/qv4arraydata.cpp \
    $$PWD/qv4arrayobject.cpp \
    $$PWD/qv4argumentsobject.cpp \
    $$PWD/qv4booleanobject.cpp \
    $$PWD/qv4dateobject.cpp \
    $$PWD/qv4errorobject.cpp \
    $$PWD/qv4function.cpp \
    $$PWD/qv4functionobject.cpp \
    $$PWD/qv4globalobject.cpp \
    $$PWD/qv4jsonobject.cpp \
    $$PWD/qv4mathobject.cpp \
    $$PWD/qv4memberdata.cpp \
    $$PWD/qv4numberobject.cpp \
    $$PWD/qv4object.cpp \
    $$PWD/qv4objectproto.cpp \
    $$PWD/qv4regexpobject.cpp \
    $$PWD/qv4stringobject.cpp \
    $$PWD/qv4variantobject.cpp \
    $$PWD/qv4objectiterator.cpp \
    $$PWD/qv4regexp.cpp \
    $$PWD/qv4serialize.cpp \
    $$PWD/qv4script.cpp \
    $$PWD/qv4executableallocator.cpp \
    $$PWD/qv4sequenceobject.cpp \
    $$PWD/qv4include.cpp \
    $$PWD/qv4qobjectwrapper.cpp \
    $$PWD/qv4arraybuffer.cpp \
    $$PWD/qv4typedarray.cpp \
    $$PWD/qv4dataview.cpp

!contains(QT_CONFIG, no-qml-debug): SOURCES += $$PWD/qv4profiling.cpp

HEADERS += \
    $$PWD/qv4global_p.h \
    $$PWD/qv4engine_p.h \
    $$PWD/qv4context_p.h \
    $$PWD/qv4context_p_p.h \
    $$PWD/qv4math_p.h \
    $$PWD/qv4persistent_p.h \
    $$PWD/qv4debugging_p.h \
    $$PWD/qv4lookup_p.h \
    $$PWD/qv4identifier_p.h \
    $$PWD/qv4identifiertable_p.h \
    $$PWD/qv4managed_p.h \
    $$PWD/qv4internalclass_p.h \
    $$PWD/qv4sparsearray_p.h \
    $$PWD/qv4arraydata_p.h \
    $$PWD/qv4arrayobject_p.h \
    $$PWD/qv4argumentsobject_p.h \
    $$PWD/qv4booleanobject_p.h \
    $$PWD/qv4dateobject_p.h \
    $$PWD/qv4errorobject_p.h \
    $$PWD/qv4function_p.h \
    $$PWD/qv4functionobject_p.h \
    $$PWD/qv4globalobject_p.h \
    $$PWD/qv4jsonobject_p.h \
    $$PWD/qv4mathobject_p.h \
    $$PWD/qv4memberdata_p.h \
    $$PWD/qv4numberobject_p.h \
    $$PWD/qv4object_p.h \
    $$PWD/qv4objectproto_p.h \
    $$PWD/qv4regexpobject_p.h \
    $$PWD/qv4stringobject_p.h \
    $$PWD/qv4variantobject_p.h \
    $$PWD/qv4property_p.h \
    $$PWD/qv4objectiterator_p.h \
    $$PWD/qv4regexp_p.h \
    $$PWD/qv4serialize_p.h \
    $$PWD/qv4script_p.h \
    $$PWD/qv4scopedvalue_p.h \
    $$PWD/qv4util_p.h \
    $$PWD/qv4executableallocator_p.h \
    $$PWD/qv4sequenceobject_p.h \
    $$PWD/qv4include_p.h \
    $$PWD/qv4qobjectwrapper_p.h \
    $$PWD/qv4profiling_p.h \
    $$PWD/qv4arraybuffer_p.h \
    $$PWD/qv4typedarray_p.h \
    $$PWD/qv4dataview_p.h

qtConfig(qml-interpreter) {
    HEADERS += \
        $$PWD/qv4vme_moth_p.h
    SOURCES += \
        $$PWD/qv4vme_moth.cpp
}

}


HEADERS += \
    $$PWD/qv4runtime_p.h \
    $$PWD/qv4runtimeapi_p.h \
    $$PWD/qv4value_p.h \
    $$PWD/qv4string_p.h \
    $$PWD/qv4value_p.h

SOURCES += \
    $$PWD/qv4runtime.cpp \
    $$PWD/qv4string.cpp \
    $$PWD/qv4value.cpp

valgrind {
    DEFINES += V4_USE_VALGRIND
}

heaptrack {
    DEFINES += V4_USE_HEAPTRACK
}
