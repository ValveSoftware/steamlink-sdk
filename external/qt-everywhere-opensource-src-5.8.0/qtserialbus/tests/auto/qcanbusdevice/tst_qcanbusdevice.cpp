/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtSerialBus/QCanBusDevice>
#include <QtSerialBus/QCanBusFrame>

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QScopedPointer>

#include <memory>

class tst_Backend : public QCanBusDevice
{
    Q_OBJECT
public:
    tst_Backend() :
        firstOpen(true), writeBufferUsed(true)
    {
        referenceFrame.setFrameId(5);
        referenceFrame.setPayload(QByteArray("FOOBAR"));
        referenceFrame.setTimeStamp({ 22, 23 });
        referenceFrame.setExtendedFrameFormat(1);
    }

    bool triggerNewFrame()
    {
        if (state() != QCanBusDevice::ConnectedState)
            return false;

        // line below triggers framesReceived() signal
        enqueueReceivedFrames(QVector<QCanBusFrame>() << referenceFrame);

        return true;
    }

    bool open()
    {
        if (firstOpen) {
            firstOpen = false;
            return false;
        }
        setState(QCanBusDevice::ConnectedState);
        return true;
    }

    void close()
    {
        setState(QCanBusDevice::UnconnectedState);
    }

    bool writeFrame(const QCanBusFrame &data)
    {
        if (state() != QCanBusDevice::ConnectedState)
            return false;

        if (writeBufferUsed) {
            enqueueOutgoingFrame(data);
            QTimer::singleShot(2000, this, [this](){ triggerDelayedWrites(); });
        } else {
            emit framesWritten(1);
        }
        return true;
    }

    void emulateError(const QString &text, QCanBusDevice::CanBusError e)
    {
        setError(text, e);
    }

    QString interpretErrorFrame(const QCanBusFrame &/*errorFrame*/)
    {
        return QString();
    }

    bool isWriteBuffered() const { return writeBufferUsed; }
    void setWriteBuffered(bool isBuffered)
    {
        // permits switching between buffered and unbuffered write mode
        writeBufferUsed = isBuffered;
    }

public slots:
    void triggerDelayedWrites()
    {
        if (framesToWrite() == 0)
            return;

        dequeueOutgoingFrame();
        emit framesWritten(1);

        if (framesToWrite() > 0)
            QTimer::singleShot(2000, this, [this](){ triggerDelayedWrites(); });
    }

private:
    QCanBusFrame referenceFrame;
    bool firstOpen;
    bool writeBufferUsed;
};

class tst_QCanBusDevice : public QObject
{
    Q_OBJECT
public:
    explicit tst_QCanBusDevice();

private slots:
    void initTestCase();
    void conf();
    void write();
    void read();
    void error();
    void cleanupTestCase();
    void tst_filtering();
    void tst_bufferingAttribute();

    void tst_waitForFramesReceived();
    void tst_waitForFramesWritten();
private:
    QScopedPointer<tst_Backend> device;
};

tst_QCanBusDevice::tst_QCanBusDevice()
{
    qRegisterMetaType<QCanBusDevice::CanBusDeviceState>();
    qRegisterMetaType<QCanBusDevice::CanBusError>();
}

void tst_QCanBusDevice::initTestCase()
{
    device.reset(new tst_Backend());
    QVERIFY(device);

    QSignalSpy stateSpy(device.data(),
                        SIGNAL(stateChanged(QCanBusDevice::CanBusDeviceState)));

    QVERIFY(!device->connectDevice()); //first connect triggered to fail
    QVERIFY(device->connectDevice());
    QTRY_VERIFY_WITH_TIMEOUT(device->state() == QCanBusDevice::ConnectedState,
                             5000);
    QCOMPARE(stateSpy.count(), 4);
    QCOMPARE(stateSpy[0].at(0).value<QCanBusDevice::CanBusDeviceState>(),
             QCanBusDevice::ConnectingState);
    QCOMPARE(stateSpy[1].at(0).value<QCanBusDevice::CanBusDeviceState>(),
             QCanBusDevice::UnconnectedState);
    QCOMPARE(stateSpy[2].at(0).value<QCanBusDevice::CanBusDeviceState>(),
             QCanBusDevice::ConnectingState);
    QCOMPARE(stateSpy[3].at(0).value<QCanBusDevice::CanBusDeviceState>(),
             QCanBusDevice::ConnectedState);
}

