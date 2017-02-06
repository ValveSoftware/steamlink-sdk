TEMPLATE = subdirs
SUBDIRS = cenumerator creaderasync creadersync cwriterasync cwritersync
!isEmpty(QT.widgets.name):SUBDIRS += enumerator terminal blockingmaster blockingslave master slave
