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

qtConfig(private_tests): \
    SUBDIRS += $$PRIVATETESTS
