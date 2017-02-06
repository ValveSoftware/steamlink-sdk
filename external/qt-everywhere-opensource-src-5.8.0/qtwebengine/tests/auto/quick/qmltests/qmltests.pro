include(../tests.pri)

QT += qmltest

IMPORTPATH += $$PWD/data

OTHER_FILES += \
    $$PWD/data/TestWebEngineView.qml \
    $$PWD/data/alert.html \
    $$PWD/data/append-document-title.js \
    $$PWD/data/big-user-script.js \
    $$PWD/data/change-document-title.js \
    $$PWD/data/download.zip \
    $$PWD/data/confirm.html \
    $$PWD/data/confirmclose.html \
    $$PWD/data/directoryupload.html \
    $$PWD/data/favicon.html \
    $$PWD/data/favicon2.html \
    $$PWD/data/favicon-candidates-gray.html \
    $$PWD/data/favicon-misc.html \
    $$PWD/data/favicon-multi.html \
    $$PWD/data/favicon-multi-gray.html \
    $$PWD/data/favicon-single.html \
    $$PWD/data/favicon-shortcut.html \
    $$PWD/data/favicon-touch.html \
    $$PWD/data/favicon-unavailable.html \
    $$PWD/data/forms.html \
    $$PWD/data/geolocation.html \
    $$PWD/data/javascript.html \
    $$PWD/data/link.html \
    $$PWD/data/localStorage.html \
    $$PWD/data/prompt.html \
    $$PWD/data/multifileupload.html \
    $$PWD/data/redirect.html \
    $$PWD/data/singlefileupload.html \
    $$PWD/data/small-favicon.png \
    $$PWD/data/test1.html \
    $$PWD/data/test2.html \
    $$PWD/data/test3.html \
    $$PWD/data/test4.html \
    $$PWD/data/keyboardModifierMapping.html \
    $$PWD/data/keyboardEvents.html \
    $$PWD/data/titleupdate.js \
    $$PWD/data/tst_desktopBehaviorLoadHtml.qml \
    $$PWD/data/tst_download.qml \
    $$PWD/data/tst_favicon.qml \
    $$PWD/data/tst_faviconDownload.qml \
    $$PWD/data/tst_filePicker.qml \
    $$PWD/data/tst_findText.qml \
    $$PWD/data/tst_focusOnNavigation.qml \
    $$PWD/data/tst_formValidation.qml \
    $$PWD/data/tst_geopermission.qml \
    $$PWD/data/tst_javaScriptDialogs.qml \
    $$PWD/data/tst_linkHovered.qml \
    $$PWD/data/tst_loadFail.qml \
    $$PWD/data/tst_loadHtml.qml \
    $$PWD/data/tst_loadProgress.qml \
    $$PWD/data/tst_loadProgressSignal.qml \
    $$PWD/data/tst_loadRecursionCrash.qml \
    $$PWD/data/tst_loadUrl.qml \
    $$PWD/data/tst_navigationHistory.qml \
    $$PWD/data/tst_navigationRequested.qml \
    $$PWD/data/tst_newViewRequest.qml \
    $$PWD/data/tst_properties.qml \
    $$PWD/data/tst_runJavaScript.qml \
    $$PWD/data/tst_scrollPosition.qml \
    $$PWD/data/tst_titleChanged.qml \
    $$PWD/data/tst_unhandledKeyEventPropagation.qml \
    $$PWD/data/tst_userScripts.qml \
    $$PWD/data/tst_viewSource.qml \
    $$PWD/data/tst_webchannel.qml \
    $$PWD/data/tst_settings.qml \
    $$PWD/data/tst_keyboardModifierMapping.qml \
    $$PWD/data/tst_keyboardEvents.qml \
    $$PWD/data/icons/favicon.png \
    $$PWD/data/icons/gray128.png \
    $$PWD/data/icons/gray16.png \
    $$PWD/data/icons/gray255.png \
    $$PWD/data/icons/gray32.png \
    $$PWD/data/icons/gray64.png \
    $$PWD/data/icons/grayicons.ico \
    $$PWD/data/icons/small-favicon.png \
    $$PWD/data/icons/qt144.png \
    $$PWD/data/icons/qt32.ico \
    $$PWD/data/icons/qtmulti.ico \
    $$PWD/mock-delegates/QtWebEngine/UIDelegates/AlertDialog.qml \
    $$PWD/mock-delegates/QtWebEngine/UIDelegates/ConfirmDialog.qml \
    $$PWD/mock-delegates/QtWebEngine/UIDelegates/FilePicker.qml \
    $$PWD/mock-delegates/QtWebEngine/UIDelegates/PromptDialog.qml \
    $$PWD/mock-delegates/QtWebEngine/UIDelegates/qmldir \
    $$PWD/mock-delegates/TestParams/FilePickerParams.qml \
    $$PWD/mock-delegates/TestParams/JSDialogParams.qml \
    $$PWD/mock-delegates/TestParams/qmldir \

load(qt_build_paths)
DEFINES += QUICK_TEST_SOURCE_DIR=\\\"$$re_escape($$PWD$${QMAKE_DIR_SEP}data)\\\"

!isQMLTestSupportApiEnabled() {
    PLUGIN_EXTENSION = .so
    PLUGIN_PREFIX = lib
    osx: PLUGIN_PREFIX = .dylib
    win32 {
        PLUGIN_EXTENSION = .dll
        PLUGIN_PREFIX =
    }

    TESTSUPPORT_MODULE = $$shell_path($$[QT_INSTALL_QML]/QtWebEngine/testsupport/$${PLUGIN_PREFIX}qtwebenginetestsupportplugin$${PLUGIN_EXTENSION})
    BUILD_DIR = $$shell_path($$clean_path($$OUT_PWD/../../../..))
    SRC_DIR = $$shell_path($$clean_path($$PWD/../../../..))

    warning("QML Test Support API is disabled. This means some QML tests that use Test Support API will fail.")
    warning("Use the following command to build Test Support module and rebuild WebEngineView API:")
    warning("cd $$BUILD_DIR && qmake WEBENGINE_CONFIG+=testsupport -r $$shell_path($$SRC_DIR/qtwebengine.pro) && make -C $$shell_path($$BUILD_DIR/src/webengine) clean && make")
    warning("After performing the command above make sure QML module \"QtWebEngine.testsupport\" is deployed at $$TESTSUPPORT_MODULE")
}
