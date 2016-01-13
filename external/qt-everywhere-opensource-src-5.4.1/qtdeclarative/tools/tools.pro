TEMPLATE = subdirs
SUBDIRS += \
    qmlmin \
    qmlimportscanner

qmlmin.CONFIG = host_build
qmlimportscanner.CONFIG = host_build

!android|android_app {
    SUBDIRS += \
        qml \
        qmlprofiler \
        qmlbundle \
        qmllint
    qtHaveModule(quick) {
        !static: SUBDIRS += qmlscene qmlplugindump
        qtHaveModule(widgets): SUBDIRS += qmleasing
    }
    qtHaveModule(qmltest): SUBDIRS += qmltestrunner
    contains(QT_CONFIG, private_tests): SUBDIRS += qmljs
}

qml.depends = qmlimportscanner
qmleasing.depends = qmlimportscanner

# qmlmin, qmlimportscanner & qmlbundle are build tools.
# qmlscene is needed by the autotests.
# qmltestrunner may be useful for manual testing.
# qmlplugindump cannot be a build tool, because it loads target plugins.
# The other apps are mostly "desktop" tools and are thus excluded.
qtNomakeTools( \
    qmlprofiler \
    qmlplugindump \
    qmleasing \
)
