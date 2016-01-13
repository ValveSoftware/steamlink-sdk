CONFIG += testcase
TARGET = tst_qquickitemlayer
SOURCES += tst_qquickitemlayer.cpp

macx:CONFIG -= app_bundle

TESTDATA = data/*

include(../../shared/util.pri)

CONFIG += parallel_test
QT += core-private gui-private  qml-private quick-private testlib

OTHER_FILES += \
    data/Smooth.qml \
    data/Enabled.qml \
    data/Mipmap.qml \
    data/Effect.qml \
    data/SourceRect.qml \
    data/TextureProvider.qml \
    data/Visible.qml \
    data/ZOrder.qml \
    data/ZOrderChange.qml \
    data/ToggleLayerAndEffect.qml \
    data/DisableLayer.qml \
    data/SamplerNameChange.qml \
    data/ItemEffect.qml \
    data/RectangleEffect.qml
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
