TEMPLATE = subdirs

SUBDIRS += \
    common \
    featureextractor \
    activedtw \
    neuralnet \
    nn \
    preprocessing

activedtw.depends = common featureextractor
neuralnet.depends = common featureextractor
nn.depends = common featureextractor
preprocessing.depends = common