void tst_QCanBusDevice::conf()
{
    QVERIFY(device->configurationKeys().isEmpty());

    // invalid QVariant ignored
    device->setConfigurationParameter(QCanBusDevice::RawFilterKey, QVariant());
    QVERIFY(device->configurationKeys().isEmpty());

    QCanBusFrame::FrameErrors error =
            (QCanBusFrame::LostArbitrationError | QCanBusFrame::BusError);

    device->setConfigurationParameter(
                QCanBusDevice::ErrorFilterKey, QVariant::fromValue(error));
    QVariant value = device->configurationParameter(QCanBusDevice::ErrorFilterKey);
    QVERIFY(value.isValid());

    QVector<int> keys = device->configurationKeys();
    QCOMPARE(keys.size(), 1);
    QVERIFY(keys[0] == QCanBusDevice::ErrorFilterKey);

    QCOMPARE(value.value<QCanBusFrame::FrameErrors>(),
             QCanBusFrame::LostArbitrationError | QCanBusFrame::BusError);

    device->setConfigurationParameter(QCanBusDevice::ErrorFilterKey, QVariant());
    QVERIFY(device->configurationKeys().isEmpty());
}

void tst_QCanBusDevice::write()
{
    // we assume unbuffered writing in this function
    device->setWriteBuffered(false);
    QCOMPARE(device->isWriteBuffered(), false);

    QSignalSpy spy(device.data(), SIGNAL(framesWritten(qint64)));
    QSignalSpy stateSpy(device.data(),
                        SIGNAL(stateChanged(QCanBusDevice::CanBusDeviceState)));

    QCanBusFrame frame;
    frame.setPayload(QByteArray("testData"));

    device->disconnectDevice();
    QTRY_VERIFY_WITH_TIMEOUT(device->state() == QCanBusDevice::UnconnectedState,
                             5000);
    QCOMPARE(stateSpy.count(), 2);
    QCOMPARE(stateSpy[0].at(0).value<QCanBusDevice::CanBusDeviceState>(),
             QCanBusDevice::ClosingState);
    QCOMPARE(stateSpy[1].at(0).value<QCanBusDevice::CanBusDeviceState>(),
             QCanBusDevice::UnconnectedState);
    stateSpy.clear();
    QVERIFY(stateSpy.isEmpty());

    device->writeFrame(frame);
    QCOMPARE(spy.count(), 0);


    device->connectDevice();
    QTRY_VERIFY_WITH_TIMEOUT(device->state() == QCanBusDevice::ConnectedState,
                             5000);
    QCOMPARE(stateSpy.count(), 2);
    QCOMPARE(stateSpy[0].at(0).value<QCanBusDevice::CanBusDeviceState>(),
             QCanBusDevice::ConnectingState);
    QCOMPARE(stateSpy[1].at(0).value<QCanBusDevice::CanBusDeviceState>(),
             QCanBusDevice::ConnectedState);

    device->writeFrame(frame);
    QCOMPARE(spy.count(), 1);
}

