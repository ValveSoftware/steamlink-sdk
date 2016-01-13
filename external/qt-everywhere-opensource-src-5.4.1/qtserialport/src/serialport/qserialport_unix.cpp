/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
** Copyright (C) 2012 Andre Hartmann <aha_1980@gmx.de>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qserialport_unix_p.h"
#include "qserialportinfo_p.h"

#include <errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef Q_OS_MAC
#if defined (MAC_OS_X_VERSION_10_4) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_4)
#include <IOKit/serial/ioss.h>
#endif
#endif

#ifdef Q_OS_QNX
#define CRTSCTS (IHFLOW | OHFLOW)
#endif

#include <private/qcore_unix_p.h>

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qmap.h>

QT_BEGIN_NAMESPACE

QString serialPortLockFilePath(const QString &portName)
{
    static const QStringList lockDirectoryPaths = QStringList()
        << QStringLiteral("/var/lock")
        << QStringLiteral("/etc/locks")
        << QStringLiteral("/var/spool/locks")
        << QStringLiteral("/var/spool/uucp")
        << QStringLiteral("/tmp")
#ifdef Q_OS_ANDROID
        << QStringLiteral("/data/local/tmp")
#endif
    ;

    QString fileName = portName;
    fileName.replace(QLatin1Char('/'), QLatin1Char('_'));
    fileName.prepend(QStringLiteral("/LCK.."));

    QString lockFilePath;

    foreach (const QString &lockDirectoryPath, lockDirectoryPaths) {
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
        foreach (const QString &lockDirectoryPath, lockDirectoryPaths)
            qWarning("\t%s\n", qPrintable(lockDirectoryPath));
        return QString();
    }

    return lockFilePath;
}

class ReadNotifier : public QSocketNotifier
{
    Q_OBJECT
public:
    ReadNotifier(QSerialPortPrivate *d, QObject *parent)
        : QSocketNotifier(d->descriptor, QSocketNotifier::Read, parent)
        , dptr(d)
    {}

protected:
    bool event(QEvent *e) Q_DECL_OVERRIDE {
        bool ret = QSocketNotifier::event(e);
        if (ret)
            dptr->readNotification();
        return ret;
    }

private:
    QSerialPortPrivate *dptr;
};

class WriteNotifier : public QSocketNotifier
{
    Q_OBJECT
public:
    WriteNotifier(QSerialPortPrivate *d, QObject *parent)
        : QSocketNotifier(d->descriptor, QSocketNotifier::Write, parent)
        , dptr(d)
    {}

protected:
    bool event(QEvent *e) Q_DECL_OVERRIDE {
        bool ret = QSocketNotifier::event(e);
        if (ret)
            dptr->completeAsyncWrite();
        return ret;
    }

private:
    QSerialPortPrivate *dptr;
};

#include "qserialport_unix.moc"

QSerialPortPrivate::QSerialPortPrivate(QSerialPort *q)
    : QSerialPortPrivateData(q)
    , descriptor(-1)
    , readNotifier(Q_NULLPTR)
    , writeNotifier(Q_NULLPTR)
    , emittedReadyRead(false)
    , emittedBytesWritten(false)
    , pendingBytesWritten(0)
    , writeSequenceStarted(false)
{
}

