
QT += core-private gui-private  qml-private

#DEFINES+=Q_ACCESSIBLE_QUICK_ITEM_ENABLE_DEBUG_DESCRIPTION

SOURCES += \
    $$PWD/qaccessiblequickview.cpp \
    $$PWD/qaccessiblequickitem.cpp \
    $$PWD/qquickaccessiblefactory.cpp \

HEADERS += \
    $$PWD/qaccessiblequickview_p.h \
    $$PWD/qaccessiblequickitem_p.h \
    $$PWD/qquickaccessiblefactory_p.h \
