/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
** Copyright (C) 2012 Andre Hartmann <aha_1980@gmx.de>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qserialport_p.h"
#include "qserialportinfo_p.h"

#include <errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef Q_OS_OSX
#if defined(MAC_OS_X_VERSION_10_4) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_4)
#include <IOKit/serial/ioss.h>
#endif
#endif

#ifdef Q_OS_QNX
#define CRTSCTS (IHFLOW | OHFLOW)
#endif

#ifdef Q_OS_LINUX

# if !defined(Q_OS_ANDROID) || !defined(Q_PROCESSOR_X86)
struct termios2 {
    tcflag_t c_iflag;       /* input mode flags */
    tcflag_t c_oflag;       /* output mode flags */
    tcflag_t c_cflag;       /* control mode flags */
    tcflag_t c_lflag;       /* local mode flags */
    cc_t c_line;            /* line discipline */
    cc_t c_cc[19];          /* control characters */
    speed_t c_ispeed;       /* input speed */
    speed_t c_ospeed;       /* output speed */
};
# endif

#ifndef TCGETS2
#define TCGETS2     _IOR('T', 0x2A, struct termios2)
#endif

#ifndef TCSETS2
#define TCSETS2     _IOW('T', 0x2B, struct termios2)
#endif

#ifndef BOTHER
#define BOTHER      0010000
#endif

#endif

#include <private/qcore_unix_p.h>

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qmap.h>

#ifdef Q_OS_OSX
#include <QtCore/qstandardpaths.h>
#endif

QT_BEGIN_NAMESPACE

QString serialPortLockFilePath(const QString &portName)
{
    static const QStringList lockDirectoryPaths = QStringList()
        << QStringLiteral("/var/lock")
        << QStringLiteral("/etc/locks")
        << QStringLiteral("/var/spool/locks")
        << QStringLiteral("/var/spool/uucp")
        << QStringLiteral("/tmp")
        << QStringLiteral("/var/tmp")
        << QStringLiteral("/var/lock/lockdev")
        << QStringLiteral("/run/lock")
#ifdef Q_OS_ANDROID
        << QStringLiteral("/data/local/tmp")
#elif defined(Q_OS_OSX)
           // This is the workaround to specify a temporary directory
           // on OSX when running the App Sandbox feature.
        << QStandardPaths::writableLocation(QStandardPaths::TempLocation);
#endif
    ;

    QString fileName = portName;
    fileName.replace(QLatin1Char('/'), QLatin1Char('_'));
    fileName.prepend(QLatin1String("/LCK.."));

    QString lockFilePath;

    for (const QString &lockDirectoryPath : lockDirectoryPaths) {
        const QString filePath = lockDirectoryPath + fileName;

        QFileInfo lockDirectoryInfo(lockDirectoryPath);
        if (lockDirectoryInfo.isReadable()) {
            if (QFile::exists(filePath) || lockDirectoryInfo.isWritable()) {
                lockFilePath = filePath;
                break;
            }
        }
    }

    if (lockFilePath.isEmpty()) {
        qWarning("The following directories are not readable or writable for detaling with lock files\n");
        for (const QString &lockDirectoryPath : lockDirectoryPaths)
            qWarning("\t%s\n", qPrintable(lockDirectoryPath));
        return QString();
    }

    return lockFilePath;
}

class ReadNotifier : public QSocketNotifier
{
public:
    ReadNotifier(QSerialPortPrivate *d, QObject *parent)
        : QSocketNotifier(d->descriptor, QSocketNotifier::Read, parent)
        , dptr(d)
    {
    }

protected:
    bool event(QEvent *e) Q_DECL_OVERRIDE
    {
        if (e->type() == QEvent::SockAct) {
            dptr->readNotification();
            return true;
        }
        return QSocketNotifier::event(e);
    }

private:
    QSerialPortPrivate *dptr;
};

class WriteNotifier : public QSocketNotifier
{
public:
    WriteNotifier(QSerialPortPrivate *d, QObject *parent)
        : QSocketNotifier(d->descriptor, QSocketNotifier::Write, parent)
        , dptr(d)
    {
    }

protected:
    bool event(QEvent *e) Q_DECL_OVERRIDE
    {
        if (e->type() == QEvent::SockAct) {
            dptr->completeAsyncWrite();
            return true;
        }
        return QSocketNotifier::event(e);
    }

private:
    QSerialPortPrivate *dptr;
};

