/****************************************************************************
**
** Copyright (C) 2014 Denis Shienkov <denis.shienkov@gmail.com>
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

#include <QtTest/QtTest>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

Q_DECLARE_METATYPE(QSerialPort::SerialPortError);
Q_DECLARE_METATYPE(QIODevice::OpenMode);
Q_DECLARE_METATYPE(QIODevice::OpenModeFlag);
Q_DECLARE_METATYPE(Qt::ConnectionType);

class tst_QSerialPort : public QObject
{
    Q_OBJECT
public:
    explicit tst_QSerialPort();

    static void enterLoop(int secs)
    {
        ++loopLevel;
        QTestEventLoop::instance().enterLoop(secs);
        --loopLevel;
    }

    static void enterLoopMsecs(int msecs)
    {
        ++loopLevel;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
        QTestEventLoop::instance().enterLoopMSecs(msecs);
#else
        Q_UNUSED(msecs);
        QTestEventLoop::instance().enterLoop(1);
#endif
        --loopLevel;
    }

    static void exitLoop()
    {
        if (loopLevel > 0)
            QTestEventLoop::instance().exitLoop();
    }

    static bool timeout()
    {
        return QTestEventLoop::instance().timeout();
    }

private slots:
    void initTestCase();

    void defaultConstruct();
    void constructByName();
    void constructByInfo();

    void openExisting_data();
    void openExisting();
    void openNotExisting_data();
    void openNotExisting();

    void flush();
    void doubleFlush();

    void waitForBytesWritten();

    void waitForReadyReadWithTimeout();
    void waitForReadyReadWithOneByte();
    void waitForReadyReadWithAlphabet();

    void twoStageSynchronousLoopback();

    void synchronousReadWrite();

    void asynchronousWriteByBytesWritten_data();
    void asynchronousWriteByBytesWritten();

    void asynchronousWriteByTimer_data();
    void asynchronousWriteByTimer();

#ifdef Q_OS_WIN
    void readBufferOverflow();
    void readAfterInputClear();
#endif

protected slots:
    void handleBytesWrittenAndExitLoopSlot(qint64 bytesWritten);
    void handleBytesWrittenAndExitLoopSlot2(qint64 bytesWritten);

private:
    void clearReceiver(const QString &customReceiverName = QString());

    QString m_senderPortName;
    QString m_receiverPortName;
    QStringList m_availablePortNames;

    static int loopLevel;
    static const QByteArray alphabetArray;
    static const QByteArray newlineArray;
};

int tst_QSerialPort::loopLevel = 0;

const QByteArray tst_QSerialPort::alphabetArray("ABCDEFGHIJKLMNOPQRSTUVWXUZ");
const QByteArray tst_QSerialPort::newlineArray("\n\r");

tst_QSerialPort::tst_QSerialPort()
{
}

// This method is a workaround for the "com0com" virtual serial port
// driver or for the SOCAT utility. The problem is that the close/clear
// methods have no effect on sender serial port. If any data didn't manage
// to be transferred before closing, then this data will continue to be
// transferred at next opening of sender port.
// Thus, this behavior influences other tests and leads to the wrong results
// (e.g. the receiver port on other test can receive some data which are
// not expected). It is recommended to use this method for cleaning of
// read FIFO of receiver for those tests in which reception of data is
// required.
void tst_QSerialPort::clearReceiver(const QString &customReceiverName)
{
    QSerialPort receiver(customReceiverName.isEmpty()
                         ? m_receiverPortName : customReceiverName);
    if (receiver.open(QIODevice::ReadOnly))
        enterLoopMsecs(100);
}

void tst_QSerialPort::initTestCase()
{
    m_senderPortName = QString::fromLocal8Bit(qgetenv("QTEST_SERIALPORT_SENDER"));
    m_receiverPortName = QString::fromLocal8Bit(qgetenv("QTEST_SERIALPORT_RECEIVER"));
    if (m_senderPortName.isEmpty() || m_receiverPortName.isEmpty()) {
        static const char message[] =
              "Test doesn't work because the names of serial ports aren't found in env.\n"
              "Please set environment variables:\n"
              " QTEST_SERIALPORT_SENDER to name of output serial port\n"
              " QTEST_SERIALPORT_RECEIVER to name of input serial port\n"
              "Specify short names of port"
#if defined(Q_OS_UNIX)
              ", like: ttyS0\n";
#elif defined(Q_OS_WIN32)
              ", like: COM1\n";
#else
              "\n";
#endif

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
        QSKIP(message);
#else
        QSKIP(message, SkipAll);
#endif
    } else {
        m_availablePortNames << m_senderPortName << m_receiverPortName;
    }
}

void tst_QSerialPort::defaultConstruct()
{
    QSerialPort serialPort;

    QCOMPARE(serialPort.error(), QSerialPort::NoError);
    QVERIFY(!serialPort.errorString().isEmpty());

    // properties
    const qint32 defaultBaudRate = static_cast<qint32>(QSerialPort::Baud9600);
    QCOMPARE(serialPort.baudRate(), defaultBaudRate);
    QCOMPARE(serialPort.baudRate(QSerialPort::Input), defaultBaudRate);
    QCOMPARE(serialPort.baudRate(QSerialPort::Output), defaultBaudRate);
    QCOMPARE(serialPort.dataBits(), QSerialPort::Data8);
    QCOMPARE(serialPort.parity(), QSerialPort::NoParity);
    QCOMPARE(serialPort.stopBits(), QSerialPort::OneStop);
    QCOMPARE(serialPort.flowControl(), QSerialPort::NoFlowControl);

    QCOMPARE(serialPort.pinoutSignals(), QSerialPort::NoSignal);
    QCOMPARE(serialPort.isRequestToSend(), false);
    QCOMPARE(serialPort.isDataTerminalReady(), false);

    // QIODevice
    QCOMPARE(serialPort.openMode(), QIODevice::NotOpen);
    QVERIFY(!serialPort.isOpen());
    QVERIFY(!serialPort.isReadable());
    QVERIFY(!serialPort.isWritable());
    QVERIFY(serialPort.isSequential());
    QCOMPARE(serialPort.canReadLine(), false);
    QCOMPARE(serialPort.pos(), qlonglong(0));
    QCOMPARE(serialPort.size(), qlonglong(0));
    QVERIFY(serialPort.atEnd());
    QCOMPARE(serialPort.bytesAvailable(), qlonglong(0));
    QCOMPARE(serialPort.bytesToWrite(), qlonglong(0));

    char c;
    QCOMPARE(serialPort.read(&c, 1), qlonglong(-1));
    QCOMPARE(serialPort.write(&c, 1), qlonglong(-1));
}

void tst_QSerialPort::constructByName()
{
    QSerialPort serialPort(m_senderPortName);
    QCOMPARE(serialPort.portName(), m_senderPortName);
    serialPort.setPortName(m_receiverPortName);
    QCOMPARE(serialPort.portName(), m_receiverPortName);
}

void tst_QSerialPort::constructByInfo()
{
    QSerialPortInfo senderPortInfo(m_senderPortName);
    QSerialPortInfo receiverPortInfo(m_receiverPortName);

#if defined(Q_OS_UNIX)
    if (senderPortInfo.isNull() || receiverPortInfo.isNull()) {
        static const char message[] =
                "Test doesn't work because the specified serial ports aren't"
                " found in system and can't be constructed by QSerialPortInfo.\n";
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
        QSKIP(message);
#else
        QSKIP(message, SkipAll);
#endif
    }
#endif

    QSerialPort serialPort(senderPortInfo);
    QCOMPARE(serialPort.portName(), m_senderPortName);
    serialPort.setPort(receiverPortInfo);
    QCOMPARE(serialPort.portName(), m_receiverPortName);
}

void tst_QSerialPort::openExisting_data()
{
    QTest::addColumn<int>("openMode");
    QTest::addColumn<bool>("openResult");
    QTest::addColumn<QSerialPort::SerialPortError>("errorCode");

    QTest::newRow("NotOpen") << int(QIODevice::NotOpen) << false << QSerialPort::UnsupportedOperationError;
    QTest::newRow("ReadOnly") << int(QIODevice::ReadOnly) << true << QSerialPort::NoError;
    QTest::newRow("WriteOnly") << int(QIODevice::WriteOnly) << true << QSerialPort::NoError;
    QTest::newRow("ReadWrite") << int(QIODevice::ReadWrite) << true << QSerialPort::NoError;
    QTest::newRow("Append") << int(QIODevice::Append) << false << QSerialPort::UnsupportedOperationError;
    QTest::newRow("Truncate") << int(QIODevice::Truncate) << false << QSerialPort::UnsupportedOperationError;
    QTest::newRow("Text") << int(QIODevice::Text) << false << QSerialPort::UnsupportedOperationError;
    QTest::newRow("Unbuffered") << int(QIODevice::Unbuffered) << false << QSerialPort::UnsupportedOperationError;
}

void tst_QSerialPort::openExisting()
{
    QFETCH(int, openMode);
    QFETCH(bool, openResult);
    QFETCH(QSerialPort::SerialPortError, errorCode);

    foreach (const QString &serialPortName, m_availablePortNames) {
        QSerialPort serialPort(serialPortName);
        QSignalSpy errorSpy(&serialPort, SIGNAL(error(QSerialPort::SerialPortError)));
        QVERIFY(errorSpy.isValid());

        QCOMPARE(serialPort.portName(), serialPortName);
        QCOMPARE(serialPort.open(QIODevice::OpenMode(openMode)), openResult);
        QCOMPARE(serialPort.isOpen(), openResult);
        QCOMPARE(serialPort.error(), errorCode);

        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(qvariant_cast<QSerialPort::SerialPortError>(errorSpy.at(0).at(0)), errorCode);
    }
}

void tst_QSerialPort::openNotExisting_data()
{
    QTest::addColumn<QString>("serialPortName");
    QTest::addColumn<bool>("openResult");
    QTest::addColumn<QSerialPort::SerialPortError>("errorCode");

    QTest::newRow("Empty") << QString("") << false << QSerialPort::DeviceNotFoundError;
    QTest::newRow("Null") << QString() << false << QSerialPort::DeviceNotFoundError;
    QTest::newRow("NotExists") << QString("ABCDEF") << false << QSerialPort::DeviceNotFoundError;
}

void tst_QSerialPort::openNotExisting()
{
    QFETCH(QString, serialPortName);
    QFETCH(bool, openResult);
    //QFETCH(QSerialPort::SerialPortError, errorCode);

    QSerialPort serialPort(serialPortName);

    QSignalSpy errorSpy(&serialPort, SIGNAL(error(QSerialPort::SerialPortError)));
    QVERIFY(errorSpy.isValid());

    QCOMPARE(serialPort.portName(), serialPortName);
    QCOMPARE(serialPort.open(QIODevice::ReadOnly), openResult);
    QCOMPARE(serialPort.isOpen(), openResult);
    //QCOMPARE(serialPort.error(), errorCode);

    //QCOMPARE(errorSpy.count(), 1);
    //QCOMPARE(qvariant_cast<QSerialPort::SerialPortError>(errorSpy.at(0).at(0)), errorCode);
}

void tst_QSerialPort::handleBytesWrittenAndExitLoopSlot(qint64 bytesWritten)
{
    QCOMPARE(bytesWritten, qint64(alphabetArray.size() + newlineArray.size()));
    exitLoop();
}

void tst_QSerialPort::flush()
{
#ifdef Q_OS_WIN
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    QSKIP("flush() does not work on Windows");
#else
    QSKIP("flush() does not work on Windows", SkipAll);
#endif
#endif

    QSerialPort serialPort(m_senderPortName);
    connect(&serialPort, SIGNAL(bytesWritten(qint64)), this, SLOT(handleBytesWrittenAndExitLoopSlot(qint64)));
    QSignalSpy bytesWrittenSpy(&serialPort, SIGNAL(bytesWritten(qint64)));

    QVERIFY(serialPort.open(QIODevice::WriteOnly));
    serialPort.write(alphabetArray + newlineArray);
    QCOMPARE(serialPort.bytesToWrite(), qint64(alphabetArray.size() + newlineArray.size()));
    serialPort.flush();
    QCOMPARE(serialPort.bytesToWrite(), qint64(0));
    enterLoop(1);
    QVERIFY2(!timeout(), "Timed out when waiting for the bytesWritten(qint64) signal.");
    QCOMPARE(bytesWrittenSpy.count(), 1);
}

void tst_QSerialPort::handleBytesWrittenAndExitLoopSlot2(qint64 bytesWritten)
{
    static qint64 bytes = 0;
    bytes += bytesWritten;

    QVERIFY(bytesWritten == newlineArray.size() || bytesWritten == alphabetArray.size());

    if (bytes == (alphabetArray.size() + newlineArray.size()))
        exitLoop();
}

void tst_QSerialPort::doubleFlush()
{
#ifdef Q_OS_WIN
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    QSKIP("flush() does not work on Windows");
#else
    QSKIP("flush() does not work on Windows", SkipAll);
#endif
#endif

    QSerialPort serialPort(m_senderPortName);
    connect(&serialPort, SIGNAL(bytesWritten(qint64)), this, SLOT(handleBytesWrittenAndExitLoopSlot2(qint64)));
    QSignalSpy bytesWrittenSpy(&serialPort, SIGNAL(bytesWritten(qint64)));

    QVERIFY(serialPort.open(QIODevice::WriteOnly));
    serialPort.write(alphabetArray);
    QCOMPARE(serialPort.bytesToWrite(), qint64(alphabetArray.size()));
    serialPort.flush();
    QCOMPARE(serialPort.bytesToWrite(), qint64(0));
    serialPort.write(newlineArray);
    QCOMPARE(serialPort.bytesToWrite(), qint64(newlineArray.size()));
    serialPort.flush();
    QCOMPARE(serialPort.bytesToWrite(), qint64(0));

    enterLoop(1);
    QVERIFY2(!timeout(), "Timed out when waiting for the bytesWritten(qint64) signal.");
    QCOMPARE(bytesWrittenSpy.count(), 2);
}

void tst_QSerialPort::waitForBytesWritten()
{
    // the dummy device on other side also has to be open
    QSerialPort dummySerialPort(m_receiverPortName);
    QVERIFY(dummySerialPort.open(QIODevice::ReadOnly));

    QSerialPort serialPort(m_senderPortName);
    QVERIFY(serialPort.open(QIODevice::WriteOnly));
    serialPort.write(alphabetArray);
    const qint64 toWrite = serialPort.bytesToWrite();
    QVERIFY(serialPort.waitForBytesWritten(1000));
    QVERIFY(toWrite > serialPort.bytesToWrite());
}

void tst_QSerialPort::waitForReadyReadWithTimeout()
{
    clearReceiver();
#ifdef Q_OS_WIN
    // the dummy device on other side also has to be open
    QSerialPort dummySerialPort(m_senderPortName);
    QVERIFY(dummySerialPort.open(QIODevice::WriteOnly));
#endif

    QSerialPort receiverSerialPort(m_receiverPortName);
    QVERIFY(receiverSerialPort.open(QIODevice::ReadOnly));
    QVERIFY(!receiverSerialPort.waitForReadyRead(5));
    QCOMPARE(receiverSerialPort.bytesAvailable(), qint64(0));
    QCOMPARE(receiverSerialPort.error(), QSerialPort::TimeoutError);
}

void tst_QSerialPort::waitForReadyReadWithOneByte()
{
#ifdef Q_OS_WIN
    clearReceiver();
#endif

    const qint64 oneByte = 1;
    const int waitMsecs = 50;

    QSerialPort senderSerialPort(m_senderPortName);
    QVERIFY(senderSerialPort.open(QIODevice::WriteOnly));
    QSerialPort receiverSerialPort(m_receiverPortName);
    QSignalSpy readyReadSpy(&receiverSerialPort, SIGNAL(readyRead()));
    QVERIFY(readyReadSpy.isValid());
    QVERIFY(receiverSerialPort.open(QIODevice::ReadOnly));
    QCOMPARE(senderSerialPort.write(alphabetArray.constData(), oneByte), oneByte);
    QVERIFY(senderSerialPort.waitForBytesWritten(waitMsecs));
    QVERIFY(receiverSerialPort.waitForReadyRead(waitMsecs));
    QCOMPARE(receiverSerialPort.bytesAvailable(), oneByte);
    QCOMPARE(receiverSerialPort.error(), QSerialPort::NoError);
    QCOMPARE(readyReadSpy.count(), 1);
}

void tst_QSerialPort::waitForReadyReadWithAlphabet()
{
#ifdef Q_OS_WIN
    clearReceiver();
#endif

    const int waitMsecs = 50;

    QSerialPort senderSerialPort(m_senderPortName);
    QVERIFY(senderSerialPort.open(QIODevice::WriteOnly));
    QSerialPort receiverSerialPort(m_receiverPortName);
    QSignalSpy readyReadSpy(&receiverSerialPort, SIGNAL(readyRead()));
    QVERIFY(readyReadSpy.isValid());
    QVERIFY(receiverSerialPort.open(QIODevice::ReadOnly));
    QCOMPARE(senderSerialPort.write(alphabetArray), qint64(alphabetArray.size()));
    QVERIFY(senderSerialPort.waitForBytesWritten(waitMsecs));

    do {
        QVERIFY(receiverSerialPort.waitForReadyRead(waitMsecs));
    } while (receiverSerialPort.bytesAvailable() < qint64(alphabetArray.size()));

    QCOMPARE(receiverSerialPort.error(), QSerialPort::NoError);
    QVERIFY(readyReadSpy.count() > 0);
}

void tst_QSerialPort::twoStageSynchronousLoopback()
{
#ifdef Q_OS_WIN
    clearReceiver();
    clearReceiver(m_senderPortName);
#endif

    QSerialPort senderPort(m_senderPortName);
    QVERIFY(senderPort.open(QSerialPort::ReadWrite));

    QSerialPort receiverPort(m_receiverPortName);
    QVERIFY(receiverPort.open(QSerialPort::ReadWrite));

    const int waitMsecs = 50;

    // first stage
    senderPort.write(newlineArray);
    senderPort.waitForBytesWritten(waitMsecs);
    QTest::qSleep(waitMsecs);
    receiverPort.waitForReadyRead(waitMsecs);
    QCOMPARE(receiverPort.bytesAvailable(), qint64(newlineArray.size()));

    receiverPort.write(receiverPort.readAll());
    receiverPort.waitForBytesWritten(waitMsecs);
    QTest::qSleep(waitMsecs);
    senderPort.waitForReadyRead(waitMsecs);
    QCOMPARE(senderPort.bytesAvailable(), qint64(newlineArray.size()));
    QCOMPARE(senderPort.readAll(), newlineArray);

    // second stage
    senderPort.write(newlineArray);
    senderPort.waitForBytesWritten(waitMsecs);
    QTest::qSleep(waitMsecs);
    receiverPort.waitForReadyRead(waitMsecs);
    QCOMPARE(receiverPort.bytesAvailable(), qint64(newlineArray.size()));
    receiverPort.write(receiverPort.readAll());
    receiverPort.waitForBytesWritten(waitMsecs);
    QTest::qSleep(waitMsecs);
    senderPort.waitForReadyRead(waitMsecs);
    QCOMPARE(senderPort.bytesAvailable(), qint64(newlineArray.size()));
    QCOMPARE(senderPort.readAll(), newlineArray);
}

void tst_QSerialPort::synchronousReadWrite()
{
#ifdef Q_OS_WIN
    clearReceiver();
#endif

    QSerialPort senderPort(m_senderPortName);
    QVERIFY(senderPort.open(QSerialPort::WriteOnly));

    QSerialPort receiverPort(m_receiverPortName);
    QVERIFY(receiverPort.open(QSerialPort::ReadOnly));

    QByteArray writeData;
    for (int i = 0; i < 1024; ++i)
        writeData.append(static_cast<char>(i));

    senderPort.write(writeData);
    senderPort.waitForBytesWritten(-1);

    QByteArray readData;
    while ((readData.size() < writeData.size()) && receiverPort.waitForReadyRead(100))
        readData.append(receiverPort.readAll());

    QCOMPARE(readData, writeData);
}

class AsyncReader : public QObject
{
    Q_OBJECT
public:
    explicit AsyncReader(QSerialPort &port, Qt::ConnectionType connectionType, int expectedBytesCount)
        : serialPort(port), expectedBytesCount(expectedBytesCount)
    {
        connect(&serialPort, SIGNAL(readyRead()), this, SLOT(receive()), connectionType);
    }

private slots:
    void receive()
    {
        if (serialPort.bytesAvailable() < expectedBytesCount)
            return;
        tst_QSerialPort::exitLoop();
    }

private:
    QSerialPort &serialPort;
    const int expectedBytesCount;
};

class AsyncWriterByBytesWritten : public QObject
{
    Q_OBJECT
public:
    explicit AsyncWriterByBytesWritten(
            QSerialPort &port, Qt::ConnectionType connectionType, const QByteArray &dataToWrite)
        : serialPort(port), writeChunkSize(0)
    {
        writeBuffer.setData(dataToWrite);
        writeBuffer.open(QIODevice::ReadOnly);
        connect(&serialPort, SIGNAL(bytesWritten(qint64)), this, SLOT(send()), connectionType);
        send();
    }

private slots:
    void send()
    {
        if (writeBuffer.bytesAvailable() > 0)
            serialPort.write(writeBuffer.read(++writeChunkSize));
    }

private:
    QSerialPort &serialPort;
    QBuffer writeBuffer;
    int writeChunkSize;
};

void tst_QSerialPort::asynchronousWriteByBytesWritten_data()
{
    QTest::addColumn<Qt::ConnectionType>("readConnectionType");
    QTest::addColumn<Qt::ConnectionType>("writeConnectionType");

    QTest::newRow("BothQueued") << Qt::QueuedConnection << Qt::QueuedConnection;
    QTest::newRow("BothDirect") << Qt::DirectConnection << Qt::DirectConnection;
    QTest::newRow("ReadDirectWriteQueued") << Qt::DirectConnection << Qt::QueuedConnection;
    QTest::newRow("ReadQueuedWriteDirect") << Qt::QueuedConnection << Qt::DirectConnection;
}

void tst_QSerialPort::asynchronousWriteByBytesWritten()
{
#ifdef Q_OS_WIN
    clearReceiver();
#endif

    QFETCH(Qt::ConnectionType, readConnectionType);
    QFETCH(Qt::ConnectionType, writeConnectionType);

    QSerialPort receiverPort(m_receiverPortName);
    QVERIFY(receiverPort.open(QSerialPort::ReadOnly));
    AsyncReader reader(receiverPort, readConnectionType, alphabetArray.size());

    QSerialPort senderPort(m_senderPortName);
    QVERIFY(senderPort.open(QSerialPort::WriteOnly));
    AsyncWriterByBytesWritten writer(senderPort, writeConnectionType, alphabetArray);

    enterLoop(1);
    QVERIFY2(!timeout(), "Timed out when waiting for the read or write.");
    QCOMPARE(receiverPort.bytesAvailable(), qint64(alphabetArray.size()));
    QCOMPARE(receiverPort.readAll(), alphabetArray);
}

class AsyncWriterByTimer : public QObject
{
    Q_OBJECT
public:
    explicit AsyncWriterByTimer(
            QSerialPort &port, Qt::ConnectionType connectionType, const QByteArray &dataToWrite)
        : serialPort(port), writeChunkSize(0)
    {
        writeBuffer.setData(dataToWrite);
        writeBuffer.open(QIODevice::ReadOnly);
        connect(&timer, SIGNAL(timeout()), this, SLOT(send()), connectionType);
        timer.start(0);
    }

private slots:
    void send()
    {
        if (writeBuffer.bytesAvailable() > 0)
            serialPort.write(writeBuffer.read(++writeChunkSize));
        else
            timer.stop();
    }

private:
    QSerialPort &serialPort;
    QBuffer writeBuffer;
    int writeChunkSize;
    QTimer timer;
};

void tst_QSerialPort::asynchronousWriteByTimer_data()
{
    QTest::addColumn<Qt::ConnectionType>("readConnectionType");
    QTest::addColumn<Qt::ConnectionType>("writeConnectionType");

    QTest::newRow("BothQueued") << Qt::QueuedConnection << Qt::QueuedConnection;
    QTest::newRow("BothDirect") << Qt::DirectConnection << Qt::DirectConnection;
    QTest::newRow("ReadDirectWriteQueued") << Qt::DirectConnection << Qt::QueuedConnection;
    QTest::newRow("ReadQueuedWriteDirect") << Qt::QueuedConnection << Qt::DirectConnection;
}

void tst_QSerialPort::asynchronousWriteByTimer()
{
#ifdef Q_OS_WIN
    clearReceiver();
#endif

    QFETCH(Qt::ConnectionType, readConnectionType);
    QFETCH(Qt::ConnectionType, writeConnectionType);

    QSerialPort receiverPort(m_receiverPortName);
    QVERIFY(receiverPort.open(QSerialPort::ReadOnly));
    AsyncReader reader(receiverPort, readConnectionType, alphabetArray.size());

    QSerialPort senderPort(m_senderPortName);
    QVERIFY(senderPort.open(QSerialPort::WriteOnly));
    AsyncWriterByTimer writer(senderPort, writeConnectionType, alphabetArray);

    enterLoop(1);
    QVERIFY2(!timeout(), "Timed out when waiting for the read or write.");
    QCOMPARE(receiverPort.bytesAvailable(), qint64(alphabetArray.size()));
    QCOMPARE(receiverPort.readAll(), alphabetArray);
}

#ifdef Q_OS_WIN
void tst_QSerialPort::readBufferOverflow()
{
    clearReceiver();

    QSerialPort senderPort(m_senderPortName);
    QVERIFY(senderPort.open(QSerialPort::WriteOnly));

    QSerialPort receiverPort(m_receiverPortName);
    QVERIFY(receiverPort.open(QSerialPort::ReadOnly));

    const int readBufferSize = alphabetArray.size() / 2;
    receiverPort.setReadBufferSize(readBufferSize);
    QCOMPARE(receiverPort.readBufferSize(), qint64(readBufferSize));

    QCOMPARE(senderPort.write(alphabetArray), qint64(alphabetArray.size()));
    QVERIFY2(senderPort.waitForBytesWritten(100), "Waiting for bytes written failed");

    QByteArray readData;
    while (receiverPort.waitForReadyRead(100)) {
        QVERIFY(receiverPort.bytesAvailable() > 0);
        readData += receiverPort.readAll();
    }

    QCOMPARE(readData, alphabetArray);

    // No more bytes available
    QVERIFY(receiverPort.bytesAvailable() == 0);
}

void tst_QSerialPort::readAfterInputClear()
{
    clearReceiver();

    QSerialPort senderPort(m_senderPortName);
    QVERIFY(senderPort.open(QSerialPort::WriteOnly));

    QSerialPort receiverPort(m_receiverPortName);
    QVERIFY(receiverPort.open(QSerialPort::ReadOnly));

    const int readBufferSize = alphabetArray.size() / 2;
    receiverPort.setReadBufferSize(readBufferSize);
    QCOMPARE(receiverPort.readBufferSize(), qint64(readBufferSize));

    const int waitMsecs = 100;

    // First write more than read buffer size
    QCOMPARE(senderPort.write(alphabetArray), qint64(alphabetArray.size()));
    QVERIFY2(senderPort.waitForBytesWritten(waitMsecs), "Waiting for bytes written failed");

    // Wait for first part of data into read buffer
    while (receiverPort.waitForReadyRead(waitMsecs));
    QCOMPARE(receiverPort.bytesAvailable(), qint64(readBufferSize));
    // Wait for second part of data into driver's FIFO
    QTest::qSleep(waitMsecs);

    QVERIFY(receiverPort.clear(QSerialPort::Input));
    QCOMPARE(receiverPort.bytesAvailable(), qint64(0));

    // Second write less than read buffer size
    QCOMPARE(senderPort.write(newlineArray), qint64(newlineArray.size()));
    QVERIFY2(senderPort.waitForBytesWritten(waitMsecs), "Waiting for bytes written failed");

    while (receiverPort.waitForReadyRead(waitMsecs));
    QCOMPARE(receiverPort.bytesAvailable(), qint64(newlineArray.size()));
    QCOMPARE(receiverPort.readAll(), newlineArray);

    // No more bytes available
    QVERIFY(receiverPort.bytesAvailable() == 0);
}
#endif

QTEST_MAIN(tst_QSerialPort)
#include "tst_qserialport.moc"
