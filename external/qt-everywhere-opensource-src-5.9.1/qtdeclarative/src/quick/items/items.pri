HEADERS += \
    $$PWD/qquickevents_p_p.h \
    $$PWD/qquickanchors_p.h \
    $$PWD/qquickanchors_p_p.h \
    $$PWD/qquickaccessibleattached_p.h \
    $$PWD/qquickitem.h \
    $$PWD/qquickitem_p.h \
    $$PWD/qquickitemchangelistener_p.h \
    $$PWD/qquickrectangle_p.h \
    $$PWD/qquickrectangle_p_p.h \
    $$PWD/qquickwindow.h \
    $$PWD/qquickwindow_p.h \
    $$PWD/qquickfocusscope_p.h \
    $$PWD/qquickitemsmodule_p.h \
    $$PWD/qquickpainteditem.h \
    $$PWD/qquickpainteditem_p.h \
    $$PWD/qquicktext_p.h \
    $$PWD/qquicktext_p_p.h \
    $$PWD/qquicktextnode_p.h \
    $$PWD/qquicktextnodeengine_p.h \
    $$PWD/qquicktextinput_p.h \
    $$PWD/qquicktextinput_p_p.h \
    $$PWD/qquicktextcontrol_p.h \
    $$PWD/qquicktextcontrol_p_p.h \
    $$PWD/qquicktextdocument.h \
    $$PWD/qquicktextdocument_p.h \
    $$PWD/qquicktextedit_p.h \
    $$PWD/qquicktextedit_p_p.h \
    $$PWD/qquicktextutil_p.h \
    $$PWD/qquickimagebase_p.h \
    $$PWD/qquickimagebase_p_p.h \
    $$PWD/qquickimage_p.h \
    $$PWD/qquickimage_p_p.h \
    $$PWD/qquickborderimage_p.h \
    $$PWD/qquickborderimage_p_p.h \
    $$PWD/qquickscalegrid_p_p.h \
    $$PWD/qquickmousearea_p.h \
    $$PWD/qquickmousearea_p_p.h \
    $$PWD/qquickpincharea_p.h \
    $$PWD/qquickpincharea_p_p.h \
    $$PWD/qquickflickable_p.h \
    $$PWD/qquickflickable_p_p.h \
    $$PWD/qquickflickablebehavior_p.h \
    $$PWD/qquickrepeater_p.h \
    $$PWD/qquickrepeater_p_p.h \
    $$PWD/qquickloader_p.h \
    $$PWD/qquickloader_p_p.h \
    $$PWD/qquicktranslate_p.h \
    $$PWD/qquickclipnode_p.h \
    $$PWD/qquickview.h \
    $$PWD/qquickview_p.h \
    $$PWD/qquickitemanimation_p.h \
    $$PWD/qquickitemanimation_p_p.h \
    $$PWD/qquickstateoperations_p.h \
    $$PWD/qquickimplicitsizeitem_p.h \
    $$PWD/qquickimplicitsizeitem_p_p.h \
    $$PWD/qquickdrag_p.h \
    $$PWD/qquickdroparea_p.h \
    $$PWD/qquickmultipointtoucharea_p.h \
    $$PWD/qquickscreen_p.h \
    $$PWD/qquickwindowattached_p.h \
    $$PWD/qquickwindowmodule_p.h \
    $$PWD/qquickrendercontrol.h \
    $$PWD/qquickrendercontrol_p.h \
    $$PWD/qquickgraphicsinfo_p.h \
    $$PWD/qquickitemgrabresult.h

SOURCES += \
    $$PWD/qquickevents.cpp \
    $$PWD/qquickanchors.cpp \
    $$PWD/qquickitem.cpp \
    $$PWD/qquickrectangle.cpp \
    $$PWD/qquickwindow.cpp \
    $$PWD/qquickfocusscope.cpp \
    $$PWD/qquickitemsmodule.cpp \
    $$PWD/qquickpainteditem.cpp \
    $$PWD/qquicktext.cpp \
    $$PWD/qquicktextnode.cpp \
    $$PWD/qquicktextnodeengine.cpp \
    $$PWD/qquicktextinput.cpp \
    $$PWD/qquicktextcontrol.cpp \
    $$PWD/qquicktextdocument.cpp \
    $$PWD/qquicktextedit.cpp \
    $$PWD/qquicktextutil.cpp \
    $$PWD/qquickimagebase.cpp \
    $$PWD/qquickimage.cpp \
    $$PWD/qquickborderimage.cpp \
    $$PWD/qquickscalegrid.cpp \
    $$PWD/qquickmousearea.cpp \
    $$PWD/qquickpincharea.cpp \
    $$PWD/qquickflickable.cpp \
    $$PWD/qquickrepeater.cpp \
    $$PWD/qquickloader.cpp \
    $$PWD/qquicktranslate.cpp \
    $$PWD/qquickclipnode.cpp \
    $$PWD/qquickview.cpp \
    $$PWD/qquickitemanimation.cpp \
    $$PWD/qquickstateoperations.cpp \
    $$PWD/qquickimplicitsizeitem.cpp \
    $$PWD/qquickaccessibleattached.cpp \
    $$PWD/qquickdrag.cpp \
    $$PWD/qquickdroparea.cpp \
    $$PWD/qquickmultipointtoucharea.cpp \
    $$PWD/qquickwindowmodule.cpp \
    $$PWD/qquickscreen.cpp \
    $$PWD/qquickwindowattached.cpp \
    $$PWD/qquickrendercontrol.cpp \
    $$PWD/qquickgraphicsinfo.cpp \
    $$PWD/qquickitemgrabresult.cpp

