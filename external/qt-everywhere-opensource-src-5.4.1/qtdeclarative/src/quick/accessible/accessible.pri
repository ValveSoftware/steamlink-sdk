
QT += core-private gui-private  qml-private

#DEFINES+=Q_ACCESSIBLE_QUICK_ITEM_ENABLE_DEBUG_DESCRIPTION

SOURCES += \
    $$PWD/qqmlaccessible.cpp \
    $$PWD/qaccessiblequickview.cpp \
    $$PWD/qaccessiblequickitem.cpp \
    $$PWD/qquickaccessiblefactory.cpp \

HEADERS += \
    $$PWD/qqmlaccessible_p.h \
    $$PWD/qaccessiblequickview_p.h \
    $$PWD/qaccessiblequickitem_p.h \
    $$PWD/qquickaccessiblefactory_p.h \
