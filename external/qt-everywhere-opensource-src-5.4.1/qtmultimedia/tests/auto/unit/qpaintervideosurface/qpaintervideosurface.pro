CONFIG += testcase
TARGET = tst_qpaintervideosurface

QT += multimedia-private multimediawidgets-private testlib
qtHaveModule(opengl) {
   QT += opengl
} else {
   DEFINES += QT_NO_OPENGL
}

SOURCES += tst_qpaintervideosurface.cpp

QT+=widgets
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

linux-*:system(". /etc/lsb-release && [ $DISTRIB_CODENAME = precise ]"):DEFINES+=UBUNTU_PRECISE_PANGOLIN

win32:contains(QT_CONFIG, angle): CONFIG += insignificant_test # QTBUG-28542
