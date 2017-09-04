TEMPLATE = app
TARGET = tst_qmlwebsockets
CONFIG += qmltestcase
CONFIG += console
SOURCES += tst_qmlwebsockets.cpp

importFiles.path = .
DEPLOYMENT += importFiles

