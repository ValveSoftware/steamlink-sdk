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

# QTBUG-60268
boot2qt: SUBDIRS -= inspectorserver qquickwebenginedefaultsurfaceformat qquickwebengineview
