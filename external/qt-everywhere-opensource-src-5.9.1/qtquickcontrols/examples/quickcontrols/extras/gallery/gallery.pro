TEMPLATE = app
TARGET = gallery
INCLUDEPATH += .
QT += quick

SOURCES += \
    main.cpp

RESOURCES += \
    gallery.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quickcontrols/extras/gallery
INSTALLS += target

OTHER_FILES += \
    qml/BlackButtonBackground.qml \
    qml/BlackButtonStyle.qml \
    qml/CircularGaugeDarkStyle.qml \
    qml/CircularGaugeDefaultStyle.qml \
    qml/CircularGaugeLightStyle.qml \
    qml/CircularGaugeView.qml \
    qml/ControlLabel.qml \
    qml/ControlView.qml \
    qml/ControlViewToolBar.qml \
    qml/CustomizerSwitch.qml \
    qml/CustomizerLabel.qml \
    qml/CustomizerSlider.qml \
    qml/FlickableMoreIndicator.qml \
    qml/gallery.qml \
    qml/PieMenuControlView.qml \
    qml/PieMenuDefaultStyle.qml \
    qml/PieMenuDarkStyle.qml \
    qml/StylePicker.qml \
    gallery.qrc
