TEMPLATE = subdirs
qtHaveModule(widgets):!contains(QT_CONFIG, opengles2):!contains(QT_CONFIG, dynamicgl): SUBDIRS += framebufferobject
