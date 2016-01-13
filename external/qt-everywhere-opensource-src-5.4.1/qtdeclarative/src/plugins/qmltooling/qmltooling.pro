TEMPLATE = subdirs

SUBDIRS =  qmldbg_tcp
qtHaveModule(quick): SUBDIRS += qmldbg_qtquick2
