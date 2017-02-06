TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS += script
qtHaveModule(widgets): SUBDIRS += scripttools