static inline void qt_set_common_props(termios *tio, QIODevice::OpenMode m)
{
#ifdef Q_OS_SOLARIS
    tio->c_iflag &= ~(IMAXBEL|IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
    tio->c_oflag &= ~OPOST;
    tio->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
    tio->c_cflag &= ~(CSIZE|PARENB);
    tio->c_cflag |= CS8;
#else
    ::cfmakeraw(tio);
#endif

    tio->c_cflag |= CLOCAL;
    tio->c_cc[VTIME] = 0;
    tio->c_cc[VMIN] = 0;

    if (m & QIODevice::ReadOnly)
        tio->c_cflag |= CREAD;
}

static inline void qt_set_databits(termios *tio, QSerialPort::DataBits databits)
{
    tio->c_cflag &= ~CSIZE;
    switch (databits) {
    case QSerialPort::Data5:
        tio->c_cflag |= CS5;
        break;
    case QSerialPort::Data6:
        tio->c_cflag |= CS6;
        break;
    case QSerialPort::Data7:
        tio->c_cflag |= CS7;
        break;
    case QSerialPort::Data8:
        tio->c_cflag |= CS8;
        break;
    default:
        tio->c_cflag |= CS8;
        break;
    }
}

static inline void qt_set_parity(termios *tio, QSerialPort::Parity parity)
{
    tio->c_iflag &= ~(PARMRK | INPCK);
    tio->c_iflag |= IGNPAR;

    switch (parity) {

#ifdef CMSPAR
    // Here Installation parity only for GNU/Linux where the macro CMSPAR.
    case QSerialPort::SpaceParity:
        tio->c_cflag &= ~PARODD;
        tio->c_cflag |= PARENB | CMSPAR;
        break;
    case QSerialPort::MarkParity:
        tio->c_cflag |= PARENB | CMSPAR | PARODD;
        break;
#endif
    case QSerialPort::NoParity:
        tio->c_cflag &= ~PARENB;
        break;
    case QSerialPort::EvenParity:
        tio->c_cflag &= ~PARODD;
        tio->c_cflag |= PARENB;
        break;
    case QSerialPort::OddParity:
        tio->c_cflag |= PARENB | PARODD;
        break;
    default:
        tio->c_cflag |= PARENB;
        tio->c_iflag |= PARMRK | INPCK;
        tio->c_iflag &= ~IGNPAR;
        break;
    }
}

static inline void qt_set_stopbits(termios *tio, QSerialPort::StopBits stopbits)
{
    switch (stopbits) {
    case QSerialPort::OneStop:
        tio->c_cflag &= ~CSTOPB;
        break;
    case QSerialPort::TwoStop:
        tio->c_cflag |= CSTOPB;
        break;
    default:
        tio->c_cflag &= ~CSTOPB;
        break;
    }
}

static inline void qt_set_flowcontrol(termios *tio, QSerialPort::FlowControl flowcontrol)
{
    switch (flowcontrol) {
    case QSerialPort::NoFlowControl:
        tio->c_cflag &= ~CRTSCTS;
        tio->c_iflag &= ~(IXON | IXOFF | IXANY);
        break;
    case QSerialPort::HardwareControl:
        tio->c_cflag |= CRTSCTS;
        tio->c_iflag &= ~(IXON | IXOFF | IXANY);
        break;
    case QSerialPort::SoftwareControl:
        tio->c_cflag &= ~CRTSCTS;
        tio->c_iflag |= IXON | IXOFF | IXANY;
        break;
    default:
        tio->c_cflag &= ~CRTSCTS;
        tio->c_iflag &= ~(IXON | IXOFF | IXANY);
        break;
    }
}

bool QSerialPortPrivate::open(QIODevice::OpenMode mode)
{
    QString lockFilePath = serialPortLockFilePath(QSerialPortInfoPrivate::portNameFromSystemLocation(systemLocation));
    bool isLockFileEmpty = lockFilePath.isEmpty();
    if (isLockFileEmpty) {
        qWarning("Failed to create a lock file for opening the device");
        setError(QSerialPortErrorInfo(QSerialPort::PermissionError, QSerialPort::tr("Permission error while creating lock file")));
        return false;
    }

    QScopedPointer<QLockFile> newLockFileScopedPointer(new QLockFile(lockFilePath));

    if (!newLockFileScopedPointer->tryLock()) {
        setError(QSerialPortErrorInfo(QSerialPort::PermissionError, QSerialPort::tr("Permission error while locking the device")));
        return false;
    }

    int flags = O_NOCTTY | O_NONBLOCK;

    switch (mode & QIODevice::ReadWrite) {
    case QIODevice::WriteOnly:
        flags |= O_WRONLY;
        break;
    case QIODevice::ReadWrite:
        flags |= O_RDWR;
        break;
    default:
        flags |= O_RDONLY;
        break;
    }

    descriptor = qt_safe_open(systemLocation.toLocal8Bit().constData(), flags);

    if (descriptor == -1) {
        setError(getSystemError());
        return false;
    }

    if (!initialize(mode)) {
        qt_safe_close(descriptor);
        return false;
    }

    lockFileScopedPointer.swap(newLockFileScopedPointer);

    return true;
}

void QSerialPortPrivate::close()
{
    if (settingsRestoredOnClose)
        ::tcsetattr(descriptor, TCSANOW, &restoredTermios);

#ifdef TIOCNXCL
    ::ioctl(descriptor, TIOCNXCL);
#endif

    delete readNotifier;
    readNotifier = nullptr;

    delete writeNotifier;
    writeNotifier = nullptr;

    qt_safe_close(descriptor);

    lockFileScopedPointer.reset(nullptr);

    descriptor = -1;
    pendingBytesWritten = 0;
    writeSequenceStarted = false;
}

QSerialPort::PinoutSignals QSerialPortPrivate::pinoutSignals()
{
    int arg = 0;

    if (::ioctl(descriptor, TIOCMGET, &arg) == -1) {
        setError(getSystemError());
        return QSerialPort::NoSignal;
    }

    QSerialPort::PinoutSignals ret = QSerialPort::NoSignal;

#ifdef TIOCM_LE
    if (arg & TIOCM_LE)
        ret |= QSerialPort::DataSetReadySignal;
#endif
#ifdef TIOCM_DTR
    if (arg & TIOCM_DTR)
        ret |= QSerialPort::DataTerminalReadySignal;
#endif
#ifdef TIOCM_RTS
    if (arg & TIOCM_RTS)
        ret |= QSerialPort::RequestToSendSignal;
#endif
#ifdef TIOCM_ST
    if (arg & TIOCM_ST)
        ret |= QSerialPort::SecondaryTransmittedDataSignal;
#endif
#ifdef TIOCM_SR
    if (arg & TIOCM_SR)
        ret |= QSerialPort::SecondaryReceivedDataSignal;
#endif
#ifdef TIOCM_CTS
    if (arg & TIOCM_CTS)
        ret |= QSerialPort::ClearToSendSignal;
#endif
#ifdef TIOCM_CAR
    if (arg & TIOCM_CAR)
        ret |= QSerialPort::DataCarrierDetectSignal;
#elif defined(TIOCM_CD)
    if (arg & TIOCM_CD)
        ret |= QSerialPort::DataCarrierDetectSignal;
#endif
#ifdef TIOCM_RNG
    if (arg & TIOCM_RNG)
        ret |= QSerialPort::RingIndicatorSignal;
#elif defined(TIOCM_RI)
    if (arg & TIOCM_RI)
        ret |= QSerialPort::RingIndicatorSignal;
#endif
#ifdef TIOCM_DSR
    if (arg & TIOCM_DSR)
        ret |= QSerialPort::DataSetReadySignal;
#endif

    return ret;
}

bool QSerialPortPrivate::setDataTerminalReady(bool set)
{
    int status = TIOCM_DTR;
    if (::ioctl(descriptor, set ? TIOCMBIS : TIOCMBIC, &status) == -1) {
        setError(getSystemError());
        return false;
    }

    return true;
}

bool QSerialPortPrivate::setRequestToSend(bool set)
{
    int status = TIOCM_RTS;
    if (::ioctl(descriptor, set ? TIOCMBIS : TIOCMBIC, &status) == -1) {
        setError(getSystemError());
        return false;
    }

    return true;
}

bool QSerialPortPrivate::flush()
{
    return completeAsyncWrite();
}

bool QSerialPortPrivate::clear(QSerialPort::Directions directions)
{
    if (::tcflush(descriptor, (directions == QSerialPort::AllDirections)
                     ? TCIOFLUSH : (directions & QSerialPort::Input) ? TCIFLUSH : TCOFLUSH) == -1) {
        setError(getSystemError());
        return false;
    }

    return true;
}

bool QSerialPortPrivate::sendBreak(int duration)
{
    if (::tcsendbreak(descriptor, duration) == -1) {
        setError(getSystemError());
        return false;
    }

    return true;
}

bool QSerialPortPrivate::setBreakEnabled(bool set)
{
    if (::ioctl(descriptor, set ? TIOCSBRK : TIOCCBRK) == -1) {
        setError(getSystemError());
        return false;
    }

    return true;
}

bool QSerialPortPrivate::waitForReadyRead(int msecs)
{
    QElapsedTimer stopWatch;
    stopWatch.start();

    do {
        bool readyToRead = false;
        bool readyToWrite = false;
        if (!waitForReadOrWrite(&readyToRead, &readyToWrite, true, !writeBuffer.isEmpty(),
                                qt_subtract_from_timeout(msecs, stopWatch.elapsed()))) {
            return false;
        }

        if (readyToRead)
            return readNotification();

        if (readyToWrite && !completeAsyncWrite())
            return false;
    } while (msecs == -1 || qt_subtract_from_timeout(msecs, stopWatch.elapsed()) > 0);
    return false;
}

bool QSerialPortPrivate::waitForBytesWritten(int msecs)
{
    if (writeBuffer.isEmpty() && pendingBytesWritten <= 0)
        return false;

    QElapsedTimer stopWatch;
    stopWatch.start();

    for (;;) {
        bool readyToRead = false;
        bool readyToWrite = false;
        if (!waitForReadOrWrite(&readyToRead, &readyToWrite, true, !writeBuffer.isEmpty(),
                                qt_subtract_from_timeout(msecs, stopWatch.elapsed()))) {
            return false;
        }

        if (readyToRead && !readNotification())
            return false;

        if (readyToWrite)
            return completeAsyncWrite();
    }
    return false;
}

bool QSerialPortPrivate::setBaudRate()
{
    if (inputBaudRate == outputBaudRate)
        return setBaudRate(inputBaudRate, QSerialPort::AllDirections);

    return (setBaudRate(inputBaudRate, QSerialPort::Input)
        && setBaudRate(outputBaudRate, QSerialPort::Output));
}

bool QSerialPortPrivate::setStandardBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
#ifdef Q_OS_LINUX
    // try to clear custom baud rate, using termios v2
    struct termios2 tio2;
    if (::ioctl(descriptor, TCGETS2, &tio2) != -1) {
        if (tio2.c_cflag & BOTHER) {
            tio2.c_cflag &= ~BOTHER;
            tio2.c_cflag |= CBAUD;
            ::ioctl(descriptor, TCSETS2, &tio2);
        }
    }

    // try to clear custom baud rate, using serial_struct (old way)
    struct serial_struct serial;
    ::memset(&serial, 0, sizeof(serial));
    if (::ioctl(descriptor, TIOCGSERIAL, &serial) != -1) {
        if (serial.flags & ASYNC_SPD_CUST) {
            serial.flags &= ~ASYNC_SPD_CUST;
            serial.custom_divisor = 0;
            // we don't check on errors because a driver can has not this feature
            ::ioctl(descriptor, TIOCSSERIAL, &serial);
        }
    }
#endif

    termios tio;
    if (!getTermios(&tio))
        return false;

    if ((directions & QSerialPort::Input) && ::cfsetispeed(&tio, baudRate) < 0) {
        setError(getSystemError());
        return false;
    }

    if ((directions & QSerialPort::Output) && ::cfsetospeed(&tio, baudRate) < 0) {
        setError(getSystemError());
        return false;
    }

    return setTermios(&tio);
}

