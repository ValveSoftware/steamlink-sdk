TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS += script
qtHaveModule(widgets): {
    QT_FOR_CONFIG += widgets
    qtConfig(textedit): SUBDIRS += scripttools
}
