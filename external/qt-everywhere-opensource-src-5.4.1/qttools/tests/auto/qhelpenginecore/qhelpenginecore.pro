TARGET = tst_qhelpenginecore
CONFIG += testcase
SOURCES += tst_qhelpenginecore.cpp
QT      += help sql testlib help


DEFINES += QT_USE_USING_NAMESPACE

wince*: {
   DEFINES += SRCDIR=\\\"./\\\"
   QT += network
   addFiles.files = $$PWD/data/*.*
   addFiles.path = data
   clucene.files = $$QT.clucene.libs/QtCLucene*.dll

   DEPLOYMENT += addFiles
   DEPLOYMENT += clucene

   DEPLOYMENT_PLUGIN += qsqlite
} else {
   DEFINES += SRCDIR=\\\"$$PWD\\\"
}
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
