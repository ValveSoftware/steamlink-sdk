TEMPLATE = subdirs
SUBDIRS += testplugin controls activeFocusOnTab applicationwindow dialogs \
           extras qquicktreemodeladaptor customcontrolsstyle
controls.depends = testplugin

# QTBUG-60268
boot2qt: SUBDIRS -= controls activeFocusOnTab applicationwindow dialogs extras customcontrolsstyle