#if defined(Q_OS_LINUX)

bool QSerialPortPrivate::setCustomBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    if (directions != QSerialPort::AllDirections) {
        setError(QSerialPortErrorInfo(QSerialPort::UnsupportedOperationError,
                                      QSerialPort::tr("Cannot set custom speed for one direction")));
        return false;
    }

    struct termios2 tio2;

    if (::ioctl(descriptor, TCGETS2, &tio2) != -1) {
        tio2.c_cflag &= ~CBAUD;
        tio2.c_cflag |= BOTHER;

        tio2.c_ispeed = baudRate;
        tio2.c_ospeed = baudRate;

        if (::ioctl(descriptor, TCSETS2, &tio2) != -1
                && ::ioctl(descriptor, TCGETS2, &tio2) != -1) {
            return true;
        }
    }

    struct serial_struct serial;

    if (::ioctl(descriptor, TIOCGSERIAL, &serial) == -1) {
        setError(getSystemError());
        return false;
    }

    serial.flags &= ~ASYNC_SPD_MASK;
    serial.flags |= (ASYNC_SPD_CUST /* | ASYNC_LOW_LATENCY*/);
    serial.custom_divisor = serial.baud_base / baudRate;

    if (serial.custom_divisor == 0) {
        setError(QSerialPortErrorInfo(QSerialPort::UnsupportedOperationError,
                                      QSerialPort::tr("No suitable custom baud rate divisor")));
        return false;
    }

    if (serial.custom_divisor * baudRate != serial.baud_base) {
        qWarning("Baud rate of serial port %s is set to %f instead of %d: divisor %f unsupported",
            qPrintable(systemLocation),
            float(serial.baud_base) / serial.custom_divisor,
            baudRate, float(serial.baud_base) / baudRate);
    }

    if (::ioctl(descriptor, TIOCSSERIAL, &serial) == -1) {
        setError(getSystemError());
        return false;
    }

    return setStandardBaudRate(B38400, directions);
}

