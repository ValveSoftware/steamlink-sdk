TEMPLATE = app
TARGET = tableview

RESOURCES += \
    tableview.qrc

OTHER_FILES += \
    main.qml

include(src/src.pri)
include(../shared/shared.pri)

target.path = $$[QT_INSTALL_EXAMPLES]/quickcontrols/controls/tableview
INSTALLS += target
