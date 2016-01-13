TEMPLATE = subdirs

PRIVATETESTS += \
    qquickage \
    qquickangleddirection \
    qquickcumulativedirection \
    qquickcustomaffector \
    qquickcustomparticle \
    qquickellipseextruder \
    qquickgroupgoal \
    qquickfriction \
    qquickgravity \
    qquickimageparticle \
    qquickitemparticle \
    qquicklineextruder \
    qquickmaskextruder \
    qquickparticlegroup \
    qquickparticlesystem \
    qquickpointattractor \
    qquickpointdirection \
    qquickrectangleextruder \
    qquickspritegoal \
    qquicktargetdirection \
    qquicktrailemitter \
    qquickturbulence \
    qquickwander

contains(QT_CONFIG, private_tests) {
    SUBDIRS += $$PRIVATETESTS
}
