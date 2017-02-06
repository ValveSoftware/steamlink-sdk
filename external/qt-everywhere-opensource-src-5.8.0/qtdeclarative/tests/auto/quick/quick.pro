TEMPLATE = subdirs

PUBLICTESTS += \
    geometry \
    qquickpixmapcache

qtConfig(opengl(es1|es2)?) {
    PUBLICTESTS += \
        rendernode
    qtHaveModule(widgets): PUBLICTESTS += nodes

    QUICKTESTS += \
        qquickanimatedsprite \
        qquickframebufferobject \
        qquickopenglinfo \
        qquickspritesequence \
        qquickshadereffect
}

!cross_compile: PRIVATETESTS += examples

# This test requires the qtconcurrent module
!qtHaveModule(concurrent): PUBLICTESTS -= qquickpixmapcache

PRIVATETESTS += \
    nokeywords \
    qquickanimations \
    qquickapplication \
    qquickbehaviors \
    qquickfontloader \
    qquickfontloader_static \
    qquickfontmetrics \
    qquickimageprovider \
    qquicklayouts \
    qquickpath \
    qquicksmoothedanimation \
    qquickspringanimation \
    qquickanimationcontroller \
    qquickstyledtext \
    qquickstates \
    qquicksystempalette \
    qquicktimeline \
    qquickxmllistmodel

# This test requires the xmlpatterns module
!qtHaveModule(xmlpatterns): PRIVATETESTS -= qquickxmllistmodel

QUICKTESTS =  \
    qquickaccessible \
    qquickanchors \
    qquickanimatedimage \
    qquickdynamicpropertyanimation \
    qquickborderimage \
    qquickwindow \
    qquickdrag \
    qquickdroparea \
    qquickflickable \
    qquickflipable \
    qquickfocusscope \
    qquickgraphicsinfo \
    qquickgridview \
    qquickimage \
    qquickitem \
    qquickitem2 \
    qquickitemlayer \
    qquicklistview \
    qquickloader \
    qquickmousearea \
    qquickmultipointtoucharea \
    qquickpainteditem \
    qquickpathview \
    qquickpincharea \
    qquickpositioners \
    qquickrectangle \
    qquickrepeater \
    qquickshortcut \
    qquicktext \
    qquicktextdocument \
    qquicktextedit \
    qquicktextinput \
    qquickvisualdatamodel \
    qquickview \
    qquickcanvasitem \
    qquickdesignersupport \
    qquickscreen \
    touchmouse \
    scenegraph

SUBDIRS += $$PUBLICTESTS

!qtConfig(accessibility):QUICKTESTS -= qquickaccessible

qtConfig(private_tests) {
    SUBDIRS += $$PRIVATETESTS
    SUBDIRS += $$QUICKTESTS
}