bool QSerialPortPrivate::open(QIODevice::OpenMode mode)
{
    Q_Q(QSerialPort);

    QString lockFilePath = serialPortLockFilePath(QSerialPortInfoPrivate::portNameFromSystemLocation(systemLocation));
    bool isLockFileEmpty = lockFilePath.isEmpty();
    if (isLockFileEmpty) {
        qWarning("Failed to create a lock file for opening the device");
        q->setError(QSerialPort::PermissionError);
        return false;
    }

    QScopedPointer<QLockFile> newLockFileScopedPointer(new QLockFile(lockFilePath));

    if (!newLockFileScopedPointer->tryLock()) {
        q->setError(QSerialPort::PermissionError);
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
        q->setError(decodeSystemError());
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
    Q_Q(QSerialPort);

    if (settingsRestoredOnClose) {
        if (::tcsetattr(descriptor, TCSANOW, &restoredTermios) == -1)
            q->setError(decodeSystemError());
    }

#ifdef TIOCNXCL
    if (::ioctl(descriptor, TIOCNXCL) == -1)
        q->setError(decodeSystemError());
#endif

    if (readNotifier) {
        readNotifier->setEnabled(false);
        readNotifier->deleteLater();
        readNotifier = Q_NULLPTR;
    }

    if (writeNotifier) {
        writeNotifier->setEnabled(false);
        writeNotifier->deleteLater();
        writeNotifier = Q_NULLPTR;
    }

    if (qt_safe_close(descriptor) == -1)
        q->setError(decodeSystemError());

    lockFileScopedPointer.reset(Q_NULLPTR);

    descriptor = -1;
    pendingBytesWritten = 0;
    writeSequenceStarted = false;
}

QSerialPort::PinoutSignals QSerialPortPrivate::pinoutSignals()
{
    Q_Q(QSerialPort);

    int arg = 0;

    if (::ioctl(descriptor, TIOCMGET, &arg) == -1) {
        q->setError(decodeSystemError());
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
#elif defined TIOCM_CD
    if (arg & TIOCM_CD)
        ret |= QSerialPort::DataCarrierDetectSignal;
#endif
#ifdef TIOCM_RNG
    if (arg & TIOCM_RNG)
        ret |= QSerialPort::RingIndicatorSignal;
#elif defined TIOCM_RI
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
    Q_Q(QSerialPort);

    int status = TIOCM_DTR;
    if (::ioctl(descriptor, set ? TIOCMBIS : TIOCMBIC, &status) == -1) {
        q->setError(decodeSystemError());
        return false;
    }

    return true;
}

bool QSerialPortPrivate::setRequestToSend(bool set)
{
    Q_Q(QSerialPort);

    int status = TIOCM_RTS;
    if (::ioctl(descriptor, set ? TIOCMBIS : TIOCMBIC, &status) == -1) {
        q->setError(decodeSystemError());
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
    Q_Q(QSerialPort);

    if (::tcflush(descriptor, (directions == QSerialPort::AllDirections)
                     ? TCIOFLUSH : (directions & QSerialPort::Input) ? TCIFLUSH : TCOFLUSH) == -1) {
        q->setError(decodeSystemError());
        return false;
    }

    return true;
}

bool QSerialPortPrivate::sendBreak(int duration)
{
    Q_Q(QSerialPort);

    if (::tcsendbreak(descriptor, duration) == -1) {
        q->setError(decodeSystemError());
        return false;
    }

    return true;
}

bool QSerialPortPrivate::setBreakEnabled(bool set)
{
    Q_Q(QSerialPort);

    if (::ioctl(descriptor, set ? TIOCSBRK : TIOCCBRK) == -1) {
        q->setError(decodeSystemError());
        return false;
    }

    return true;
}

qint64 QSerialPortPrivate::readData(char *data, qint64 maxSize)
{
    return readBuffer.read(data, maxSize);
}

bool QSerialPortPrivate::waitForReadyRead(int msecs)
{
    QElapsedTimer stopWatch;
    stopWatch.start();

    do {
        bool readyToRead = false;
        bool readyToWrite = false;
        if (!waitForReadOrWrite(&readyToRead, &readyToWrite, true, !writeBuffer.isEmpty(),
                                timeoutValue(msecs, stopWatch.elapsed()))) {
            return false;
        }

        if (readyToRead)
            return readNotification();

        if (readyToWrite && !completeAsyncWrite())
            return false;
    } while (msecs == -1 || timeoutValue(msecs, stopWatch.elapsed()) > 0);
    return false;
}

bool QSerialPortPrivate::waitForBytesWritten(int msecs)
{
    if (writeBuffer.isEmpty() && pendingBytesWritten <= 0)
        return false;

    QElapsedTimer stopWatch;
    stopWatch.start();

    forever {
        bool readyToRead = false;
        bool readyToWrite = false;
        if (!waitForReadOrWrite(&readyToRead, &readyToWrite, true, !writeBuffer.isEmpty(),
                                timeoutValue(msecs, stopWatch.elapsed()))) {
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

QSerialPort::SerialPortError
QSerialPortPrivate::setBaudRate_helper(qint32 baudRate,
        QSerialPort::Directions directions)
{
    if ((directions & QSerialPort::Input) && ::cfsetispeed(&currentTermios, baudRate) < 0)
            return decodeSystemError();

    if ((directions & QSerialPort::Output) && ::cfsetospeed(&currentTermios, baudRate) < 0)
            return decodeSystemError();

    return QSerialPort::NoError;
}

#if defined(Q_OS_LINUX)

QSerialPort::SerialPortError
QSerialPortPrivate::setStandardBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    struct serial_struct currentSerialInfo;

    if ((::ioctl(descriptor, TIOCGSERIAL, &currentSerialInfo) != -1)
            && (currentSerialInfo.flags & ASYNC_SPD_CUST)) {
        currentSerialInfo.flags &= ~ASYNC_SPD_CUST;
        currentSerialInfo.custom_divisor = 0;
        if (::ioctl(descriptor, TIOCSSERIAL, &currentSerialInfo) == -1)
            return decodeSystemError();
    }

    return setBaudRate_helper(baudRate, directions);
}

#else

QSerialPort::SerialPortError
QSerialPortPrivate::setStandardBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    return setBaudRate_helper(baudRate, directions);
}

#endif

#if defined(Q_OS_LINUX)

QSerialPort::SerialPortError
QSerialPortPrivate::setCustomBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    Q_UNUSED(directions);

    struct serial_struct currentSerialInfo;

    if (::ioctl(descriptor, TIOCGSERIAL, &currentSerialInfo) == -1)
        return decodeSystemError();

    currentSerialInfo.flags &= ~ASYNC_SPD_MASK;
    currentSerialInfo.flags |= (ASYNC_SPD_CUST /* | ASYNC_LOW_LATENCY*/);
    currentSerialInfo.custom_divisor = currentSerialInfo.baud_base / baudRate;

    if (currentSerialInfo.custom_divisor == 0)
        return QSerialPort::UnsupportedOperationError;

    if (currentSerialInfo.custom_divisor * baudRate != currentSerialInfo.baud_base) {
        qWarning("Baud rate of serial port %s is set to %d instead of %d: divisor %f unsupported",
            qPrintable(systemLocation),
            currentSerialInfo.baud_base / currentSerialInfo.custom_divisor,
            baudRate, (float)currentSerialInfo.baud_base / baudRate);
    }

    if (::ioctl(descriptor, TIOCSSERIAL, &currentSerialInfo) == -1)
        return decodeSystemError();

    return setBaudRate_helper(B38400, directions);
}

#elif defined(Q_OS_MAC)

QSerialPort::SerialPortError
QSerialPortPrivate::setCustomBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    Q_UNUSED(directions);

#if defined (MAC_OS_X_VERSION_10_4) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_4)
    if (::ioctl(descriptor, IOSSIOSPEED, &baudRate) == -1)
        return decodeSystemError();

    return QSerialPort::NoError;
#endif

    return QSerialPort::UnsupportedOperationError;
}

#elif defined (Q_OS_QNX)

QSerialPort::SerialPortError
QSerialPortPrivate::setCustomBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    // On QNX, the values of the 'Bxxxx' constants are set to 'xxxx' (i.e.
    // B115200 is defined to '115200'), which means that literal values can be
    // passed to cfsetispeed/cfsetospeed, including custom values, provided
    // that the underlying hardware supports them.
    return setBaudRate_helper(baudRate, directions);
}

#else

QSerialPort::SerialPortError
QSerialPortPrivate::setCustomBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    Q_UNUSED(baudRate);
    Q_UNUSED(directions);

    return QSerialPort::UnsupportedOperationError;
}