void tst_QCanBusDevice::read()
{
    QSignalSpy stateSpy(device.data(),
                        SIGNAL(stateChanged(QCanBusDevice::CanBusDeviceState)));

    device->disconnectDevice();
    QCOMPARE(device->state(), QCanBusDevice::UnconnectedState);
    stateSpy.clear();

    const QCanBusFrame frame1 = device->readFrame();

    QVERIFY(device->connectDevice());
    QTRY_VERIFY_WITH_TIMEOUT(device->state() == QCanBusDevice::ConnectedState,
                             5000);
    QCOMPARE(stateSpy.count(), 2);
    QCOMPARE(stateSpy[0].at(0).value<QCanBusDevice::CanBusDeviceState>(),
             QCanBusDevice::ConnectingState);
    QCOMPARE(stateSpy[1].at(0).value<QCanBusDevice::CanBusDeviceState>(),
             QCanBusDevice::ConnectedState);

    device->triggerNewFrame();
    const QCanBusFrame frame2 = device->readFrame();
    QVERIFY(!frame1.frameId());
    QVERIFY(!frame1.isValid());
    QVERIFY(frame2.frameId());
    QVERIFY(frame2.isValid());
}

void tst_QCanBusDevice::error()
{
    QSignalSpy spy(device.data(), SIGNAL(errorOccurred(QCanBusDevice::CanBusError)));
    QString testString(QStringLiteral("testString"));

    auto backend = qobject_cast<tst_Backend *>(device.data());
    QVERIFY(backend);

    //NoError
    QVERIFY(device->errorString().isEmpty());

    //ReadError
    backend->emulateError(testString+QString::fromLatin1("a"),
                         QCanBusDevice::ReadError);
    QCOMPARE(testString+QString::fromLatin1("a"), device->errorString());
    QVERIFY(device->error() == 1);
    QCOMPARE(spy.count(), 1);

    //WriteError
    backend->emulateError(testString+QString::fromLatin1("b"),
                         QCanBusDevice::WriteError);
    QCOMPARE(testString+QString::fromLatin1("b"), device->errorString());
    QVERIFY(device->error() == 2);
    QCOMPARE(spy.count(), 2);

    //ConnectionError
    backend->emulateError(testString+QString::fromLatin1("c"),
                         QCanBusDevice::ConnectionError);
    QCOMPARE(testString+QString::fromLatin1("c"), device->errorString());
    QVERIFY(device->error() == 3);
    QCOMPARE(spy.count(), 3);

    //ConfigurationError
    backend->emulateError(testString+QString::fromLatin1("d"),
                         QCanBusDevice::ConfigurationError);
    QCOMPARE(testString+QString::fromLatin1("d"), device->errorString());
    QVERIFY(device->error() == 4);
    QCOMPARE(spy.count(), 4);

    //UnknownError
    backend->emulateError(testString+QString::fromLatin1("e"),
                          QCanBusDevice::UnknownError);
    QCOMPARE(testString+QString::fromLatin1("e"), device->errorString());
    QVERIFY(device->error() == 5);
    QCOMPARE(spy.count(), 5);
}

void tst_QCanBusDevice::cleanupTestCase()
{
    device->disconnectDevice();
    QCOMPARE(device->state(), QCanBusDevice::UnconnectedState);
    QCanBusFrame frame = device->readFrame();
    QVERIFY(!frame.frameId());
}

