TEMPLATE = subdirs

qtHaveModule(widgets): SUBDIRS += embeddedsvgviewer  svggenerator  svgviewer
SUBDIRS += embedded richtext draganddrop network

qtHaveModule(opengl):!qtConfig(opengles2): SUBDIRS += opengl
