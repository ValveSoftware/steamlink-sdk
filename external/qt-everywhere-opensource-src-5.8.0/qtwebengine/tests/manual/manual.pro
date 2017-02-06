TEMPLATE = subdirs

SUBDIRS += \
    widgets \
    quick

!qtHaveModule(webenginewidgets): SUBDIRS -= widgets
