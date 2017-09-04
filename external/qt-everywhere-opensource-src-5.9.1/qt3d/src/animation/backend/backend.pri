
INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/handle_types_p.h \
    $$PWD/handler_p.h \
    $$PWD/nodefunctor_p.h \
    $$PWD/managers_p.h \
    $$PWD/keyframe_p.h \
    $$PWD/fcurve_p.h \
    $$PWD/bezierevaluator_p.h \
    $$PWD/functionrangefinder_p.h \
    $$PWD/clipanimator_p.h \
    $$PWD/blendedclipanimator_p.h \
    $$PWD/backendnode_p.h \
    $$PWD/loadanimationclipjob_p.h \
    $$PWD/channelmapping_p.h \
    $$PWD/channelmapper_p.h \
    $$PWD/findrunningclipanimatorsjob_p.h \
    $$PWD/evaluateclipanimatorjob_p.h \
    $$PWD/clipblendnode_p.h \
    $$PWD/clipblendnodevisitor_p.h \
    $$PWD/animationutils_p.h \
    $$PWD/buildblendtreesjob_p.h \
    $$PWD/evaluateblendclipanimatorjob_p.h \
    $$PWD/lerpclipblend_p.h \
    $$PWD/additiveclipblend_p.h \
    $$PWD/clipblendvalue_p.h \
    $$PWD/animationclip_p.h

SOURCES += \
    $$PWD/handler.cpp \
    $$PWD/fcurve.cpp \
    $$PWD/bezierevaluator.cpp \
    $$PWD/functionrangefinder.cpp \
    $$PWD/clipanimator.cpp \
    $$PWD/blendedclipanimator.cpp \
    $$PWD/backendnode.cpp \
    $$PWD/loadanimationclipjob.cpp \
    $$PWD/channelmapping.cpp \
    $$PWD/channelmapper.cpp \
    $$PWD/findrunningclipanimatorsjob.cpp \
    $$PWD/evaluateclipanimatorjob.cpp \
    $$PWD/clipblendnode.cpp \
    $$PWD/managers.cpp \
    $$PWD/clipblendnodevisitor.cpp \
    $$PWD/animationutils.cpp \
    $$PWD/buildblendtreesjob.cpp \
    $$PWD/evaluateblendclipanimatorjob.cpp \
    $$PWD/lerpclipblend.cpp \
    $$PWD/additiveclipblend.cpp \
    $$PWD/clipblendvalue.cpp \
    $$PWD/animationclip.cpp
