QT += qml quick
TARGET = texteditor
!no_desktop: QT += widgets

include(src/src.pri)
include(../shared/shared.pri)

OTHER_FILES += \
    qml/main.qml \
    qml/ToolBarSeparator.qml

RESOURCES += \
    resources.qrc