#endif

bool QSerialPortPrivate::setBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    Q_Q(QSerialPort);

    if (baudRate <= 0) {
        q->setError(QSerialPort::UnsupportedOperationError);
        return false;
    }

    const qint32 unixBaudRate = QSerialPortPrivate::settingFromBaudRate(baudRate);

    const QSerialPort::SerialPortError error = (unixBaudRate > 0)
        ? setStandardBaudRate(unixBaudRate, directions)
        : setCustomBaudRate(baudRate, directions);

    if (error == QSerialPort::NoError)
        return updateTermios();

    q->setError(error);
    return false;
}

bool QSerialPortPrivate::setDataBits(QSerialPort::DataBits dataBits)
{
    currentTermios.c_cflag &= ~CSIZE;
    switch (dataBits) {
    case QSerialPort::Data5:
        currentTermios.c_cflag |= CS5;
        break;
    case QSerialPort::Data6:
        currentTermios.c_cflag |= CS6;
        break;
    case QSerialPort::Data7:
        currentTermios.c_cflag |= CS7;
        break;
    case QSerialPort::Data8:
        currentTermios.c_cflag |= CS8;
        break;
    default:
        currentTermios.c_cflag |= CS8;
        break;
    }
    return updateTermios();
}

bool QSerialPortPrivate::setParity(QSerialPort::Parity parity)
{
    currentTermios.c_iflag &= ~(PARMRK | INPCK);
    currentTermios.c_iflag |= IGNPAR;

    switch (parity) {

#ifdef CMSPAR
    // Here Installation parity only for GNU/Linux where the macro CMSPAR.
    case QSerialPort::SpaceParity:
        currentTermios.c_cflag &= ~PARODD;
        currentTermios.c_cflag |= PARENB | CMSPAR;
        break;
    case QSerialPort::MarkParity:
        currentTermios.c_cflag |= PARENB | CMSPAR | PARODD;
        break;
#endif
    case QSerialPort::NoParity:
        currentTermios.c_cflag &= ~PARENB;
        break;
    case QSerialPort::EvenParity:
        currentTermios.c_cflag &= ~PARODD;
        currentTermios.c_cflag |= PARENB;
        break;
    case QSerialPort::OddParity:
        currentTermios.c_cflag |= PARENB | PARODD;
        break;
    default:
        currentTermios.c_cflag |= PARENB;
        currentTermios.c_iflag |= PARMRK | INPCK;
        currentTermios.c_iflag &= ~IGNPAR;
        break;
    }

    return updateTermios();
}

