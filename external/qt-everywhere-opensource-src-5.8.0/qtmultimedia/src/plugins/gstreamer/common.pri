QT += core-private multimedia-private network

qtHaveModule(widgets) {
    QT += widgets multimediawidgets-private
    DEFINES += HAVE_WIDGETS
}

LIBS += -lqgsttools_p

QMAKE_USE += gstreamer

qtConfig(resourcepolicy) {
    QMAKE_USE += libresourceqt5
    DEFINES += HAVE_RESOURCE_POLICY
}

qtConfig(gstreamer_app) {
    QMAKE_USE += gstreamer_app
    DEFINES += HAVE_GST_APPSRC
}

