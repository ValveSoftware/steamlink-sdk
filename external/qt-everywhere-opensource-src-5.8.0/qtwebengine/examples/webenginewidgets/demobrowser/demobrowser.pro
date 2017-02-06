TEMPLATE = app
TARGET = demobrowser
QT += webenginewidgets network widgets printsupport
CONFIG += c++11

qtHaveModule(uitools):!embedded: QT += uitools
else: DEFINES += QT_NO_UITOOLS

FORMS += \
    addbookmarkdialog.ui \
    bookmarks.ui \
    cookies.ui \
    cookiesexceptions.ui \
    downloaditem.ui \
    downloads.ui \
    history.ui \
    passworddialog.ui \
    printtopdfdialog.ui \
    proxy.ui \
    savepagedialog.ui \
    settings.ui

HEADERS += \
    autosaver.h \
    bookmarks.h \
    browserapplication.h \
    browsermainwindow.h \
    chasewidget.h \
    downloadmanager.h \
    edittableview.h \
    edittreeview.h \
    featurepermissionbar.h\
    fullscreennotification.h \
    history.h \
    modelmenu.h \
    printtopdfdialog.h \
    savepagedialog.h \
    searchlineedit.h \
    settings.h \
    squeezelabel.h \
    tabwidget.h \
    toolbarsearch.h \
    urllineedit.h \
    webview.h \
    xbel.h

SOURCES += \
    autosaver.cpp \
    bookmarks.cpp \
    browserapplication.cpp \
    browsermainwindow.cpp \
    chasewidget.cpp \
    downloadmanager.cpp \
    edittableview.cpp \
    edittreeview.cpp \
    featurepermissionbar.cpp\
    fullscreennotification.cpp \
    history.cpp \
    modelmenu.cpp \
    printtopdfdialog.cpp \
    savepagedialog.cpp \
    searchlineedit.cpp \
    settings.cpp \
    squeezelabel.cpp \
    tabwidget.cpp \
    toolbarsearch.cpp \
    urllineedit.cpp \
    webview.cpp \
    xbel.cpp \
    main.cpp

RESOURCES += data/data.qrc htmls/htmls.qrc

build_all:!build_pass {
    CONFIG -= build_all
    CONFIG += release
}

win32 {
   RC_FILE = demobrowser.rc
}

mac {
    ICON = demobrowser.icns
    QMAKE_INFO_PLIST = Info_mac.plist
    TARGET = Demobrowser
}

EXAMPLE_FILES = \
    Info_mac.plist demobrowser.icns demobrowser.ico demobrowser.rc \
    cookiejar.h cookiejar.cpp  # FIXME: these are currently unused.

# install
target.path = $$[QT_INSTALL_EXAMPLES]/webenginewidgets/demobrowser
INSTALLS += target
