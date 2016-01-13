TEMPLATE = lib
CONFIG += plugin
SOURCES = plugin.cpp
QT = core declarative
DESTDIR = ../imports/org/qtproject/PureQmlModule

include(../qmldir_copier.pri)

# Copy the necessary QML files to the build tree
COMPONENTA_IN_FILE = $${_PRO_FILE_PWD_}/ComponentA.qml
COMPONENTA_OUT_FILE = $$OUT_PWD/$$DESTDIR/ComponentA.qml

COMPONENTA_OUT_DIR = $$OUT_PWD/$$DESTDIR
COMPONENTA_OUT_DIR ~= s,/,$$QMAKE_DIR_SEP,

copyComponentA.input = COMPONENTA_IN_FILE
copyComponentA.output = $$COMPONENTA_OUT_FILE
!contains(TEMPLATE_PREFIX, vc):copyComponentA.variable_out = PRE_TARGETDEPS
win32 {
    copyComponentA.commands = ($$QMAKE_CHK_DIR_EXISTS $$COMPONENTA_OUT_DIR $(MKDIR) $$COMPONENTA_OUT_DIR) && $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
} else {
    copyComponentA.commands = $(MKDIR) $$COMPONENTA_OUT_DIR && $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
}
copyComponentA.name = COPY ${QMAKE_FILE_IN}
copyComponentA.CONFIG += no_link
# `clean' should leave the build in a runnable state, which means it shouldn't delete
copyComponentA.CONFIG += no_clean
QMAKE_EXTRA_COMPILERS += copyComponentA

COMPONENTB_IN_FILE = $${_PRO_FILE_PWD_}/ComponentB.qml
COMPONENTB_OUT_FILE = $$OUT_PWD/$$DESTDIR/ComponentB.qml

COMPONENTB_OUT_DIR = $$OUT_PWD/$$DESTDIR
COMPONENTB_OUT_DIR ~= s,/,$$QMAKE_DIR_SEP,

copyComponentB.input = COMPONENTB_IN_FILE
copyComponentB.output = $$COMPONENTB_OUT_FILE
!contains(TEMPLATE_PREFIX, vc):copyComponentB.variable_out = PRE_TARGETDEPS
win32 {
    copyComponentB.commands = ($$QMAKE_CHK_DIR_EXISTS $$COMPONENTB_OUT_DIR $(MKDIR) $$COMPONENTB_OUT_DIR) && $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
} else {
    copyComponentB.commands = $(MKDIR) $$COMPONENTB_OUT_DIR && $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
}
copyComponentB.name = COPY ${QMAKE_FILE_IN}
copyComponentB.CONFIG += no_link
# `clean' should leave the build in a runnable state, which means it shouldn't delete
copyComponentB.CONFIG += no_clean
QMAKE_EXTRA_COMPILERS += copyComponentB

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
