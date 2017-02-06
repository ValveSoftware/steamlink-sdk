TEMPLATE = app

# FIXME: Re-enable once we want to run tests on the CI
# CONFIG += testcase

VPATH += $$_PRO_FILE_PWD_
TARGET = tst_$$TARGET

SOURCES += $${TARGET}.cpp
INCLUDEPATH += \
    $$PWD \
    ../shared

QT += testlib network quick webview-private

osx: CONFIG -= app_bundle

DISTFILES += \
    $$PWD/qquickwebview/dummy.qml

RESOURCES += \
    $$PWD/html/testdata.qrc
