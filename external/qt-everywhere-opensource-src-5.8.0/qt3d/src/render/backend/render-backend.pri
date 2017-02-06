#DEFINES += QT3D_RENDER_VIEW_JOB_TIMING
#DEFINES += QT3D_RENDER_DUMP_BACKEND_NODES

INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/renderthread_p.h \
    $$PWD/renderconfiguration_p.h \
    $$PWD/renderer_p.h \
    $$PWD/renderview_p.h \
    $$PWD/rendercommand_p.h \
    $$PWD/renderqueue_p.h \
    $$PWD/parameterpack_p.h \
    $$PWD/rendertarget_p.h \
    $$PWD/attachmentpack_p.h \
    $$PWD/shadervariables_p.h \
    $$PWD/qgraphicsutils_p.h \
    $$PWD/managers_p.h \
    $$PWD/handle_types_p.h \
    $$PWD/platformsurfacefilter_p.h \
    $$PWD/cameralens_p.h \
    $$PWD/entity_p.h \
    $$PWD/layer_p.h \
    $$PWD/nodefunctor_p.h \
    $$PWD/transform_p.h \
    $$PWD/boundingvolumedebug_p.h \
    $$PWD/nodemanagers_p.h \
    $$PWD/triangleboundingvolume_p.h \
    $$PWD/openglvertexarrayobject_p.h \
    $$PWD/trianglesextractor_p.h \
    $$PWD/trianglesvisitor_p.h \
    $$PWD/abstractrenderer_p.h \
    $$PWD/computecommand_p.h \
    $$PWD/rendersettings_p.h \
    $$PWD/stringtoint_p.h \
    $$PWD/backendnode_p.h \
    $$PWD/rendertargetoutput_p.h \
    $$PWD/commandexecuter_p.h \
    $$PWD/uniform_p.h \
    $$PWD/shaderparameterpack_p.h \
    $$PWD/renderviewbuilder_p.h \
    $$PWD/offscreensurfacehelper_p.h

SOURCES += \
    $$PWD/renderthread.cpp \
    $$PWD/renderconfiguration.cpp \
    $$PWD/renderer.cpp \
    $$PWD/renderview.cpp \
    $$PWD/rendercommand.cpp \
    $$PWD/renderqueue.cpp \
    $$PWD/parameterpack.cpp \
    $$PWD/rendertarget.cpp \
    $$PWD/managers.cpp \
    $$PWD/platformsurfacefilter.cpp \
    $$PWD/cameralens.cpp \
    $$PWD/entity.cpp \
    $$PWD/layer.cpp \
    $$PWD/transform.cpp \
    $$PWD/boundingvolumedebug.cpp \
    $$PWD/nodemanagers.cpp \
    $$PWD/triangleboundingvolume.cpp \
    $$PWD/trianglesextractor.cpp \
    $$PWD/trianglesvisitor.cpp \
    $$PWD/computecommand.cpp \
    $$PWD/rendersettings.cpp \
    $$PWD/stringtoint.cpp \
    $$PWD/backendnode.cpp \
    $$PWD/rendertargetoutput.cpp \
    $$PWD/attachmentpack.cpp \
    $$PWD/commandexecuter.cpp \
    $$PWD/openglvertexarrayobject.cpp \
    $$PWD/uniform.cpp \
    $$PWD/shaderparameterpack.cpp \
    $$PWD/renderviewbuilder.cpp \
    $$PWD/offscreensurfacehelper.cpp

