TEMPLATE = subdirs

SUBDIRS += \
    inspectorserver \
    publicapi \
    qquickwebenginedefaultsurfaceformat \
    qquickwebengineview

isQMLTestSupportApiEnabled() {
    SUBDIRS += \
        qmltests \
        qquickwebengineviewgraphics
}
