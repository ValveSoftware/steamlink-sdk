TEMPLATE = lib
CONFIG += plugin
SOURCES = plugin.cpp
QT = core declarative
DESTDIR = ../imports/org/qtproject/AutoTestQmlMixedPluginType

include(../qmldir_copier.pri)

# Copy the necessary QML files to the build tree
FOO_IN_FILE = $${_PRO_FILE_PWD_}/Foo.qml
FOO_OUT_FILE = $$OUT_PWD/$$DESTDIR/Foo.qml

FOO_OUT_DIR = $$OUT_PWD/$$DESTDIR
FOO_OUT_DIR ~= s,/,$$QMAKE_DIR_SEP,

copyFoo.input = FOO_IN_FILE
copyFoo.output = $$FOO_OUT_FILE
!contains(TEMPLATE_PREFIX, vc):copyFoo.variable_out = PRE_TARGETDEPS
win32 {
    copyFoo.commands = ($$QMAKE_CHK_DIR_EXISTS $$FOO_OUT_DIR $(MKDIR) $$FOO_OUT_DIR) && $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
} else {
    copyFoo.commands = $(MKDIR) $$FOO_OUT_DIR && $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
}
copyFoo.name = COPY ${QMAKE_FILE_IN}
copyFoo.CONFIG += no_link
# `clean' should leave the build in a runnable state, which means it shouldn't delete
copyFoo.CONFIG += no_clean
QMAKE_EXTRA_COMPILERS += copyFoo

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