#elif defined(Q_OS_OSX)

bool QSerialPortPrivate::setCustomBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    if (directions != QSerialPort::AllDirections) {
        setError(QSerialPortErrorInfo(QSerialPort::UnsupportedOperationError,
                                      QSerialPort::tr("Cannot set custom speed for one direction")));
        return false;
    }

#if defined(MAC_OS_X_VERSION_10_4) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_4)
    if (::ioctl(descriptor, IOSSIOSPEED, &baudRate) == -1) {
        setError(getSystemError());
        return false;
    }

    return true;
#else
    setError(QSerialPortErrorInfo(QSerialPort::UnsupportedOperationError,
                                  QSerialPort::tr("Custom baud rate is not supported")));
    return false;
#endif
}

#elif defined(Q_OS_QNX)

bool QSerialPortPrivate::setCustomBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    // On QNX, the values of the 'Bxxxx' constants are set to 'xxxx' (i.e.
    // B115200 is defined to '115200'), which means that literal values can be
    // passed to cfsetispeed/cfsetospeed, including custom values, provided
    // that the underlying hardware supports them.
    return setStandardBaudRate(baudRate, directions);
}

#else

bool QSerialPortPrivate::setCustomBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    Q_UNUSED(baudRate);
    Q_UNUSED(directions);

    setError(QSerialPortErrorInfo(QSerialPort::UnsupportedOperationError,
                                  QSerialPort::tr("Custom baud rate is not supported")));
    return false;
}

