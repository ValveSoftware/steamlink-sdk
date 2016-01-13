TEMPLATE=app
TARGET=tst_qml
CONFIG += qmltestcase
SOURCES += tst_qml.cpp


importFiles.files = soundeffect

importFiles.path = .
DEPLOYMENT += importFiles
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
