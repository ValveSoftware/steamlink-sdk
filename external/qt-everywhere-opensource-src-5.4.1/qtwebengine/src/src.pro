TEMPLATE = subdirs

process.depends = core
webengine.depends = core
webenginewidgets.depends = core webengine
webengine_plugin.subdir = webengine/plugin
webengine_plugin.target = sub-webengine-plugin
webengine_plugin.depends = webengine
webengine_experimental_plugin.subdir = webengine/plugin/experimental
webengine_experimental_plugin.target = sub-webengine-experimental-plugin
webengine_experimental_plugin.depends = webengine


SUBDIRS += core \
           process \
           webengine \
           webengine_plugin \
           webengine_experimental_plugin

# FIXME: We probably want a bit more control over config options to tweak what to build/ship or not.
# Another example of where this could be necessary is to make it easy to build proprietery codecs support.
!contains(WEBENGINE_CONFIG, no_ui_delegates): SUBDIRS += webengine/ui

qtHaveModule(widgets) {
    SUBDIRS += webenginewidgets
}
