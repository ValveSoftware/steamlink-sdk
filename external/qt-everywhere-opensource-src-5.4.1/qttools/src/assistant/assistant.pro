TEMPLATE = subdirs

SUBDIRS += clucene \
           help \
           assistant \
           qhelpgenerator \
           qcollectiongenerator \
           qhelpconverter

help.depends = clucene
assistant.depends = help
qhelpgenerator.depends = help
qcollectiongenerator.depends = help
qhelpconverter.depends = help

qtNomakeTools( \
    assistant \
    qhelpgenerator \
    qcollectiongenerator \
    qhelpconverter \
)
