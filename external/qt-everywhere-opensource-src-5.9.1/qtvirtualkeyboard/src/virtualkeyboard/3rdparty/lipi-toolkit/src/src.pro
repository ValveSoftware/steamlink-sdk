TEMPLATE = subdirs

SUBDIRS += \
    common \
    util \
    reco \
    lipiengine

util.depends = common
reco.depends = util
lipiengine.depends = common util reco