#endif

bool QSerialPortPrivate::setBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    if (baudRate <= 0) {
        setError(QSerialPortErrorInfo(QSerialPort::UnsupportedOperationError, QSerialPort::tr("Invalid baud rate value")));
        return false;
    }

    const qint32 unixBaudRate = QSerialPortPrivate::settingFromBaudRate(baudRate);

    return (unixBaudRate > 0)
            ? setStandardBaudRate(unixBaudRate, directions)
            : setCustomBaudRate(baudRate, directions);
}

bool QSerialPortPrivate::setDataBits(QSerialPort::DataBits dataBits)
{
    termios tio;
    if (!getTermios(&tio))
        return false;

    qt_set_databits(&tio, dataBits);

    return setTermios(&tio);
}

bool QSerialPortPrivate::setParity(QSerialPort::Parity parity)
{
    termios tio;
    if (!getTermios(&tio))
        return false;

    qt_set_parity(&tio, parity);

    return setTermios(&tio);
}

bool QSerialPortPrivate::setStopBits(QSerialPort::StopBits stopBits)
{
    termios tio;
    if (!getTermios(&tio))
        return false;

    qt_set_stopbits(&tio, stopBits);

    return setTermios(&tio);
}

bool QSerialPortPrivate::setFlowControl(QSerialPort::FlowControl flowControl)
{
    termios tio;
    if (!getTermios(&tio))
        return false;

    qt_set_flowcontrol(&tio, flowControl);

    return setTermios(&tio);
}

