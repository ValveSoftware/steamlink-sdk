TEMPLATE=subdirs

qtHaveModule(webengine) {
    SUBDIRS += \
        webengine/customdialogs \
        webengine/minimal \
        webengine/quicknanobrowser
}

qtHaveModule(webenginewidgets) {
    SUBDIRS += \
        webenginewidgets/minimal \
        webenginewidgets/contentmanipulation \
        webenginewidgets/cookiebrowser \
        webenginewidgets/demobrowser \
        webenginewidgets/markdowneditor \
        webenginewidgets/simplebrowser

    contains(WEBENGINE_CONFIG, use_spellchecker):!cross_compile {
        !contains(WEBENGINE_CONFIG, use_native_spellchecker) {
            SUBDIRS += webenginewidgets/spellchecker
        } else {
            message("Spellcheck example will not be built because it depends on usage of Hunspell dictionaries.")
        }
    }

}
