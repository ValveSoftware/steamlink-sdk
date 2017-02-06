TEMPLATE = subdirs
load(qfeatures)
qtHaveModule(widgets):!contains(QT_DISABLED_FEATURES, bearermanagement): SUBDIRS += bearercloud
