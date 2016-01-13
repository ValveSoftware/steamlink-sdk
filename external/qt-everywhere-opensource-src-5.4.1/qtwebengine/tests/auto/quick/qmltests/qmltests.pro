include(../tests.pri)

QT += qmltest

IMPORTPATH += $$PWD/data

OTHER_FILES += \
    $$PWD/data/TestWebEngineView.qml \
    $$PWD/data/favicon.html \
    $$PWD/data/favicon.png \
    $$PWD/data/favicon2.html \
    $$PWD/data/javascript.html \
    $$PWD/data/link.html \
    $$PWD/data/redirect.html \
    $$PWD/data/small-favicon.png \
    $$PWD/data/test1.html \
    $$PWD/data/test2.html \
    $$PWD/data/test3.html \
    $$PWD/data/tst_desktopBehaviorLoadHtml.qml \
    $$PWD/data/tst_favIconLoad.qml \
    $$PWD/data/tst_linkHovered.qml \
    $$PWD/data/tst_loadFail.qml \
    $$PWD/data/tst_loadHtml.qml \
    $$PWD/data/tst_loadProgress.qml \
    $$PWD/data/tst_loadProgressSignal.qml \
    $$PWD/data/tst_loadUrl.qml \
    $$PWD/data/tst_navigationHistory.qml \
    $$PWD/data/tst_navigationRequested.qml \
    $$PWD/data/tst_properties.qml \
    $$PWD/data/tst_runJavaScript.qml \
    $$PWD/data/tst_titleChanged.qml

load(qt_build_paths)
DEFINES += QUICK_TEST_SOURCE_DIR=\"\\\"$$PWD$${QMAKE_DIR_SEP}data\\\"\"