bool QSerialPortPrivate::readNotification()
{
    Q_Q(QSerialPort);

    // Always buffered, read data from the port into the read buffer
    qint64 newBytes = buffer.size();
    qint64 bytesToRead = ReadChunkSize;

    if (readBufferMaxSize && bytesToRead > (readBufferMaxSize - buffer.size())) {
        bytesToRead = readBufferMaxSize - buffer.size();
        if (bytesToRead == 0) {
            // Buffer is full. User must read data from the buffer
            // before we can read more from the port.
            setReadNotificationEnabled(false);
            return false;
        }
    }

    char *ptr = buffer.reserve(bytesToRead);
    const qint64 readBytes = readFromPort(ptr, bytesToRead);

    buffer.chop(bytesToRead - qMax(readBytes, qint64(0)));

    if (readBytes <= 0) {
        QSerialPortErrorInfo error = getSystemError();
        if (error.errorCode != QSerialPort::ResourceError)
            error.errorCode = QSerialPort::ReadError;
        else
            setReadNotificationEnabled(false);
        setError(error);
        return false;
    }

    newBytes = buffer.size() - newBytes;

    // If read buffer is full, disable the read port notifier.
    if (readBufferMaxSize && buffer.size() == readBufferMaxSize)
        setReadNotificationEnabled(false);

    // only emit readyRead() when not recursing, and only if there is data available
    const bool hasData = newBytes > 0;

    if (!emittedReadyRead && hasData) {
        emittedReadyRead = true;
        emit q->readyRead();
        emittedReadyRead = false;
    }

    return true;
}

bool QSerialPortPrivate::startAsyncWrite()
{
    if (writeBuffer.isEmpty() || writeSequenceStarted)
        return true;

    // Attempt to write it all in one chunk.
    qint64 written = writeToPort(writeBuffer.readPointer(), writeBuffer.nextDataBlockSize());
    if (written < 0) {
        QSerialPortErrorInfo error = getSystemError();
        if (error.errorCode != QSerialPort::ResourceError)
            error.errorCode = QSerialPort::WriteError;
        setError(error);
        return false;
    }

    writeBuffer.free(written);
    pendingBytesWritten += written;
    writeSequenceStarted = true;

    if (!isWriteNotificationEnabled())
        setWriteNotificationEnabled(true);
    return true;
}

bool QSerialPortPrivate::completeAsyncWrite()
{
    Q_Q(QSerialPort);

    if (pendingBytesWritten > 0) {
        if (!emittedBytesWritten) {
            emittedBytesWritten = true;
            emit q->bytesWritten(pendingBytesWritten);
            pendingBytesWritten = 0;
            emittedBytesWritten = false;
        }
    }

    writeSequenceStarted = false;

    if (writeBuffer.isEmpty()) {
        setWriteNotificationEnabled(false);
        return true;
    }

    return startAsyncWrite();
}

inline bool QSerialPortPrivate::initialize(QIODevice::OpenMode mode)
{
#ifdef TIOCEXCL
    if (::ioctl(descriptor, TIOCEXCL) == -1)
        setError(getSystemError());
#endif

    termios tio;
    if (!getTermios(&tio))
        return false;

    restoredTermios = tio;

    qt_set_common_props(&tio, mode);
    qt_set_databits(&tio, dataBits);
    qt_set_parity(&tio, parity);
    qt_set_stopbits(&tio, stopBits);
    qt_set_flowcontrol(&tio, flowControl);

    if (!setTermios(&tio))
        return false;

    if (!setBaudRate())
        return false;

    if (mode & QIODevice::ReadOnly)
        setReadNotificationEnabled(true);

    return true;
}

qint64 QSerialPortPrivate::writeData(const char *data, qint64 maxSize)
{
    writeBuffer.append(data, maxSize);
    if (!writeBuffer.isEmpty() && !isWriteNotificationEnabled())
        setWriteNotificationEnabled(true);
    return maxSize;
}

bool QSerialPortPrivate::setTermios(const termios *tio)
{
    if (::tcsetattr(descriptor, TCSANOW, tio) == -1) {
        setError(getSystemError());
        return false;
    }
    return true;
}

bool QSerialPortPrivate::getTermios(termios *tio)
{
    ::memset(tio, 0, sizeof(termios));
    if (::tcgetattr(descriptor, tio) == -1) {
        setError(getSystemError());
        return false;
    }
    return true;
}

