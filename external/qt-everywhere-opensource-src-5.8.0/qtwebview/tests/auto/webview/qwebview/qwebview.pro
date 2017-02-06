CONFIG += testcase parallel_test
TARGET = tst_qwebview
osx:CONFIG -= app_bundle

!qtHaveModule(quick) {
  DEFINES += QT_NO_QQUICKWEBVIEW_TESTS
}

!android: qtHaveModule(webengine) {
    QT += webengine webengine-private
    DEFINES += QT_WEBVIEW_WEBENGINE_BACKEND
}

QT += webview-private testlib
SOURCES += \
    tst_qwebview.cpp
