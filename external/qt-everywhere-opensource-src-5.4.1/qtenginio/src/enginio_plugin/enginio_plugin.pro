requires(qtHaveModule(qml))

TARGETPATH = Enginio

QT = qml enginio enginio-private core-private

QMAKE_DOCS = $$PWD/doc/qtenginioqml.qdocconf
OTHER_FILES += \
    doc/qtenginioqml.qdocconf \
    doc/enginio_plugin.qdoc

include(../src.pri)


TARGET = enginioplugin
TARGET.module_name = Enginio

SOURCES += \
    enginioqmlclient.cpp \
    enginioqmlmodel.cpp \
    enginioplugin.cpp \
    enginioqmlreply.cpp \

HEADERS += \
    enginioqmlobjectadaptor_p.h \
    enginioqmlclient_p_p.h \
    enginioplugin_p.h \
    enginioqmlclient_p.h \
    enginioqmlmodel_p.h \
    enginioqmlreply_p.h

CONFIG += no_cxx_module
load(qml_plugin)

QMLDIRFILE = $${_PRO_FILE_PWD_}/qmldir

copy2build.input = QMLDIRFILE
copy2build.output = ../../qml/$${TARGET.module_name}/qmldir
!contains(TEMPLATE_PREFIX, vc):copy2build.variable_out = PRE_TARGETDEPS
copy2build.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
copy2build.name = COPY ${QMAKE_FILE_IN}
copy2build.CONFIG += no_link
force_independent: QMAKE_EXTRA_COMPILERS += copy2build

DEFINES +=  "ENGINIO_VERSION=\\\"$$MODULE_VERSION\\\""
