QT += qml quick
TARGET = workshop
!no_desktop: QT += widgets

include(src/src.pri)

INCLUDEPATH += ../../shared

OTHER_FILES += \
    main.qml \
    content/AboutDialog.qml \
    content/Controls.qml \
    content/ImageViewer.qml \
    content/ModelView.qml \
    content/Styles.qml

RESOURCES += \
    workshop.qrc