void tst_QCanBusDevice::tst_filtering()
{
    QCanBusDevice::Filter defaultFilter;
    QCOMPARE(defaultFilter.frameId, 0x0u);
    QCOMPARE(defaultFilter.frameIdMask, 0x0u);
    QCOMPARE(defaultFilter.type, QCanBusFrame::InvalidFrame);
    QCOMPARE(defaultFilter.format, QCanBusDevice::Filter::MatchBaseAndExtendedFormat);

    QList<QCanBusDevice::Filter> filters;
    QCanBusDevice::Filter f;
    f.frameId = 0x1;
    f.type = QCanBusFrame::DataFrame;
    f.frameIdMask = 0xFF;
    f.format = QCanBusDevice::Filter::MatchBaseAndExtendedFormat;
    filters.append(f);

    f.frameId = 0x2;
    f.type = QCanBusFrame::RemoteRequestFrame;
    f.frameIdMask = 0x0;
    f.format = QCanBusDevice::Filter::MatchBaseFormat;
    filters.append(f);

    QVariant wrapper = QVariant::fromValue(filters);
    QList<QCanBusDevice::Filter> newFilter
            = wrapper.value<QList<QCanBusDevice::Filter> >();
    QCOMPARE(newFilter.count(), 2);

    QCOMPARE(newFilter.at(0).type, QCanBusFrame::DataFrame);
    QCOMPARE(newFilter.at(0).frameId, 0x1u);
    QCOMPARE(newFilter.at(0).frameIdMask, 0xFFu);
    QVERIFY(newFilter.at(0).format & QCanBusDevice::Filter::MatchBaseAndExtendedFormat);
    QVERIFY(newFilter.at(0).format & QCanBusDevice::Filter::MatchBaseFormat);
    QVERIFY(newFilter.at(0).format & QCanBusDevice::Filter::MatchExtendedFormat);

    QCOMPARE(newFilter.at(1).type, QCanBusFrame::RemoteRequestFrame);
    QCOMPARE(newFilter.at(1).frameId, 0x2u);
    QCOMPARE(newFilter.at(1).frameIdMask, 0x0u);
    QVERIFY((newFilter.at(1).format & QCanBusDevice::Filter::MatchBaseAndExtendedFormat)
              != QCanBusDevice::Filter::MatchBaseAndExtendedFormat);
    QVERIFY(newFilter.at(1).format & QCanBusDevice::Filter::MatchBaseFormat);
    QVERIFY(!(newFilter.at(1).format & QCanBusDevice::Filter::MatchExtendedFormat));
}

void tst_QCanBusDevice::tst_bufferingAttribute()
{
    std::unique_ptr<tst_Backend> canDevice(new tst_Backend);
    QVERIFY((bool)canDevice);
    // by default buffered set to true
    QCOMPARE(canDevice->isWriteBuffered(), true);

    canDevice->setWriteBuffered(false);
    QCOMPARE(canDevice->isWriteBuffered(), false);
    canDevice->setWriteBuffered(true);
    QCOMPARE(canDevice->isWriteBuffered(), true);
}

void tst_QCanBusDevice::tst_waitForFramesReceived()
{
    if (device->state() != QCanBusDevice::ConnectedState) {
        QVERIFY(device->connectDevice());
        QTRY_VERIFY_WITH_TIMEOUT(device->state() == QCanBusDevice::ConnectedState,
                                 5000);
    }

    QVERIFY(!device->framesAvailable());
    QVERIFY(device->triggerNewFrame());
    QVERIFY(device->framesAvailable());

    // frame is available, function blocks and times out
    bool result = device->waitForFramesReceived(2000);
    QVERIFY(!result);

    QCanBusFrame frame = device->readFrame();
    QVERIFY(frame.isValid());
    QCOMPARE(frame.payload(), QByteArray("FOOBAR"));
    QVERIFY(!device->framesAvailable());

    QElapsedTimer elapsed;
    elapsed.start();
    // no pending frame (should trigger active wait & timeout)
    result = device->waitForFramesReceived(5000);
    QVERIFY(elapsed.hasExpired(4000)); // should have caused time elapse
    QVERIFY(!result);

    QTimer::singleShot(2000, [&]() { device->triggerNewFrame(); });
    elapsed.restart();
    // frame will be inserted after 2s
    result = device->waitForFramesReceived(8000);
    QVERIFY(!elapsed.hasExpired(8000));
    QVERIFY(result);

    frame = device->readFrame();
    QVERIFY(frame.isValid());
    QCOMPARE(frame.payload(), QByteArray("FOOBAR"));
    QVERIFY(!device->framesAvailable());

    QTimer::singleShot(2000, [&]() {
        device->emulateError(QString("TriggerError"), QCanBusDevice::ReadError);
    });
    elapsed.restart();
    // error will be inserted after 2s
    result = device->waitForFramesReceived(8000);
    QVERIFY(!elapsed.hasExpired(8000));
    QVERIFY(!result);
    QCOMPARE(device->errorString(), QString("TriggerError"));
    QCOMPARE(device->error(), QCanBusDevice::ReadError);

    // test recursive calling of waitForFramesReceived() behavior
    int handleCounter = 0;
    QTimer::singleShot(1000, [&]() {
        device->triggerNewFrame();
        device->triggerNewFrame();
    });
    QTimer::singleShot(2000, [&]() { device->triggerNewFrame(); });
    QObject::connect(device.data(), &QCanBusDevice::framesReceived, [this, &handleCounter]() {
        handleCounter++;
        //this should trigger a recursion which we want to catch
        device->waitForFramesReceived(5000);
    });
    result = device->waitForFramesReceived(8000);
    QVERIFY(result);
    QTRY_COMPARE_WITH_TIMEOUT(handleCounter, 3, 5000);
}

