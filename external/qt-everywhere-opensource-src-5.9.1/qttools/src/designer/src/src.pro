TEMPLATE = subdirs

SUBDIRS = \
    uiplugin \
    uitools \
    lib \
    components \
    designer

contains(QT_CONFIG, shared): SUBDIRS += plugins

uitools.depends = uiplugin
lib.depends = uiplugin
components.depends = lib
designer.depends = components
plugins.depends = lib

qtNomakeTools( \
    lib \
    components \
    designer \
    plugins \
)
