TARGET = tst_checkxmlfiles
CONFIG += testcase
SOURCES += tst_checkxmlfiles.cpp \
           ../qxmlquery/TestFundament.cpp
QT = core gui testlib

include (../xmlpatterns.pri)

wince*: {
QT += network
requires(!isEmpty(_QMAKE_SUPER_CACHE_))
addFiles.files = \
        ../../../examples/xquery/globalVariables/globals.gccxml \
        ../../../../qtdoc/doc/src/diagrams/stylesheet/treeview.svg \
        ../../../../qtdoc/doc/src/diagrams/designer-manual/designer-mainwindow-actions.ui \
        ../../../../qtsql/examples/sql/masterdetail/albumdetails.xml \
        ../../../../qtbase/examples/widgets/tools/undo/undo.qrc
addFiles.path = xmlfiles
DEPLOYMENT += addFiles
} else {
    DEFINES += SOURCETREE=\\\"$$absolute_path(../../..)/\\\"
}
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
