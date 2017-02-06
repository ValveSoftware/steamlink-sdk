TEMPLATE = subdirs

qtHaveModule(widgets) {
    no-png {
        message("Some graphics-related tools are unavailable without PNG support")
    } else {
        SUBDIRS = assistant \
                  pixeltool \
                  designer

        linguist.depends = designer
    }
}

SUBDIRS += linguist \
    qdoc \
    qtattributionsscanner

!android|android_app: SUBDIRS += qtplugininfo

if(!android|android_app):!uikit: SUBDIRS += qtpaths

mac {
    SUBDIRS += macdeployqt
}

android {
    SUBDIRS += androiddeployqt
}

qtHaveModule(dbus): SUBDIRS += qdbus

win32|winrt:SUBDIRS += windeployqt
winrt:SUBDIRS += winrtrunner
qtHaveModule(gui):!android:!uikit:!qnx:!winrt: SUBDIRS += qtdiag

qtNomakeTools( \
    pixeltool \
    macdeployqt \
)
