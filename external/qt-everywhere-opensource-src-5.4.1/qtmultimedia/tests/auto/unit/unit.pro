TEMPLATE = subdirs

SUBDIRS += multimedia.pro
qtHaveModule(widgets): SUBDIRS += multimediawidgets.pro
qtHaveModule(qml): SUBDIRS += multimediaqml.pro
