CONFIG += java
DESTDIR = $$[QT_INSTALL_PREFIX/get]/jar

PATHPREFIX = $$PWD/src/org/qtproject/qt5/android/bluetooth

JAVACLASSPATH += $$PWD/src/
JAVASOURCES += \
    $$PATHPREFIX/QtBluetoothBroadcastReceiver.java \
    $$PATHPREFIX/QtBluetoothSocketServer.java \
    $$PATHPREFIX/QtBluetoothInputStreamThread.java


# install
target.path = $$[QT_INSTALL_PREFIX]/jar
INSTALLS += target
