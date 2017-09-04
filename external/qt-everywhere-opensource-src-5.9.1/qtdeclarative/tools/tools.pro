TEMPLATE = subdirs
QT_FOR_CONFIG += qml-private
SUBDIRS += \
    qmlmin \
    qmlimportscanner

qtConfig(commandlineparser): SUBDIRS += qmlcachegen

!android|android_app {
    SUBDIRS += \
        qml \
        qmllint

    qtConfig(qml-profiler): SUBDIRS += qmlprofiler

    qtHaveModule(quick) {
        !static: {
            SUBDIRS += \
                qmlscene \
                qmltime

            qtConfig(regularexpression):qtConfig(process) {
                SUBDIRS += \
                    qmlplugindump
            }
        }
        qtHaveModule(widgets): SUBDIRS += qmleasing
    }
    qtHaveModule(qmltest): SUBDIRS += qmltestrunner
    qtConfig(private_tests): SUBDIRS += qmljs
}

qml.depends = qmlimportscanner
qmleasing.depends = qmlimportscanner

# qmlmin, qmlimportscanner & qmlcachegen are build tools.
# qmlscene is needed by the autotests.
# qmltestrunner may be useful for manual testing.
# qmlplugindump cannot be a build tool, because it loads target plugins.
# The other apps are mostly "desktop" tools and are thus excluded.
qtNomakeTools( \
    qmlprofiler \
    qmlplugindump \
    qmleasing \
)
