TEMPLATE = subdirs

!uikit: SUBDIRS += qmlwebsockets qmlwebsockets_compat

# QTBUG-60268
boot2qt: SUBDIRS -= qmlwebsockets_compat
