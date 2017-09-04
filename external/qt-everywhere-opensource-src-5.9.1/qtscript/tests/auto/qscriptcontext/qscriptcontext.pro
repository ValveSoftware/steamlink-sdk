TARGET = tst_qscriptcontext
CONFIG += testcase
QT = core script testlib
SOURCES  += tst_qscriptcontext.cpp

# QTBUG-23463
linux-*:system(". /etc/lsb-release && [ $DISTRIB_CODENAME = oneiric ]"):DEFINES+=UBUNTU_ONEIRIC
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
