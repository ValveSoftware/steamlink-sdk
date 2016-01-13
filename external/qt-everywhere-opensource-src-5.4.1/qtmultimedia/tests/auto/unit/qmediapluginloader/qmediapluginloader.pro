CONFIG += testcase
TARGET = tst_qmediapluginloader

QT += multimedia-private testlib

SOURCES += tst_qmediapluginloader.cpp

wince* {
    PLUGIN_DEPLOY.sources = $$OUTPUT_DIR/plugins/mediaservice/*.dll
    PLUGIN_DEPLOY.path = mediaservice
    DEPLOYMENT += PLUGIN_DEPLOY
}

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
