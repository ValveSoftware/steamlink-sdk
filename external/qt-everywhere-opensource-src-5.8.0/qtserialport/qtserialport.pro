lessThan(QT_MAJOR_VERSION, 5) {
    message("Cannot build current QtSerialPort sources with Qt version $${QT_VERSION}.")
    error("Use at least Qt 5.0.0 or try to download QtSerialPort for Qt4.")
}

requires(!winrt)
requires(!ios)

load(configure)
qtCompileTest(ntddmodm)

load(qt_parts)
