CXX_MODULE = qml
TARGET = qtgraphicaleffectsplugin
TARGETPATH = QtGraphicalEffects
QT += quick qml
IMPORT_VERSION = 1.0
QML_FILES = \
    Blend.qml \
    BrightnessContrast.qml \
    ColorOverlay.qml \
    Colorize.qml \
    ConicalGradient.qml \
    Desaturate.qml \
    DirectionalBlur.qml \
    Displace.qml \
    DropShadow.qml \
    FastBlur.qml \
    GammaAdjust.qml \
    GaussianBlur.qml \
    Glow.qml \
    HueSaturation.qml \
    InnerShadow.qml \
    LevelAdjust.qml \
    LinearGradient.qml \
    MaskedBlur.qml \
    OpacityMask.qml \
    RadialBlur.qml \
    RadialGradient.qml \
    RectangularGlow.qml \
    RecursiveBlur.qml \
    ThresholdMask.qml \
    ZoomBlur.qml \

QMAKE_DOCS = $$PWD/doc/qtgraphicaleffects.qdocconf
SOURCES += plugin.cpp
load(qml_plugin)
