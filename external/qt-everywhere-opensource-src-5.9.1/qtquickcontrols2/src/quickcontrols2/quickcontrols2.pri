HEADERS += \
    $$PWD/qquickanimatednode_p.h \
    $$PWD/qquickcolorimageprovider_p.h \
    $$PWD/qquickplaceholdertext_p.h \
    $$PWD/qquickproxytheme_p.h \
    $$PWD/qquickstyle.h \
    $$PWD/qquickstyle_p.h \
    $$PWD/qquickstyleattached_p.h \
    $$PWD/qquickstyleplugin_p.h \
    $$PWD/qquickstyleselector_p.h \
    $$PWD/qquickstyleselector_p_p.h \
    $$PWD/qquickpaddedrectangle_p.h

SOURCES += \
    $$PWD/qquickanimatednode.cpp \
    $$PWD/qquickcolorimageprovider.cpp \
    $$PWD/qquickplaceholdertext.cpp \
    $$PWD/qquickproxytheme.cpp \
    $$PWD/qquickstyle.cpp \
    $$PWD/qquickstyleattached.cpp \
    $$PWD/qquickstyleplugin.cpp \
    $$PWD/qquickstyleselector.cpp \
    $$PWD/qquickpaddedrectangle.cpp

qtConfig(quick-listview):qtConfig(quick-pathview) {
    HEADERS += \
        $$PWD/qquicktumblerview_p.h
    SOURCES += \
        $$PWD/qquicktumblerview.cpp
}
