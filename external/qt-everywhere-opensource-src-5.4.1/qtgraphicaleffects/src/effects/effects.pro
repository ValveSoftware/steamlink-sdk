TARGETPATH = "QtGraphicalEffects"

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
    private/FastGlow.qml \
    private/FastInnerShadow.qml \
    private/FastMaskedBlur.qml \
    private/GaussianDirectionalBlur.qml \
    private/GaussianGlow.qml \
    private/GaussianInnerShadow.qml \
    private/GaussianMaskedBlur.qml \
    private/SourceProxy.qml

QMAKE_DOCS = $$PWD/doc/qtgraphicaleffects.qdocconf
load(qml_module)
