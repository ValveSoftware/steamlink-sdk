TARGET = QtScxml
QT = core-private qml-private
MODULE_CONFIG += c++11 qscxmlc

load(qt_module)

QMAKE_DOCS = $$PWD/doc/qtscxml.qdocconf

CONFIG  += $$MODULE_CONFIG
DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII

HEADERS += \
    qscxmlcompiler.h \
    qscxmlcompiler_p.h \
    qscxmlstatemachine.h \
    qscxmlstatemachine_p.h \
    qscxmlglobals.h \
    qscxmlglobals_p.h \
    qscxmlnulldatamodel.h \
    qscxmlecmascriptdatamodel.h \
    qscxmlecmascriptplatformproperties_p.h \
    qscxmlexecutablecontent.h \
    qscxmlexecutablecontent_p.h \
    qscxmlevent.h \
    qscxmlevent_p.h \
    qscxmldatamodel.h \
    qscxmldatamodel_p.h \
    qscxmlcppdatamodel_p.h \
    qscxmlcppdatamodel.h \
    qscxmlerror.h \
    qscxmlinvokableservice_p.h \
    qscxmlinvokableservice.h \
    qscxmltabledata.h \
    qscxmltabledata_p.h \
    qscxmlstatemachineinfo_p.h

SOURCES += \
    qscxmlcompiler.cpp \
    qscxmlstatemachine.cpp \
    qscxmlnulldatamodel.cpp \
    qscxmlecmascriptdatamodel.cpp \
    qscxmlecmascriptplatformproperties.cpp \
    qscxmlexecutablecontent.cpp \
    qscxmlevent.cpp \
    qscxmldatamodel.cpp \
    qscxmlcppdatamodel.cpp \
    qscxmlerror.cpp \
    qscxmlinvokableservice.cpp \
    qscxmltabledata.cpp \
    qscxmlstatemachineinfo.cpp

FEATURES += ../../mkspecs/features/qscxmlc.prf
features.files = $$FEATURES
features.path = $$[QT_HOST_DATA]/mkspecs/features/
INSTALLS += features

!force_independent:if(!debug_and_release|!build_all|CONFIG(release, debug|release)) {
    # Copy qscxmlc.prf to the qtbase build directory (for non-installed developer builds)
    prf2build.input = FEATURES
    prf2build.output = $$[QT_INSTALL_DATA/get]/mkspecs/features/${QMAKE_FILE_BASE}${QMAKE_FILE_EXT}
    prf2build.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
    prf2build.name = COPY ${QMAKE_FILE_IN}
    prf2build.CONFIG = no_link no_clean target_predeps
    QMAKE_EXTRA_COMPILERS += prf2build
}
