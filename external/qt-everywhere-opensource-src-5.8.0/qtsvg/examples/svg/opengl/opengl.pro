TEMPLATE = subdirs
qtHaveModule(widgets):!qtConfig(opengles2):!qtConfig(dynamicgl): SUBDIRS += framebufferobject
