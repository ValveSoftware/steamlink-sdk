SOURCES += \
    $$PWD/qquickapplication.cpp\
    $$PWD/qquickutilmodule.cpp\
    $$PWD/qquickanimation.cpp \
    $$PWD/qquicksystempalette.cpp \
    $$PWD/qquickspringanimation.cpp \
    $$PWD/qquicksmoothedanimation.cpp \
    $$PWD/qquickanimationcontroller.cpp \
    $$PWD/qquickstate.cpp\
    $$PWD/qquicktransitionmanager.cpp \
    $$PWD/qquickstatechangescript.cpp \
    $$PWD/qquickpropertychanges.cpp \
    $$PWD/qquickstategroup.cpp \
    $$PWD/qquicktransition.cpp \
    $$PWD/qquicktimeline.cpp \
    $$PWD/qquickpixmapcache.cpp \
    $$PWD/qquickbehavior.cpp \
    $$PWD/qquickfontloader.cpp \
    $$PWD/qquickstyledtext.cpp \
    $$PWD/qquickimageprovider.cpp \
    $$PWD/qquicksvgparser.cpp \
    $$PWD/qquickvaluetypes.cpp \
    $$PWD/qquickglobal.cpp \
    $$PWD/qquickanimator.cpp \
    $$PWD/qquickanimatorjob.cpp \
    $$PWD/qquickanimatorcontroller.cpp \
    $$PWD/qquickfontmetrics.cpp \
    $$PWD/qquicktextmetrics.cpp \
    $$PWD/qquickshortcut.cpp \
    $$PWD/qquickvalidator.cpp

!contains(QT_CONFIG, no-qml-debug): SOURCES += $$PWD/qquickprofiler.cpp

HEADERS += \
    $$PWD/qquickapplication_p.h\
    $$PWD/qquickutilmodule_p.h\
    $$PWD/qquickanimation_p.h \
    $$PWD/qquickanimation_p_p.h \
    $$PWD/qquicksystempalette_p.h \
    $$PWD/qquickspringanimation_p.h \
    $$PWD/qquickanimationcontroller_p.h \
    $$PWD/qquicksmoothedanimation_p.h \
    $$PWD/qquicksmoothedanimation_p_p.h \
    $$PWD/qquickstate_p.h\
    $$PWD/qquickstatechangescript_p.h \
    $$PWD/qquickpropertychanges_p.h \
    $$PWD/qquickstate_p_p.h\
    $$PWD/qquicktransitionmanager_p_p.h \
    $$PWD/qquickstategroup_p.h \
    $$PWD/qquicktransition_p.h \
    $$PWD/qquicktimeline_p_p.h \
    $$PWD/qquickpixmapcache_p.h \
    $$PWD/qquickbehavior_p.h \
    $$PWD/qquickfontloader_p.h \
    $$PWD/qquickstyledtext_p.h \
    $$PWD/qquickimageprovider.h \
    $$PWD/qquicksvgparser_p.h \
    $$PWD/qquickvaluetypes_p.h \
    $$PWD/qquickanimator_p.h \
    $$PWD/qquickanimator_p_p.h \
    $$PWD/qquickanimatorjob_p.h \
    $$PWD/qquickanimatorcontroller_p.h \
    $$PWD/qquickprofiler_p.h \
    $$PWD/qquickfontmetrics_p.h \
    $$PWD/qquicktextmetrics_p.h \
    $$PWD/qquickshortcut_p.h \
    $$PWD/qquickvalidator_p.h

qtConfig(quick-path) {
    SOURCES += \
        $$PWD/qquickpath.cpp \
        $$PWD/qquickpathinterpolator.cpp
    HEADERS += \
        $$PWD/qquickpath_p.h \
        $$PWD/qquickpath_p_p.h \
        $$PWD/qquickpathinterpolator_p.h
}
