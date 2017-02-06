CXX_MODULE = purchasing
TARGET  = declarative_purchasing
TARGETPATH = QtPurchasing
IMPORT_VERSION = 1.0

QT += qml quick purchasing purchasing-private
SOURCES += \
    $$PWD/inapppurchase.cpp \
    $$PWD/qinappproductqmltype.cpp \
    $$PWD/qinappstoreqmltype.cpp

win32 {
    CONFIG += skip_target_version_ext
    VERSION = $$MODULE_VERSION
    QMAKE_TARGET_PRODUCT = "Qt Purchasing (Qt $$QT_VERSION)"
    QMAKE_TARGET_DESCRIPTION = "Purchasing QML plugin for Qt."
}

HEADERS += \
    $$PWD/qinappproductqmltype_p.h \
    $$PWD/qinappstoreqmltype_p.h

OTHER_FILES += qmldir

load(qml_plugin)