QSerialPortErrorInfo QSerialPortPrivate::getSystemError(int systemErrorCode) const
{
    if (systemErrorCode == -1)
        systemErrorCode = errno;

    QSerialPortErrorInfo error;
    error.errorString = qt_error_string(systemErrorCode);

    switch (systemErrorCode) {
    case ENODEV:
        error.errorCode = QSerialPort::DeviceNotFoundError;
        break;
#ifdef ENOENT
    case ENOENT:
        error.errorCode = QSerialPort::DeviceNotFoundError;
        break;
#endif
    case EACCES:
        error.errorCode = QSerialPort::PermissionError;
        break;
    case EBUSY:
        error.errorCode = QSerialPort::PermissionError;
        break;
    case EAGAIN:
        error.errorCode = QSerialPort::ResourceError;
        break;
    case EIO:
        error.errorCode = QSerialPort::ResourceError;
        break;
    case EBADF:
        error.errorCode = QSerialPort::ResourceError;
        break;
#ifdef Q_OS_OSX
    case ENXIO:
        error.errorCode = QSerialPort::ResourceError;
        break;
#endif
#ifdef EINVAL
    case EINVAL:
        error.errorCode = QSerialPort::UnsupportedOperationError;
        break;
#endif
#ifdef ENOIOCTLCMD
    case ENOIOCTLCMD:
        error.errorCode = QSerialPort::UnsupportedOperationError;
        break;
#endif
#ifdef ENOTTY
    case ENOTTY:
        error.errorCode = QSerialPort::UnsupportedOperationError;
        break;
#endif
#ifdef EPERM
    case EPERM:
        error.errorCode = QSerialPort::PermissionError;
        break;
#endif
    default:
        error.errorCode = QSerialPort::UnknownError;
        break;
    }
    return error;
}

bool QSerialPortPrivate::isReadNotificationEnabled() const
{
    return readNotifier && readNotifier->isEnabled();
}

void QSerialPortPrivate::setReadNotificationEnabled(bool enable)
{
    Q_Q(QSerialPort);

    if (readNotifier) {
        readNotifier->setEnabled(enable);
    } else if (enable) {
        readNotifier = new ReadNotifier(this, q);
        readNotifier->setEnabled(true);
    }
}

bool QSerialPortPrivate::isWriteNotificationEnabled() const
{
    return writeNotifier && writeNotifier->isEnabled();
}

void QSerialPortPrivate::setWriteNotificationEnabled(bool enable)
{
    Q_Q(QSerialPort);

    if (writeNotifier) {
        writeNotifier->setEnabled(enable);
    } else if (enable) {
        writeNotifier = new WriteNotifier(this, q);
        writeNotifier->setEnabled(true);
    }
}

bool QSerialPortPrivate::waitForReadOrWrite(bool *selectForRead, bool *selectForWrite,
                                           bool checkRead, bool checkWrite,
                                           int msecs)
{
    Q_ASSERT(selectForRead);
    Q_ASSERT(selectForWrite);

    pollfd pfd = qt_make_pollfd(descriptor, 0);

    if (checkRead)
        pfd.events |= POLLIN;

    if (checkWrite)
        pfd.events |= POLLOUT;

    const int ret = qt_poll_msecs(&pfd, 1, msecs);
    if (ret < 0) {
        setError(getSystemError());
        return false;
    }
    if (ret == 0) {
        setError(QSerialPortErrorInfo(QSerialPort::TimeoutError));
        return false;
    }
    if (pfd.revents & POLLNVAL) {
        setError(getSystemError(EBADF));
        return false;
    }

    *selectForWrite = ((pfd.revents & POLLOUT) != 0);
    *selectForRead = ((pfd.revents & POLLIN) != 0);
    return true;
}

qint64 QSerialPortPrivate::readFromPort(char *data, qint64 maxSize)
{
    return qt_safe_read(descriptor, data, maxSize);
}

qint64 QSerialPortPrivate::writeToPort(const char *data, qint64 maxSize)
{
    qint64 bytesWritten = 0;
#if defined(CMSPAR)
    bytesWritten = qt_safe_write(descriptor, data, maxSize);
#else
    if (parity != QSerialPort::MarkParity
            && parity != QSerialPort::SpaceParity) {
        bytesWritten = qt_safe_write(descriptor, data, maxSize);
    } else {// Perform parity emulation.
        bytesWritten = writePerChar(data, maxSize);
    }
#endif

    return bytesWritten;
}

#ifndef CMSPAR

static inline bool evenParity(quint8 c)
{
    c ^= c >> 4;        //(c7 ^ c3)(c6 ^ c2)(c5 ^ c1)(c4 ^ c0)
    c ^= c >> 2;        //[(c7 ^ c3)(c5 ^ c1)][(c6 ^ c2)(c4 ^ c0)]
    c ^= c >> 1;
    return c & 1;       //(c7 ^ c3)(c5 ^ c1)(c6 ^ c2)(c4 ^ c0)
}