bool QSerialPortPrivate::setStopBits(QSerialPort::StopBits stopBits)
{
    switch (stopBits) {
    case QSerialPort::OneStop:
        currentTermios.c_cflag &= ~CSTOPB;
        break;
    case QSerialPort::TwoStop:
        currentTermios.c_cflag |= CSTOPB;
        break;
    default:
        currentTermios.c_cflag &= ~CSTOPB;
        break;
    }
    return updateTermios();
}

bool QSerialPortPrivate::setFlowControl(QSerialPort::FlowControl flowControl)
{
    switch (flowControl) {
    case QSerialPort::NoFlowControl:
        currentTermios.c_cflag &= ~CRTSCTS;
        currentTermios.c_iflag &= ~(IXON | IXOFF | IXANY);
        break;
    case QSerialPort::HardwareControl:
        currentTermios.c_cflag |= CRTSCTS;
        currentTermios.c_iflag &= ~(IXON | IXOFF | IXANY);
        break;
    case QSerialPort::SoftwareControl:
        currentTermios.c_cflag &= ~CRTSCTS;
        currentTermios.c_iflag |= IXON | IXOFF | IXANY;
        break;
    default:
        currentTermios.c_cflag &= ~CRTSCTS;
        currentTermios.c_iflag &= ~(IXON | IXOFF | IXANY);
        break;
    }
    return updateTermios();
}

