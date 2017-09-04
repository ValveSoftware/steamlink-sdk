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
    $$PWD/levelofdetail_p.h \
    $$PWD/nodefunctor_p.h \
    $$PWD/transform_p.h \
    $$PWD/boundingvolumedebug_p.h \
    $$PWD/nodemanagers_p.h \
    $$PWD/triangleboundingvolume_p.h \
    $$PWD/openglvertexarrayobject_p.h \
    $$PWD/trianglesextractor_p.h \
    $$PWD/buffervisitor_p.h \
    $$PWD/bufferutils_p.h \
    $$PWD/trianglesvisitor_p.h \
    $$PWD/abstractrenderer_p.h \
    $$PWD/computecommand_p.h \
    $$PWD/rendersettings_p.h \
    $$PWD/stringtoint_p.h \
    $$PWD/backendnode_p.h \
    $$PWD/rendertargetoutput_p.h \
    $$PWD/uniform_p.h \
    $$PWD/shaderparameterpack_p.h \
    $$PWD/renderviewbuilder_p.h \
    $$PWD/frameprofiler_p.h \
    $$PWD/offscreensurfacehelper_p.h \
    $$PWD/resourceaccessor_p.h \
    $$PWD/commandthread_p.h

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
    $$PWD/levelofdetail.cpp \
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
    $$PWD/openglvertexarrayobject.cpp \
    $$PWD/uniform.cpp \
    $$PWD/shaderparameterpack.cpp \
    $$PWD/renderviewbuilder.cpp \
    $$PWD/offscreensurfacehelper.cpp \
    $$PWD/resourceaccessor.cpp \
    $$PWD/commandthread.cpp

include($$OUT_PWD/../core/qt3dcore-config.pri)
QT_FOR_CONFIG += 3dcore-private
qtConfig(qt3d-profile-jobs): {
  HEADERS += $$PWD/commandexecuter_p.h
  SOURCES += $$PWD/commandexecuter.cpp
}

