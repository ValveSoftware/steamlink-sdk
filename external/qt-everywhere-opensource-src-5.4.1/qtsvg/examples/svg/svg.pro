TEMPLATE = subdirs

qtHaveModule(widgets): SUBDIRS += embeddedsvgviewer  svggenerator  svgviewer
SUBDIRS += embedded richtext draganddrop network

qtHaveModule(opengl):!contains(QT_CONFIG, opengles2): SUBDIRS += opengl