qtConfig(quick-animatedimage) {
    HEADERS += \
        $$PWD/qquickanimatedimage_p.h \
        $$PWD/qquickanimatedimage_p_p.h
    SOURCES += \
        $$PWD/qquickanimatedimage.cpp
}

qtConfig(quick-gridview) {
    HEADERS += \
        $$PWD/qquickgridview_p.h
    SOURCES += \
        $$PWD/qquickgridview.cpp
}

qtConfig(quick-itemview) {
    HEADERS += \
        $$PWD/qquickitemview_p.h \
        $$PWD/qquickitemview_p_p.h
    SOURCES += \
        $$PWD/qquickitemview.cpp
}

qtConfig(quick-viewtransitions) {
    HEADERS += \
        $$PWD/qquickitemviewtransition_p.h
    SOURCES += \
        $$PWD/qquickitemviewtransition.cpp
}

qtConfig(quick-listview) {
    HEADERS += \
        $$PWD/qquicklistview_p.h
    SOURCES += \
        $$PWD/qquicklistview.cpp
}

qtConfig(quick-pathview) {
    HEADERS += \
        $$PWD/qquickpathview_p.h \
        $$PWD/qquickpathview_p_p.h
    SOURCES += \
        $$PWD/qquickpathview.cpp
}

qtConfig(quick-positioners) {
    HEADERS += \
        $$PWD/qquickpositioners_p.h \
        $$PWD/qquickpositioners_p_p.h
    SOURCES += \
        $$PWD/qquickpositioners.cpp
}

qtConfig(quick-flipable) {
    HEADERS += \
        $$PWD/qquickflipable_p.h
    SOURCES += \
        $$PWD/qquickflipable.cpp
}

qtConfig(quick-shadereffect) {
    HEADERS += \
        $$PWD/qquickshadereffectsource_p.h \
        $$PWD/qquickshadereffectmesh_p.h \
        $$PWD/qquickshadereffect_p.h \
        $$PWD/qquickgenericshadereffect_p.h
    SOURCES += \
        $$PWD/qquickshadereffectsource.cpp \
        $$PWD/qquickshadereffectmesh.cpp \
        $$PWD/qquickshadereffect.cpp \
        $$PWD/qquickgenericshadereffect.cpp

    qtConfig(opengl) {
        SOURCES += \
            $$PWD/qquickopenglshadereffect.cpp \
            $$PWD/qquickopenglshadereffectnode.cpp
        HEADERS += \
            $$PWD/qquickopenglshadereffect_p.h \
            $$PWD/qquickopenglshadereffectnode_p.h

        OTHER_FILES += \
            $$PWD/shaders/shadereffect.vert \
            $$PWD/shaders/shadereffect.frag \
            $$PWD/shaders/shadereffectfallback.vert \
            $$PWD/shaders/shadereffectfallback.frag \
            $$PWD/shaders/shadereffect_core.vert \
            $$PWD/shaders/shadereffect_core.frag \
            $$PWD/shaders/shadereffectfallback_core.vert \
            $$PWD/shaders/shadereffectfallback_core.frag
    }
}

qtConfig(quick-sprite) {
    HEADERS += \
        $$PWD/qquickspriteengine_p.h \
        $$PWD/qquicksprite_p.h \
        $$PWD/qquickspritesequence_p.h \
        $$PWD/qquickanimatedsprite_p.h \
        $$PWD/qquickanimatedsprite_p_p.h \
        $$PWD/qquickspritesequence_p_p.h
    SOURCES += \
        $$PWD/qquickspriteengine.cpp \
        $$PWD/qquicksprite.cpp \
        $$PWD/qquickspritesequence.cpp \
        $$PWD/qquickanimatedsprite.cpp
}

# Items that depend on OpenGL Renderer
qtConfig(opengl(es1|es2)?) {
    SOURCES += \
        $$PWD/qquickopenglinfo.cpp \
        $$PWD/qquickframebufferobject.cpp

    HEADERS += \
        $$PWD/qquickopenglinfo_p.h \
        $$PWD/qquickframebufferobject.h
}

RESOURCES += \
    $$PWD/items.qrc

qtConfig(quick-canvas): \
    include(context2d/context2d.pri)
