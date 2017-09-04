TARGET = QtPurchasing
QT = core

QMAKE_TARGET_PRODUCT = "Qt Purchasing (Qt $$QT_VERSION)"
QMAKE_TARGET_DESCRIPTION = "Purchasing component for Qt."

QMAKE_DOCS = $$PWD/doc/qtpurchasing.qdocconf

ANDROID_BUNDLED_JAR_DEPENDENCIES = \
    jar/QtPurchasing-bundled.jar
ANDROID_JAR_DEPENDENCIES = \
    jar/QtPurchasing.jar

HEADERS += \
    $$PWD/qtpurchasingglobal.h

include(inapppurchase/inapppurchase.pri)

load(qt_module)
