TEMPLATE = app

QT += qml quick sql
CONFIG += c++11

HEADERS += sqlcontactmodel.h \
    sqlconversationmodel.h

SOURCES += main.cpp \
    sqlcontactmodel.cpp \
    sqlconversationmodel.cpp

RESOURCES += \
    qml.qrc \
    ../shared/shared.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quickcontrols2/chattutorial/chapter4-models
INSTALLS += target
