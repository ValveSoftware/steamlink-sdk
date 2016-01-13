TEMPLATE=app
TARGET=tst_qmltestexample

SOURCES += tst_qmltest.cpp

!QTDIR_build {
# This is the code actual testcases should use:

CONFIG += qmltestcase

TESTDATA += tst_basic.qml tst_item.qml

} else {
# This code exists solely for the purpose of building this example
# inside the examples/ hierarchy.

QT += qml qmltest

macx: CONFIG -= app_bundle

target.path = $$[QT_INSTALL_EXAMPLES]/qmltest/qmltest
qml.files = tst_basic.qml tst_item.qml
qml.path = $$[QT_INSTALL_EXAMPLES]/qmltest/qmltest
INSTALLS += target qml

}
