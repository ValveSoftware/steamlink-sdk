QT = core bluetooth bluetooth-private testlib

TARGET = tst_qlowenergycontroller-gattserver
CONFIG += testcase c++11

qtConfig(linux_crypto_api): DEFINES += CONFIG_LINUX_CRYPTO_API
qtConfig(bluez_le): DEFINES += CONFIG_BLUEZ_LE

SOURCES += tst_qlowenergycontroller-gattserver.cpp
