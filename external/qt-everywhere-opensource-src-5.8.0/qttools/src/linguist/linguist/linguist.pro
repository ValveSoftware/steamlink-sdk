QT += core-private widgets xml uitools-private
qtHaveModule(printsupport): QT += printsupport

DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII

include(../shared/formats.pri)

QMAKE_DOCS = $$PWD/doc/qtlinguist.qdocconf

DEFINES += QFORMINTERNAL_NAMESPACE

SOURCES += \
    batchtranslationdialog.cpp \
    errorsview.cpp \
    finddialog.cpp \
    formpreviewview.cpp \
    globals.cpp \
    main.cpp \
    mainwindow.cpp \
    messageeditor.cpp \
    messageeditorwidgets.cpp \
    messagehighlighter.cpp \
    messagemodel.cpp \
    phrasebookbox.cpp \
    phrase.cpp \
    phrasemodel.cpp \
    phraseview.cpp \
    printout.cpp \
    recentfiles.cpp \
    sourcecodeview.cpp \
    statistics.cpp \
    translatedialog.cpp \
    translationsettingsdialog.cpp \
    ../shared/simtexth.cpp

HEADERS += \
    batchtranslationdialog.h \
    errorsview.h \
    finddialog.h \
    formpreviewview.h \
    globals.h \
    mainwindow.h \
    messageeditor.h \
    messageeditorwidgets.h \
    messagehighlighter.h \
    messagemodel.h \
    phrasebookbox.h \
    phrase.h \
    phrasemodel.h \
    phraseview.h \
    printout.h \
    recentfiles.h \
    sourcecodeview.h \
    statistics.h \
    translatedialog.h \
    translationsettingsdialog.h \
    ../shared/simtexth.h

contains(QT_PRODUCT, OpenSource.*):DEFINES *= QT_OPENSOURCE
DEFINES += QT_KEYWORDS
TARGET = linguist
win32:RC_FILE = linguist.rc
mac {
    static:CONFIG -= global_init_link_order
    ICON = linguist.icns
    TARGET = Linguist
    QMAKE_INFO_PLIST=Info_mac.plist
}
PROJECTNAME = Qt \
    Linguist

phrasebooks.path = $$[QT_INSTALL_DATA]/phrasebooks
# ## will this work on windows?
phrasebooks.files = $$PWD/../phrasebooks/*
INSTALLS += phrasebooks

FORMS += statistics.ui \
    phrasebookbox.ui \
    batchtranslation.ui \
    translatedialog.ui \
    mainwindow.ui \
    translationsettings.ui \
    finddialog.ui
RESOURCES += linguist.qrc

load(qt_app)
