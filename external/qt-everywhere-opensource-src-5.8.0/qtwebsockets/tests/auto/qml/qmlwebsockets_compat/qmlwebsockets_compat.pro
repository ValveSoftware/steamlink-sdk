TEMPLATE = app
TARGET = tst_qmlwebsockets_compat
CONFIG += qmltestcase
CONFIG += console
SOURCES += tst_qmlwebsockets_compat.cpp

importFiles.path = .
DEPLOYMENT += importFiles