qint64 QSerialPortPrivate::writePerChar(const char *data, qint64 maxSize)
{
    termios tio;
    if (!getTermios(&tio))
        return -1;

    qint64 ret = 0;
    quint8 const charMask = (0xFF >> (8 - dataBits));

    while (ret < maxSize) {

        bool par = evenParity(*data & charMask);
        // False if need EVEN, true if need ODD.
        par ^= parity == QSerialPort::MarkParity;
        if (par ^ (tio.c_cflag & PARODD)) { // Need switch parity mode?
            tio.c_cflag ^= PARODD;
            flush(); //force sending already buffered data, because setTermios(&tio); cleares buffers
            //todo: add receiving buffered data!!!
            if (!setTermios(&tio))
                break;
        }

        int r = qt_safe_write(descriptor, data, 1);
        if (r < 0)
            return -1;
        if (r > 0) {
            data += r;
            ret += r;
        }
    }
    return ret;
}

#endif //CMSPAR

typedef QMap<qint32, qint32> BaudRateMap;

// The OS specific defines can be found in termios.h

static const BaudRateMap createStandardBaudRateMap()
{
    BaudRateMap baudRateMap;

#ifdef B50
    baudRateMap.insert(50, B50);
#endif

#ifdef B75
    baudRateMap.insert(75, B75);
#endif

#ifdef B110
    baudRateMap.insert(110, B110);
#endif

#ifdef B134
    baudRateMap.insert(134, B134);
#endif

#ifdef B150
    baudRateMap.insert(150, B150);
#endif

#ifdef B200
    baudRateMap.insert(200, B200);
#endif

#ifdef B300
    baudRateMap.insert(300, B300);
#endif

#ifdef B600
    baudRateMap.insert(600, B600);
#endif

#ifdef B1200
    baudRateMap.insert(1200, B1200);
#endif

#ifdef B1800
    baudRateMap.insert(1800, B1800);
#endif

#ifdef B2400
    baudRateMap.insert(2400, B2400);
#endif

#ifdef B4800
    baudRateMap.insert(4800, B4800);
#endif

#ifdef B7200
    baudRateMap.insert(7200, B7200);
#endif

#ifdef B9600
    baudRateMap.insert(9600, B9600);
#endif

#ifdef B14400
    baudRateMap.insert(14400, B14400);
#endif

#ifdef B19200
    baudRateMap.insert(19200, B19200);
#endif

#ifdef B28800
    baudRateMap.insert(28800, B28800);
#endif

#ifdef B38400
    baudRateMap.insert(38400, B38400);
#endif

#ifdef B57600
    baudRateMap.insert(57600, B57600);
#endif

#ifdef B76800
    baudRateMap.insert(76800, B76800);
#endif

#ifdef B115200
    baudRateMap.insert(115200, B115200);
#endif

#ifdef B230400
    baudRateMap.insert(230400, B230400);
#endif

#ifdef B460800
    baudRateMap.insert(460800, B460800);
#endif

#ifdef B500000
    baudRateMap.insert(500000, B500000);
#endif

#ifdef B576000
    baudRateMap.insert(576000, B576000);
#endif

#ifdef B921600
    baudRateMap.insert(921600, B921600);
#endif

#ifdef B1000000
    baudRateMap.insert(1000000, B1000000);
#endif

#ifdef B1152000
    baudRateMap.insert(1152000, B1152000);
#endif

#ifdef B1500000
    baudRateMap.insert(1500000, B1500000);
#endif

#ifdef B2000000
    baudRateMap.insert(2000000, B2000000);
#endif

#ifdef B2500000
    baudRateMap.insert(2500000, B2500000);
#endif

#ifdef B3000000
    baudRateMap.insert(3000000, B3000000);
#endif

#ifdef B3500000
    baudRateMap.insert(3500000, B3500000);
#endif

#ifdef B4000000
    baudRateMap.insert(4000000, B4000000);
#endif

    return baudRateMap;
}

static const BaudRateMap& standardBaudRateMap()
{
    static const BaudRateMap baudRateMap = createStandardBaudRateMap();
    return baudRateMap;
}

qint32 QSerialPortPrivate::settingFromBaudRate(qint32 baudRate)
{
    return standardBaudRateMap().value(baudRate);
}

QList<qint32> QSerialPortPrivate::standardBaudRates()
{
    return standardBaudRateMap().keys();
}

QSerialPort::Handle QSerialPort::handle() const
{
    Q_D(const QSerialPort);
    return d->descriptor;
}

QT_END_NAMESPACE
