QT += qml quick
!no_desktop: QT += widgets

QT += quick qml
SOURCES += main.cpp
include(../../controls/shared/shared.pri)

OTHER_FILES += \
    systemdialogs.qml \
    FileDialogs.qml \
    ColorDialogs.qml \
    FontDialogs.qml \
    MessageDialogs.qml \
    CustomDialogs.qml

target.path = $$[QT_INSTALL_EXAMPLES]/quickcontrols/dialogs/systemdialogs
INSTALLS += target

RESOURCES += systemdialogs.qrc
