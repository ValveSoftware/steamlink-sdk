TEMPLATE = subdirs

SUBDIRS = \
    uitools \
    lib \
    components \
    designer

contains(QT_CONFIG, shared): SUBDIRS += plugins

components.depends = lib
designer.depends = components
plugins.depends = lib

qtNomakeTools( \
    lib \
    components \
    designer \
    plugins \
)