void tst_QCanBusDevice::tst_waitForFramesWritten()
{
    if (device->state() != QCanBusDevice::ConnectedState) {
        QVERIFY(device->connectDevice());
        QTRY_VERIFY_WITH_TIMEOUT(device->state() == QCanBusDevice::ConnectedState,
                                 5000);
    }

    device->setWriteBuffered(false);
    bool result = device->waitForFramesWritten(1000);
    QVERIFY(!result); //no buffer, wait not possible

    device->setWriteBuffered(true);

    QVERIFY(device->framesToWrite() == 0);
    result = device->waitForFramesWritten(1000);
    QVERIFY(!result); //nothing in buffer, nothing to wait for

    QCanBusFrame frame;
    frame.setPayload(QByteArray("testData"));

    // test error case
    QTimer::singleShot(500, [&]() {
        device->emulateError(QString("TriggerWriteError"), QCanBusDevice::ReadError);
    });
    device->writeFrame(frame);
    QElapsedTimer elapsed;
    elapsed.start();

    // error will be triggered
    result = device->waitForFramesWritten(8000);
    QVERIFY(!elapsed.hasExpired(8000));
    QVERIFY(!result);
    QCOMPARE(device->errorString(), QString("TriggerWriteError"));
    QCOMPARE(device->error(), QCanBusDevice::ReadError);

    // flush remaining frames out to reset the test
    QTRY_VERIFY_WITH_TIMEOUT(device->framesToWrite() == 0, 10000);

    // test timeout
    device->writeFrame(frame);
    result = device->waitForFramesWritten(500);
    QVERIFY(elapsed.hasExpired(500));
    QVERIFY(!result);

    // flush remaining frames out to reset the test
    QTRY_VERIFY_WITH_TIMEOUT(device->framesToWrite() == 0, 10000);

    device->writeFrame(frame);
    device->writeFrame(frame);
    elapsed.restart();
    result = device->waitForFramesWritten(8000);
    QVERIFY(!elapsed.hasExpired(8000));
    QVERIFY(result);

    // flush remaining frames out to reset the test
    QTRY_VERIFY_WITH_TIMEOUT(device->framesToWrite() == 0, 10000);

    // test recursive calling of waitForFramesWritten() behavior
    int handleCounter = 0;
    device->writeFrame(frame);
    QTimer::singleShot(1000, [&]() { device->writeFrame(frame); });
    QTimer::singleShot(2000, [&]() { device->writeFrame(frame); });
    QObject::connect(device.data(), &QCanBusDevice::framesWritten, [this, &handleCounter]() {
        handleCounter++;
        //this should trigger a recursion which we want to catch
        device->waitForFramesWritten(5000);
    });
    result = device->waitForFramesWritten(8000);
    QVERIFY(result);
    QTRY_COMPARE_WITH_TIMEOUT(handleCounter, 3, 5000);

    device->setWriteBuffered(false);
}

QTEST_MAIN(tst_QCanBusDevice)

#include "tst_qcanbusdevice.moc"