bool QSerialPortPrivate::setDataErrorPolicy(QSerialPort::DataErrorPolicy policy)
{
    tcflag_t parmrkMask = PARMRK;
#ifndef CMSPAR
    // in space/mark parity emulation also used PARMRK flag
    if (parity == QSerialPort::SpaceParity
            || parity == QSerialPort::MarkParity) {
        parmrkMask = 0;
    }
#endif //CMSPAR
    switch (policy) {
    case QSerialPort::SkipPolicy:
        currentTermios.c_iflag &= ~parmrkMask;
        currentTermios.c_iflag |= IGNPAR | INPCK;
        break;
    case QSerialPort::PassZeroPolicy:
        currentTermios.c_iflag &= ~(IGNPAR | parmrkMask);
        currentTermios.c_iflag |= INPCK;
        break;
    case QSerialPort::IgnorePolicy:
        currentTermios.c_iflag &= ~INPCK;
        break;
    case QSerialPort::StopReceivingPolicy:
        currentTermios.c_iflag &= ~IGNPAR;
        currentTermios.c_iflag |= parmrkMask | INPCK;
        break;
    default:
        currentTermios.c_iflag &= ~INPCK;
        break;
    }
    return updateTermios();
}

bool QSerialPortPrivate::readNotification()
{
    Q_Q(QSerialPort);

    // Always buffered, read data from the port into the read buffer
    qint64 newBytes = readBuffer.size();
    qint64 bytesToRead = policy == QSerialPort::IgnorePolicy ? ReadChunkSize : 1;

    if (readBufferMaxSize && bytesToRead > (readBufferMaxSize - readBuffer.size())) {
        bytesToRead = readBufferMaxSize - readBuffer.size();
        if (bytesToRead == 0) {
            // Buffer is full. User must read data from the buffer
            // before we can read more from the port.
            return false;
        }
    }

    char *ptr = readBuffer.reserve(bytesToRead);
    const qint64 readBytes = readFromPort(ptr, bytesToRead);

    if (readBytes <= 0) {
        QSerialPort::SerialPortError error = decodeSystemError();
        if (error != QSerialPort::ResourceError)
            error = QSerialPort::ReadError;
        else
            setReadNotificationEnabled(false);
        q->setError(error);
        readBuffer.chop(bytesToRead);
        return false;
    }

    readBuffer.chop(bytesToRead - qMax(readBytes, qint64(0)));

    newBytes = readBuffer.size() - newBytes;

    // If read buffer is full, disable the read port notifier.
    if (readBufferMaxSize && readBuffer.size() == readBufferMaxSize)
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
    Q_Q(QSerialPort);

    if (writeBuffer.isEmpty() || writeSequenceStarted)
        return true;

    // Attempt to write it all in one chunk.
    qint64 written = writeToPort(writeBuffer.readPointer(), writeBuffer.nextDataBlockSize());
    if (written < 0) {
        QSerialPort::SerialPortError error = decodeSystemError();
        if (error != QSerialPort::ResourceError)
            error = QSerialPort::WriteError;
        q->setError(error);
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
    Q_Q(QSerialPort);

#ifdef TIOCEXCL
    if (::ioctl(descriptor, TIOCEXCL) == -1)
        q->setError(decodeSystemError());
#endif

    if (::tcgetattr(descriptor, &restoredTermios) == -1) {
        q->setError(decodeSystemError());
        return false;
    }

    currentTermios = restoredTermios;
#ifdef Q_OS_SOLARIS
    currentTermios.c_iflag &= ~(IMAXBEL|IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
    currentTermios.c_oflag &= ~OPOST;
    currentTermios.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
    currentTermios.c_cflag &= ~(CSIZE|PARENB);
    currentTermios.c_cflag |= CS8;
#else
    ::cfmakeraw(&currentTermios);
#endif
    currentTermios.c_cflag |= CLOCAL;
    currentTermios.c_cc[VTIME] = 0;
    currentTermios.c_cc[VMIN] = 0;

    if (mode & QIODevice::ReadOnly)
        currentTermios.c_cflag |= CREAD;

    if (!updateTermios())
        return false;

    if (mode & QIODevice::ReadOnly)
        setReadNotificationEnabled(true);

    return true;
}

qint64 QSerialPortPrivate::bytesToWrite() const
{
    return writeBuffer.size();
}

qint64 QSerialPortPrivate::writeData(const char *data, qint64 maxSize)
{
    ::memcpy(writeBuffer.reserve(maxSize), data, maxSize);
    if (!writeBuffer.isEmpty() && !isWriteNotificationEnabled())
        setWriteNotificationEnabled(true);
    return maxSize;
}

bool QSerialPortPrivate::updateTermios()
{
    Q_Q(QSerialPort);

    if (::tcsetattr(descriptor, TCSANOW, &currentTermios) == -1) {
        q->setError(decodeSystemError());
        return false;
    }
    return true;
}

QSerialPort::SerialPortError QSerialPortPrivate::decodeSystemError() const
{
    QSerialPort::SerialPortError error;
    switch (errno) {
    case ENODEV:
        error = QSerialPort::DeviceNotFoundError;
        break;
#ifdef ENOENT
    case ENOENT:
        error = QSerialPort::DeviceNotFoundError;
        break;
#endif
    case EACCES:
        error = QSerialPort::PermissionError;
        break;
    case EBUSY:
        error = QSerialPort::PermissionError;
        break;
    case EAGAIN:
        error = QSerialPort::ResourceError;
        break;
    case EIO:
        error = QSerialPort::ResourceError;
        break;
    case EBADF:
        error = QSerialPort::ResourceError;
        break;
#ifdef Q_OS_MAC
    case ENXIO:
        error = QSerialPort::ResourceError;
        break;
#endif
#ifdef EINVAL
    case EINVAL:
        error = QSerialPort::UnsupportedOperationError;
        break;
#endif
#ifdef ENOIOCTLCMD
    case ENOIOCTLCMD:
        error = QSerialPort::UnsupportedOperationError;
        break;
#endif
#ifdef ENOTTY
    case ENOTTY:
        error = QSerialPort::UnsupportedOperationError;
        break;
#endif
#ifdef EPERM
    case EPERM:
        error = QSerialPort::PermissionError;
        break;
#endif
    default:
        error = QSerialPort::UnknownError;
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
    Q_Q(QSerialPort);

    Q_ASSERT(selectForRead);
    Q_ASSERT(selectForWrite);

    fd_set fdread;
    FD_ZERO(&fdread);
    if (checkRead)
        FD_SET(descriptor, &fdread);

    fd_set fdwrite;
    FD_ZERO(&fdwrite);
    if (checkWrite)
        FD_SET(descriptor, &fdwrite);

    struct timeval tv;
    tv.tv_sec = msecs / 1000;
    tv.tv_usec = (msecs % 1000) * 1000;

    const int ret = ::select(descriptor + 1, &fdread, &fdwrite, 0, msecs < 0 ? 0 : &tv);
    if (ret < 0) {
        q->setError(decodeSystemError());
        return false;
    }
    if (ret == 0) {
        q->setError(QSerialPort::TimeoutError);
        return false;
    }

    *selectForRead = FD_ISSET(descriptor, &fdread);
    *selectForWrite = FD_ISSET(descriptor, &fdwrite);
    return true;
}

qint64 QSerialPortPrivate::readFromPort(char *data, qint64 maxSize)
{
    qint64 bytesRead = 0;
#if defined (CMSPAR)
    if (parity == QSerialPort::NoParity
            || policy != QSerialPort::StopReceivingPolicy) {
#else
    if (parity != QSerialPort::MarkParity
            && parity != QSerialPort::SpaceParity) {
#endif
        bytesRead = qt_safe_read(descriptor, data, maxSize);
    } else {// Perform parity emulation.
        bytesRead = readPerChar(data, maxSize);
    }

    return bytesRead;
}

qint64 QSerialPortPrivate::writeToPort(const char *data, qint64 maxSize)
{
    qint64 bytesWritten = 0;
#if defined (CMSPAR)
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

static inline bool evenParity(quint8 c)
{
    c ^= c >> 4;        //(c7 ^ c3)(c6 ^ c2)(c5 ^ c1)(c4 ^ c0)
    c ^= c >> 2;        //[(c7 ^ c3)(c5 ^ c1)][(c6 ^ c2)(c4 ^ c0)]
    c ^= c >> 1;
    return c & 1;       //(c7 ^ c3)(c5 ^ c1)(c6 ^ c2)(c4 ^ c0)
}

#ifndef CMSPAR

qint64 QSerialPortPrivate::writePerChar(const char *data, qint64 maxSize)
{
    qint64 ret = 0;
    quint8 const charMask = (0xFF >> (8 - dataBits));

    while (ret < maxSize) {

        bool par = evenParity(*data & charMask);
        // False if need EVEN, true if need ODD.
        par ^= parity == QSerialPort::MarkParity;
        if (par ^ (currentTermios.c_cflag & PARODD)) { // Need switch parity mode?
            currentTermios.c_cflag ^= PARODD;
            flush(); //force sending already buffered data, because updateTermios() cleares buffers
            //todo: add receiving buffered data!!!
            if (!updateTermios())
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

qint64 QSerialPortPrivate::readPerChar(char *data, qint64 maxSize)
{
    Q_Q(QSerialPort);

    qint64 ret = 0;
    quint8 const charMask = (0xFF >> (8 - dataBits));

    // 0 - prefix not started,
    // 1 - received 0xFF,
    // 2 - received 0xFF and 0x00
    int prefix = 0;
    while (ret < maxSize) {

        qint64 r = qt_safe_read(descriptor, data, 1);
        if (r < 0) {
            if (errno == EAGAIN) // It is ok for nonblocking mode.
                break;
            return -1;
        }
        if (r == 0)
            break;

        bool par = true;
        switch (prefix) {
        case 2: // Previously received both 0377 and 0.
            par = false;
            prefix = 0;
            break;
        case 1: // Previously received 0377.
            if (*data == '\0') {
                ++prefix;
                continue;
            }
            prefix = 0;
            break;
        default:
            if (*data == '\377') {
                prefix = 1;
                continue;
            }
            break;
        }
        // Now: par contains parity ok or error, *data contains received character
        par ^= evenParity(*data & charMask); //par contains parity bit value for EVEN mode
        par ^= (currentTermios.c_cflag & PARODD); //par contains parity bit value for current mode
        if (par ^ (parity == QSerialPort::SpaceParity)) { //if parity error
            switch (policy) {
            case QSerialPort::SkipPolicy:
                continue;       //ignore received character
            case QSerialPort::StopReceivingPolicy:
                if (parity != QSerialPort::NoParity)
                    q->setError(QSerialPort::ParityError);
                else
                    q->setError(*data == '\0' ?
                                QSerialPort::BreakConditionError : QSerialPort::FramingError);
                return ++ret;   //abort receiving
                break;
            case QSerialPort::UnknownPolicy:
                // Unknown error policy is used! Falling back to PassZeroPolicy
            case QSerialPort::PassZeroPolicy:
                *data = '\0';   //replace received character by zero
                break;
            case QSerialPort::IgnorePolicy:
                break;          //ignore error and pass received character
            }
        }
        ++data;
        ++ret;
    }
    return ret;
}

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

qint32 QSerialPortPrivate::baudRateFromSetting(qint32 setting)
{
    return standardBaudRateMap().key(setting);
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
