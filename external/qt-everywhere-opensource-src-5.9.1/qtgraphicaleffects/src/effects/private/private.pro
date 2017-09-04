CXX_MODULE=qml
TARGETPATH=QtGraphicalEffects/private
TARGET = qtgraphicaleffectsprivate

QT += quick qml
# Needed to get a hold of QQuickShaderEffectSource, QQuickImage and QQuickItemPrivate
QT += quick-private gui-private core-private qml-private

SOURCES += plugin.cpp \
           qgfxshaderbuilder.cpp \
           qgfxsourceproxy.cpp

HEADERS += \
           qgfxsourceproxy_p.h \
           qgfxshaderbuilder_p.h

QML_FILES = \
    DropShadowBase.qml \
    FastGlow.qml \
    FastInnerShadow.qml \
    FastMaskedBlur.qml \
    GaussianDirectionalBlur.qml \
    GaussianGlow.qml \
    GaussianInnerShadow.qml \
    GaussianMaskedBlur.qml

!static: CONFIG += qmlcache

load(qml_plugin)
