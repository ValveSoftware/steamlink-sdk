TEMPLATE=subdirs

qtHaveModule(webengine) {
    SUBDIRS += webengine/quicknanobrowser
}

qtHaveModule(webenginewidgets) {
    SUBDIRS += \
        webenginewidgets/browser \
        webenginewidgets/fancybrowser
}
