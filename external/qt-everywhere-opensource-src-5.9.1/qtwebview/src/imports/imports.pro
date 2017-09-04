CXX_MODULE = webview
TARGET  = declarative_webview
TARGETPATH = QtWebView
IMPORT_VERSION = 1.1

QT += qml quick webview-private
SOURCES += \
    $$PWD/webview.cpp

QMLDIR_CONT = \
    "module QtWebView" \
    "plugin declarative_webview" \
    "typeinfo plugins.qmltypes" \
    "classname QWebViewModule"
qtHaveModule(webengine):QMLDIR_CONT += "depends QtWebEngine 1.0"

QMLDIR_FILE = $$_PRO_FILE_PWD_/qmldir
write_file($$QMLDIR_FILE, QMLDIR_CONT)|error("Aborting.")
load(qml_plugin)

OTHER_FILES += qmldir
