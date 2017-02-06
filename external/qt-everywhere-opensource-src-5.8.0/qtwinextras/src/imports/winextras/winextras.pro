CXX_MODULE = qml
TARGET  = qml_winextras
TARGETPATH = QtWinExtras
IMPORT_VERSION = 1.0

QT += qml quick winextras winextras-private

DEFINES += QT_NO_FOREACH

QML_FILES += \
    JumpListLink.qml \
    JumpListDestination.qml \
    JumpListSeparator.qml

HEADERS += \
    qquickdwmfeatures_p.h \
    qquickdwmfeatures_p_p.h \
    qquicktaskbarbutton_p.h \
    qquickjumplist_p.h \
    qquickjumplistitem_p.h \
    qquickjumplistcategory_p.h \
    qquickthumbnailtoolbar_p.h \
    qquickthumbnailtoolbutton_p.h \
    qquickiconloader_p.h \
    qquickwin_p.h

SOURCES += \
    plugin.cpp \
    qquickdwmfeatures.cpp \
    qquicktaskbarbutton.cpp \
    qquickjumplist.cpp \
    qquickjumplistitem.cpp \
    qquickjumplistcategory.cpp \
    qquickthumbnailtoolbar.cpp \
    qquickthumbnailtoolbutton.cpp \
    qquickiconloader.cpp

OTHER_FILES += \
    qmldir \
    JumpListLink.qml \
    JumpListDestination.qml \
    JumpListSeparator.qml

qtConfig(dynamicgl):LIBS_PRIVATE += -luser32

load(qml_plugin)
