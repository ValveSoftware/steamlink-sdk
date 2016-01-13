TEMPLATE    = subdirs
SUBDIRS     = calculatorform

!static:SUBDIRS += calculatorbuilder \
                   containerextension \
                   customwidgetplugin \
                   taskmenuextension \
                   worldtimeclockbuilder \
                   worldtimeclockplugin

# the sun cc compiler has a problem with the include lines for the form.prf
solaris-cc*:SUBDIRS -= calculatorbuilder \
                       worldtimeclockbuilder

qtNomakeTools( \
    containerextension \
    customwidgetplugin \
    taskmenuextension \
    worldtimeclockbuilder \
    worldtimeclockplugin \
)
