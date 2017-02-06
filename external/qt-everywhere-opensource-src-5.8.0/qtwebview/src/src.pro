TEMPLATE = subdirs

android|ios|macos|winrt|qtHaveModule(webengine) {
    SUBDIRS += webview imports
    imports.depends = webview
}

android: SUBDIRS += jar
