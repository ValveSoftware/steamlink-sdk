# The code below handles deployment to Android and Maemo, aswell as copying
# of the application data to shadow build directories on desktop.
# It is recommended not to modify this file, since newer versions of Qt Creator
# may offer an updated version of it.

defineTest(qtcAddDeployment) {
for(deploymentfolder, DEPLOYMENTFOLDERS) {
    item = item$${deploymentfolder}
    greaterThan(QT_MAJOR_VERSION, 4) {
        itemsources = $${item}.files
    } else {
        itemsources = $${item}.sources
    }
    $$itemsources = $$eval($${deploymentfolder}.source)
    itempath = $${item}.path
    $$itempath= $$eval($${deploymentfolder}.target)
    export($$itemsources)
    export($$itempath)
    DEPLOYMENT += $$item
}

MAINPROFILEPWD = $$_PRO_FILE_PWD_

android {
    installPrefix = /assets
    x86 {
        target.path = /libs/x86
    } else: armeabi-v7a {
        target.path = /libs/armeabi-v7a
    } else {
        target.path = /libs/armeabi
    }
} else:maemo5 {
    installPrefix = /opt/$${TARGET}
    target.path = $${installPrefix}/bin
    desktopfile.files = $${TARGET}.desktop
    desktopfile.path = /usr/share/applications/hildon
    icon.files = $${TARGET}64.png
    icon.path = /usr/share/icons/hicolor/64x64/apps
} else:!isEmpty(MEEGO_VERSION_MAJOR) {
    installPrefix = /opt/$${TARGET}
    target.path = $${installPrefix}/bin
    desktopfile.files = $${TARGET}_harmattan.desktop
    desktopfile.path = /usr/share/applications
    icon.files = $${TARGET}80.png
    icon.path = /usr/share/icons/hicolor/80x80/apps
} else { # Assumed to be a Desktop Unix, Windows, or Mac
    installPrefix = $$desktopInstallPrefix
    target.path = $${installPrefix}
    sources.files = *.cpp *.h *.desktop *.png *.pro *.qml *.qmlproject *.svg *.qrc
    sources.path = $$desktopInstallPrefix
    export(sources.files)
    export(sources.path)
    INSTALLS += sources
    copyCommand =
    win32 {
        for(deploymentfolder, DEPLOYMENTFOLDERS) {
            source = $$MAINPROFILEPWD/$$eval($${deploymentfolder}.source)
            source = $$replace(source, /, \\)
            sourcePathSegments = $$split(source, \\)
            target = $$OUT_PWD/$$eval($${deploymentfolder}.target)/$$last(sourcePathSegments)
            target = $$replace(target, /, \\)
            target ~= s,\\\\\\.?\\\\,\\,
            !isEqual(source,$$target) {
                !isEmpty(copyCommand):copyCommand += &&
                isEqual(QMAKE_DIR_SEP, \\) {
                    copyCommand += $(COPY_DIR) \"$$source\" \"$$target\"
                } else {
                    source = $$replace(source, \\\\, /)
                    target = $$OUT_PWD/$$eval($${deploymentfolder}.target)
                    target = $$replace(target, \\\\, /)
                    copyCommand += test -d \"$$target\" || mkdir -p \"$$target\" && cp -r \"$$source\" \"$$target\"
                }
            }
        }
    } else {
        for(deploymentfolder, DEPLOYMENTFOLDERS) {
            source = $$MAINPROFILEPWD/$$eval($${deploymentfolder}.source)
            source = $$replace(source, \\\\, /)
            macx {
                target = $$OUT_PWD/$${TARGET}.app/Contents/Resources/$$eval($${deploymentfolder}.target)
            } else {
                target = $$OUT_PWD/$$eval($${deploymentfolder}.target)
            }
            target = $$replace(target, \\\\, /)
            sourcePathSegments = $$split(source, /)
            targetFullPath = $$target/$$last(sourcePathSegments)
            targetFullPath ~= s,/\\.?/,/,
            !isEqual(source,$$targetFullPath) {
                !isEmpty(copyCommand):copyCommand += &&
                copyCommand += $(MKDIR) \"$$target\"
                copyCommand += && $(COPY_DIR) \"$$source\" \"$$target\"
            }
        }
    }
    !isEmpty(copyCommand) {
        copyCommand = @echo Copying application data... && $$copyCommand
        copydeploymentfolders.commands = $$copyCommand
        first.depends = $(first) copydeploymentfolders
        export(first.depends)
        export(copydeploymentfolders.commands)
        QMAKE_EXTRA_TARGETS += first copydeploymentfolders
    }
}

for(deploymentfolder, DEPLOYMENTFOLDERS) {
    item = item$${deploymentfolder}
    itemfiles = $${item}.files
    $$itemfiles = $$eval($${deploymentfolder}.source)
    itempath = $${item}.path
    $$itempath = $${installPrefix}/$$eval($${deploymentfolder}.target)
    export($$itemfiles)
    export($$itempath)
    INSTALLS += $$item
}

!isEmpty(desktopfile.path) {
    export(icon.files)
    export(icon.path)
    export(desktopfile.files)
    export(desktopfile.path)
    INSTALLS += icon desktopfile
}

export(target.path)
INSTALLS += target

export (ICON)
export (INSTALLS)
export (DEPLOYMENT)
export (LIBS)
export (QMAKE_EXTRA_TARGETS)
}
