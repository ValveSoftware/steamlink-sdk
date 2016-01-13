TARGET = tst_qsvgrenderer
CONFIG += testcase
QT += svg testlib widgets gui-private

SOURCES += tst_qsvgrenderer.cpp
RESOURCES += resources.qrc

wince* {
   addFiles.files = *.svg *.svgz
   addFiles.path = .

   DEPLOYMENT += addFiles
   wince*|qt_not_deployed {
       DEPLOYMENT_PLUGIN += qsvg
   }
}

DEFINES += SRCDIR=\\\"$$PWD/\\\"
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
