TEMPLATE = subdirs
SUBDIRS = auto
qtHaveModule(location):qtHaveModule(quick): SUBDIRS += plugins/declarativetestplugin
