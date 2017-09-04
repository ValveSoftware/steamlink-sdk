TEMPLATE = subdirs

sub_localsockettestserver.subdir = localsockettestserver
sub_localsockettestserver.target = sub-localsockettestserver

sub_integration.subdir = integration
sub_integration.target = sub-integration
sub_integration.depends = sub-localsockettestserver

SUBDIRS += benchmarks repc sub_integration integration_multiprocess modelview cmake pods repcodegenerator repparser \
           sub_localsockettestserver \
           modelreplica

# QTBUG-60268
boot2qt: SUBDIRS -= sub_integration modelview
