TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS += datavisualization

qtHaveModule(quick): SUBDIRS += datavisualizationqml2
