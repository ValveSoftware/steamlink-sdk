QT += qml quick
TARGET = basiclayouts
!no_desktop: QT += widgets

include(src/src.pri)
include(../shared/shared.pri)

OTHER_FILES += \
    main.qml

RESOURCES += \
    resources.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quickcontrols/controls/basiclayouts
INSTALLS += target
