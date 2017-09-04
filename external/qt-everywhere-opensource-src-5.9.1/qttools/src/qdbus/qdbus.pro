TEMPLATE = subdirs
QT_FOR_CONFIG += xml
qtConfig(dom): SUBDIRS = qdbus
qtHaveModule(widgets): SUBDIRS += qdbusviewer
