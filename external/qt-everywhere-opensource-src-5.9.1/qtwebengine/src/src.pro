TEMPLATE = subdirs

process.depends = core
webengine.depends = core
webenginewidgets.depends = core webengine
webengine_plugin.subdir = webengine/plugin
webengine_plugin.target = sub-webengine-plugin
webengine_plugin.depends = webengine

core.depends = buildtools

SUBDIRS += buildtools \
           core \
           process \
           webengine \
           webengine_plugin \
           plugins


use?(spellchecker):!use?(native_spellchecker):!cross_compile {
    SUBDIRS += qwebengine_convert_dict
    qwebengine_convert_dict.subdir = tools/qwebengine_convert_dict
    qwebengine_convert_dict.depends = core
}

isQMLTestSupportApiEnabled() {
    webengine_testsupport_plugin.subdir = webengine/plugin/testsupport
    webengine_testsupport_plugin.target = sub-webengine-testsupport-plugin
    webengine_testsupport_plugin.depends = webengine
    SUBDIRS += webengine_testsupport_plugin
}

!contains(WEBENGINE_CONFIG, no_ui_delegates) {
    SUBDIRS += webengine/ui \
               webengine/ui2
}

qtHaveModule(widgets) {
   SUBDIRS += webenginewidgets
   plugins.depends = webenginewidgets
}
