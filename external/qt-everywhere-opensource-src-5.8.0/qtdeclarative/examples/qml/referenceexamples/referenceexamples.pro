TEMPLATE = subdirs

SUBDIRS += \
    adding \
    coercion \
    default \
    properties \
    methods

qtHaveModule(widgets): SUBDIRS += extended

qtHaveModule(quick): SUBDIRS += \
    attached \
    binding \
    grouped \
    signal \
    valuesource
