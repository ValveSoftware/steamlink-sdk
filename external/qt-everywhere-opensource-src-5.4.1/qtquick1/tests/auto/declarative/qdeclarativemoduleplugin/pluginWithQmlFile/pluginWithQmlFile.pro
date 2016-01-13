TEMPLATE = lib
CONFIG += plugin
SOURCES = plugin.cpp
QT = core declarative
DESTDIR = ../imports/org/qtproject/AutoTestPluginWithQmlFile

include(../qmldir_copier.pri)

# Copy the necessary QML files to the build tree
QML_IN_FILE = $${_PRO_FILE_PWD_}/MyQmlFile.qml
QML_OUT_FILE = $$OUT_PWD/$$DESTDIR/MyQmlFile.qml

QML_OUT_DIR = $$OUT_PWD/$$DESTDIR
QML_OUT_DIR ~= s,/,$$QMAKE_DIR_SEP,

copyMyQmlFile.input = QML_IN_FILE
copyMyQmlFile.output = $$QML_OUT_FILE
!contains(TEMPLATE_PREFIX, vc):copyMyQmlFile.variable_out = PRE_TARGETDEPS
win32 {
    copyMyQmlFile.commands = ($$QMAKE_CHK_DIR_EXISTS $$QML_OUT_DIR $(MKDIR) $$QML_OUT_DIR) && $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
} else {
    copyMyQmlFile.commands = $(MKDIR) $$QML_OUT_DIR && $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
}
copyMyQmlFile.name = COPY ${QMAKE_FILE_IN}
copyMyQmlFile.CONFIG += no_link
# `clean' should leave the build in a runnable state, which means it shouldn't delete
copyMyQmlFile.CONFIG += no_clean
QMAKE_EXTRA_COMPILERS += copyMyQmlFile

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
