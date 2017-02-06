TEMPLATE = subdirs
SUBDIRS += \
    quicktemplates2 \
    quickcontrols2 \
    imports

quickcontrols2.depends = quicktemplates2
imports.depends = quickcontrols2 quicktemplates2
