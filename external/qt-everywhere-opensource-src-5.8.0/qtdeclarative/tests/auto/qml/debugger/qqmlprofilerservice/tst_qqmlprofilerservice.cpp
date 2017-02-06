/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "debugutil_p.h"
#include "../../../shared/util.h"

#include <private/qqmlprofilerclient_p.h>
#include <private/qqmldebugconnection_p.h>

#include <QtTest/qtest.h>
#include <QtCore/qlibraryinfo.h>

#define STR_PORT_FROM "13773"
#define STR_PORT_TO "13783"

struct QQmlProfilerData
{
    QQmlProfilerData(qint64 time = -2, int messageType = -1, int detailType = -1,
                     const QString &detailData = QString()) :
        time(time), messageType(messageType), detailType(detailType), detailData(detailData),
        line(-1), column(-1), framerate(-1), animationcount(-1), amount(-1)
    {}

    qint64 time;
    int messageType;
    int detailType;

    //###
    QString detailData; //used by RangeData and RangeLocation
    int line;           //used by RangeLocation
    int column;         //used by RangeLocation
    int framerate;      //used by animation events
    int animationcount; //used by animation events
    qint64 amount;      //used by heap events
};

class QQmlProfilerTestClient : public QQmlProfilerClient
{
    Q_OBJECT

public:
    QQmlProfilerTestClient(QQmlDebugConnection *connection) : QQmlProfilerClient(connection),
        lastTimestamp(-1) {}

    QVector<QQmlProfilerData> qmlMessages;
    QVector<QQmlProfilerData> javascriptMessages;
    QVector<QQmlProfilerData> jsHeapMessages;
    QVector<QQmlProfilerData> asynchronousMessages;
    QVector<QQmlProfilerData> pixmapMessages;

    qint64 lastTimestamp;

signals:
    void recordingFinished();

private:
    void traceStarted(qint64 time, int engineId);
    void traceFinished(qint64 time, int engineId);
    void rangeStart(QQmlProfilerDefinitions::RangeType type, qint64 startTime);
    void rangeData(QQmlProfilerDefinitions::RangeType type, qint64 time, const QString &data);
    void rangeLocation(QQmlProfilerDefinitions::RangeType type, qint64 time,
                       const QQmlEventLocation &location);
    void rangeEnd(QQmlProfilerDefinitions::RangeType type, qint64 endTime);
    void animationFrame(qint64 time, int frameRate, int animationCount, int threadId);
    void sceneGraphEvent(QQmlProfilerDefinitions::SceneGraphFrameType type, qint64 time,
                         qint64 numericData1, qint64 numericData2, qint64 numericData3,
                         qint64 numericData4, qint64 numericData5);
    void pixmapCacheEvent(QQmlProfilerDefinitions::PixmapEventType type, qint64 time,
                          const QString &url, int numericData1, int numericData2);
    void memoryAllocation(QQmlProfilerDefinitions::MemoryType type, qint64 time, qint64 amount);
    void inputEvent(QQmlProfilerDefinitions::InputEventType type, qint64 time, int a, int b);
    void complete();

    void unknownEvent(QQmlProfilerDefinitions::Message messageType, qint64 time, int detailType);
    void unknownData(QPacket &stream);
};

void QQmlProfilerTestClient::traceStarted(qint64 time, int engineId)
{
    asynchronousMessages.append(QQmlProfilerData(time, QQmlProfilerDefinitions::Event,
                                        QQmlProfilerDefinitions::StartTrace,
                                        QString::number(engineId)));
}

void QQmlProfilerTestClient::traceFinished(qint64 time, int engineId)
{
    asynchronousMessages.append(QQmlProfilerData(time, QQmlProfilerDefinitions::Event,
                                        QQmlProfilerDefinitions::EndTrace,
                                        QString::number(engineId)));
}

void QQmlProfilerTestClient::rangeStart(QQmlProfilerDefinitions::RangeType type, qint64 startTime)
{
    QVERIFY(type >= 0 && type < QQmlProfilerDefinitions::MaximumRangeType);
    QVERIFY(lastTimestamp <= startTime);
    lastTimestamp = startTime;
    QQmlProfilerData data(startTime, QQmlProfilerDefinitions::RangeStart, type);
    if (type == QQmlProfilerDefinitions::Javascript)
        javascriptMessages.append(data);
    else
        qmlMessages.append(data);
}

void QQmlProfilerTestClient::rangeData(QQmlProfilerDefinitions::RangeType type, qint64 time,
                                      const QString &string)
{
    QVERIFY(type >= 0 && type < QQmlProfilerDefinitions::MaximumRangeType);
    QVERIFY(lastTimestamp <= time);
    lastTimestamp = time;
    QQmlProfilerData data(time, QQmlProfilerDefinitions::RangeData, type, string);
    if (type == QQmlProfilerDefinitions::Javascript)
        javascriptMessages.append(data);
    else
        qmlMessages.append(data);
}

void QQmlProfilerTestClient::rangeLocation(QQmlProfilerDefinitions::RangeType type, qint64 time,
                                          const QQmlEventLocation &location)
{
    QVERIFY(type >= 0 && type < QQmlProfilerDefinitions::MaximumRangeType);
    QVERIFY(location.line >= -2);
    QVERIFY(lastTimestamp <= time);
    lastTimestamp = time;
    QQmlProfilerData data(time, QQmlProfilerDefinitions::RangeLocation, type, location.filename);
    data.line = location.line;
    data.column = location.column;
    if (type == QQmlProfilerDefinitions::Javascript)
        javascriptMessages.append(data);
    else
        qmlMessages.append(data);
}

void QQmlProfilerTestClient::rangeEnd(QQmlProfilerDefinitions::RangeType type, qint64 endTime)
{
    QVERIFY(type >= 0 && type < QQmlProfilerDefinitions::MaximumRangeType);
    QVERIFY(lastTimestamp <= endTime);
    lastTimestamp = endTime;
    QQmlProfilerData data(endTime, QQmlProfilerDefinitions::RangeEnd, type);
    if (type == QQmlProfilerDefinitions::Javascript)
        javascriptMessages.append(data);
    else
        qmlMessages.append(data);
}

void QQmlProfilerTestClient::animationFrame(qint64 time, int frameRate, int animationCount, int threadId)
{
    QVERIFY(threadId >= 0);
    QVERIFY(frameRate != -1);
    QVERIFY(animationCount != -1);
    QVERIFY(lastTimestamp <= time);
    lastTimestamp = time;
    QQmlProfilerData data(time, QQmlProfilerDefinitions::Event,
                          QQmlProfilerDefinitions::AnimationFrame);
    data.framerate = frameRate;
    data.animationcount = animationCount;
    asynchronousMessages.append(data);
}

void QQmlProfilerTestClient::sceneGraphEvent(QQmlProfilerDefinitions::SceneGraphFrameType type,
                                            qint64 time, qint64 numericData1, qint64 numericData2,
                                            qint64 numericData3, qint64 numericData4,
                                            qint64 numericData5)
{
    Q_UNUSED(numericData1);
    Q_UNUSED(numericData2);
    Q_UNUSED(numericData3);
    Q_UNUSED(numericData4);
    Q_UNUSED(numericData5);
    QVERIFY(lastTimestamp <= time);
    lastTimestamp = time;
    asynchronousMessages.append(QQmlProfilerData(time, QQmlProfilerDefinitions::SceneGraphFrame,
                                                 type));
}

void QQmlProfilerTestClient::pixmapCacheEvent(QQmlProfilerDefinitions::PixmapEventType type,
                                             qint64 time, const QString &url, int numericData1,
                                             int numericData2)
{
    QVERIFY(lastTimestamp <= time);
    lastTimestamp = time;
    QQmlProfilerData data(time, QQmlProfilerDefinitions::PixmapCacheEvent, type, url);
    switch (type) {
    case QQmlProfilerDefinitions::PixmapSizeKnown:
        data.line = numericData1;
        data.column = numericData2;
        break;
    case QQmlProfilerDefinitions::PixmapReferenceCountChanged:
    case QQmlProfilerDefinitions::PixmapCacheCountChanged:
        data.animationcount = numericData1;
        break;
    default:
        break;
    }
    pixmapMessages.append(data);
}

void QQmlProfilerTestClient::memoryAllocation(QQmlProfilerDefinitions::MemoryType type, qint64 time,
                                             qint64 amount)
{
    QVERIFY(lastTimestamp <= time);
    lastTimestamp = time;
    QQmlProfilerData data(time, QQmlProfilerDefinitions::MemoryAllocation, type);
    data.amount = amount;
    jsHeapMessages.append(data);
}

void QQmlProfilerTestClient::inputEvent(QQmlProfilerDefinitions::InputEventType type, qint64 time,
                                        int a, int b)
{
    QVERIFY(lastTimestamp <= time);
    lastTimestamp = time;
    qmlMessages.append(QQmlProfilerData(time, QQmlProfilerDefinitions::Event, type,
                                        QString::number(a) + QLatin1Char('x') +
                                        QString::number(b)));
}

void QQmlProfilerTestClient::unknownEvent(QQmlProfilerDefinitions::Message messageType, qint64 time,
                                         int detailType)
{
    QFAIL(qPrintable(QString::fromLatin1("Unknown event %1 with detail type %2 received at %3.")
                     .arg(messageType).arg(detailType).arg(time)));
}

void QQmlProfilerTestClient::unknownData(QPacket &stream)
{
    QFAIL(qPrintable(QString::fromLatin1("%1 bytes of extra data after receiving message.")
                     .arg(stream.device()->bytesAvailable())));
}

void QQmlProfilerTestClient::complete()
{
    emit recordingFinished();
}

class tst_QQmlProfilerService : public QQmlDataTest
{
    Q_OBJECT

public:
    tst_QQmlProfilerService()
        : m_process(0)
        , m_connection(0)
        , m_client(0)
    {
    }


private:
    QQmlDebugProcess *m_process;
    QQmlDebugConnection *m_connection;
    QQmlProfilerTestClient *m_client;

    enum MessageListType {
        MessageListQML,
        MessageListJavaScript,
        MessageListJsHeap,
        MessageListAsynchronous,
        MessageListPixmap
    };

    enum CheckType {
        CheckMessageType  = 1 << 0,
        CheckDetailType   = 1 << 1,
        CheckLine         = 1 << 2,
        CheckColumn       = 1 << 3,
        CheckDataEndsWith = 1 << 4,

        CheckAll = CheckMessageType | CheckDetailType | CheckLine | CheckColumn | CheckDataEndsWith
    };

    void connect(bool block, const QString &testFile, bool restrictServices = true);
    void checkTraceReceived();
    void checkJsHeap();
    bool verify(MessageListType type, int expectedPosition, const QQmlProfilerData &expected,
                quint32 checks);

private slots:
    void cleanup();

    void connect_data();
    void connect();
    void pixmapCacheData();
    void scenegraphData();
    void profileOnExit();
    void controlFromJS();
    void signalSourceLocation();
    void javascript();
    void flushInterval();
};

#define VERIFY(type, position, expected, checks) QVERIFY(verify(type, position, expected, checks))

void tst_QQmlProfilerService::connect(bool block, const QString &testFile, bool restrictServices)
{
    // ### Still using qmlscene due to QTBUG-33377
    const QString executable = QLibraryInfo::location(QLibraryInfo::BinariesPath) + "/qmlscene";
    QStringList arguments;
    arguments << QString::fromLatin1("-qmljsdebugger=port:%1,%2%3%4")
                 .arg(STR_PORT_FROM).arg(STR_PORT_TO)
                 .arg(block ? QStringLiteral(",block") : QString())
                 .arg(restrictServices ? QStringLiteral(",services:CanvasFrameRate") : QString())
              << QQmlDataTest::instance()->testFile(testFile);

    m_process = new QQmlDebugProcess(executable, this);
    m_process->start(QStringList() << arguments);
    QVERIFY2(m_process->waitForSessionStart(), "Could not launch application, or did not get 'Waiting for connection'.");

    m_connection = new QQmlDebugConnection();
    m_client = new QQmlProfilerTestClient(m_connection);
    QList<QQmlDebugClient *> others = QQmlDebugTest::createOtherClients(m_connection);

    const int port = m_process->debugPort();
    m_connection->connectToHost(QLatin1String("127.0.0.1"), port);
    QVERIFY(m_client);
    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);

    foreach (QQmlDebugClient *other, others)
        QCOMPARE(other->state(), restrictServices ? QQmlDebugClient::Unavailable :
                                                    QQmlDebugClient::Enabled);
    qDeleteAll(others);
}

void tst_QQmlProfilerService::checkTraceReceived()
{
    QVERIFY2(QQmlDebugTest::waitForSignal(m_client, SIGNAL(recordingFinished())),
             "No trace received in time.");

    // must start with "StartTrace"
    QQmlProfilerData expected(0, QQmlProfilerDefinitions::Event,
                              QQmlProfilerDefinitions::StartTrace);
    VERIFY(MessageListAsynchronous, 0, expected, CheckMessageType | CheckDetailType);

    // must end with "EndTrace"
    expected.detailType = QQmlProfilerDefinitions::EndTrace;
    VERIFY(MessageListAsynchronous, m_client->asynchronousMessages.length() - 1, expected,
           CheckMessageType | CheckDetailType);
}

void tst_QQmlProfilerService::checkJsHeap()
{
    QVERIFY2(m_client->jsHeapMessages.count() > 0, "no JavaScript heap messages received");

    bool seen_alloc = false;
    bool seen_small = false;
    bool seen_large = false;
    qint64 allocated = 0;
    qint64 used = 0;
    qint64 lastTimestamp = -1;
    foreach (const QQmlProfilerData &message, m_client->jsHeapMessages) {
        switch (message.detailType) {
        case QV4::Profiling::HeapPage:
            allocated += message.amount;
            seen_alloc = true;
            break;
        case QV4::Profiling::SmallItem:
            used += message.amount;
            seen_small = true;
            break;
        case QV4::Profiling::LargeItem:
            allocated += message.amount;
            used += message.amount;
            seen_large = true;
            break;
        }

        QVERIFY(message.time >= lastTimestamp);
        // The heap will only be consistent after all events of the same timestamp are processed.
        if (lastTimestamp == -1) {
            lastTimestamp = message.time;
            continue;
        } else if (message.time == lastTimestamp) {
            continue;
        }

        lastTimestamp = message.time;

        QVERIFY2(used >= 0, QString::fromLatin1("Negative memory usage seen: %1")
                 .arg(used).toUtf8().constData());

        QVERIFY2(allocated >= 0, QString::fromLatin1("Negative memory allocation seen: %1")
                 .arg(allocated).toUtf8().constData());

        QVERIFY2(used <= allocated,
                 QString::fromLatin1("More memory usage than allocation seen: %1 > %2")
                 .arg(used).arg(allocated).toUtf8().constData());
    }

    QVERIFY2(seen_alloc, "No heap allocation seen");
    QVERIFY2(seen_small, "No small item seen");
    QVERIFY2(seen_large, "No large item seen");
}

bool tst_QQmlProfilerService::verify(tst_QQmlProfilerService::MessageListType type,
                                     int expectedPosition, const QQmlProfilerData &expected,
                                     quint32 checks)
{
    QVector<QQmlProfilerData> *target = 0;
    switch (type) {
        case MessageListQML:          target = &(m_client->qmlMessages); break;
        case MessageListJavaScript:   target = &(m_client->javascriptMessages); break;
        case MessageListJsHeap:       target = &(m_client->jsHeapMessages); break;
        case MessageListAsynchronous: target = &(m_client->asynchronousMessages); break;
        case MessageListPixmap:       target = &(m_client->pixmapMessages); break;
    }

    if (target->length() <= expectedPosition) {
        qWarning() << "Not enough events. expected position:" << expectedPosition
                   << "length:" << target->length();
        return false;
    }

    uint position = expectedPosition;
    qint64 timestamp = target->at(expectedPosition).time;
    while (position > 0 && target->at(position - 1).time == timestamp)
        --position;

    QStringList warnings;

    do {
        const QQmlProfilerData &actual = target->at(position);
        if ((checks & CheckMessageType) &&  actual.messageType != expected.messageType) {
            warnings << QString::fromLatin1("%1: unexpected messageType. actual: %2 - expected: %3")
                       .arg(position).arg(actual.messageType).arg(expected.messageType);
            continue;
        }
        if ((checks & CheckDetailType) && actual.detailType != expected.detailType) {
            warnings << QString::fromLatin1("%1: unexpected detailType. actual: %2 - expected: %3")
                       .arg(position).arg(actual.detailType).arg(expected.detailType);
            continue;
        }
        if ((checks & CheckLine) && actual.line != expected.line) {
            warnings << QString::fromLatin1("%1: unexpected line. actual: %2 - expected: %3")
                       .arg(position).arg(actual.line).arg(expected.line);
            continue;
        }
        if ((checks & CheckColumn) && actual.column != expected.column) {
            warnings << QString::fromLatin1("%1: unexpected column. actual: %2 - expected: %3")
                       .arg(position).arg(actual.column).arg(expected.column);
            continue;
        }
        if ((checks & CheckDataEndsWith) && !actual.detailData.endsWith(expected.detailData)) {
            warnings << QString::fromLatin1("%1: unexpected detailData. actual: %2 - expected: %3")
                       .arg(position).arg(actual.detailData).arg(expected.detailData);
            continue;
        }
        return true;
    } while (target->at(++position).time == timestamp);

    foreach (const QString &message, warnings)
        qWarning() << message.toLocal8Bit().constData();

    return false;
}

void tst_QQmlProfilerService::cleanup()
{
    if (m_client && QTest::currentTestFailed()) {
        qDebug() << "QML Messages:" << m_client->qmlMessages.count();
        int i = 0;
        foreach (const QQmlProfilerData &data, m_client->qmlMessages) {
            qDebug() << i++ << data.time << data.messageType << data.detailType << data.detailData
                     << data.line << data.column;
        }
        qDebug() << " ";
        qDebug() << "JavaScript Messages:" << m_client->javascriptMessages.count();
        i = 0;
        foreach (const QQmlProfilerData &data, m_client->javascriptMessages) {
            qDebug() << i++ << data.time << data.messageType << data.detailType << data.detailData
                     << data.line << data.column;
        }
        qDebug() << " ";
        qDebug() << "Asynchronous Messages:" << m_client->asynchronousMessages.count();
        i = 0;
        foreach (const QQmlProfilerData &data, m_client->asynchronousMessages) {
            qDebug() << i++ << data.time << data.messageType << data.detailType << data.detailData
                     << data.framerate << data.animationcount << data.line << data.column;
        }
        qDebug() << " ";
        qDebug() << "Pixmap Cache Messages:" << m_client->pixmapMessages.count();
        i = 0;
        foreach (const QQmlProfilerData &data, m_client->pixmapMessages) {
            qDebug() << i++ << data.time << data.messageType << data.detailType << data.detailData
                     << data.line << data.column;
        }
        qDebug() << " ";
        qDebug() << "Javascript Heap Messages:" << m_client->jsHeapMessages.count();
        i = 0;
        foreach (const QQmlProfilerData &data, m_client->jsHeapMessages) {
            qDebug() << i++ << data.time << data.messageType << data.detailType;
        }
        qDebug() << " ";
        qDebug() << "Process State:" << (m_process ? m_process->state() : QLatin1String("null"));
        qDebug() << "Application Output:" << (m_process ? m_process->output() : QLatin1String("null"));
        qDebug() << "Connection State:" << QQmlDebugTest::connectionStateString(m_connection);
        qDebug() << "Client State:" << QQmlDebugTest::clientStateString(m_client);
    }
    delete m_process;
    m_process = 0;
    delete m_client;
    m_client = 0;
    delete m_connection;
    m_connection = 0;
}

void tst_QQmlProfilerService::connect_data()
{
    QTest::addColumn<bool>("blockMode");
    QTest::addColumn<bool>("restrictMode");
    QTest::addColumn<bool>("traceEnabled");
    QTest::newRow("normal/unrestricted/disabled") << false << false << false;
    QTest::newRow("block/unrestricted/disabled") << true << false << false;
    QTest::newRow("normal/restricted/disabled") << false << true << false;
    QTest::newRow("block/restricted/disabled") << true << true << false;
    QTest::newRow("normal/unrestricted/enabled") << false << false << true;
    QTest::newRow("block/unrestricted/enabled") << true << false << true;
    QTest::newRow("normal/restricted/enabled") << false << true << true;
    QTest::newRow("block/restricted/enabled") << true << true << true;
}

void tst_QQmlProfilerService::connect()
{
    QFETCH(bool, blockMode);
    QFETCH(bool, restrictMode);
    QFETCH(bool, traceEnabled);

    connect(blockMode, "test.qml", restrictMode);

    // if the engine is waiting, then the first message determines if it starts with trace enabled
    if (!traceEnabled)
        m_client->sendRecordingStatus(false);
    m_client->sendRecordingStatus(true);
    m_client->sendRecordingStatus(false);
    checkTraceReceived();
    checkJsHeap();
}

void tst_QQmlProfilerService::pixmapCacheData()
{
    connect(true, "pixmapCacheTest.qml");

    m_client->sendRecordingStatus(true);
    QVERIFY(QQmlDebugTest::waitForSignal(m_process, SIGNAL(readyReadStandardOutput())));

    while (m_process->output().indexOf(QLatin1String("image loaded")) == -1 &&
           m_process->output().indexOf(QLatin1String("image error")) == -1)
        QVERIFY(QQmlDebugTest::waitForSignal(m_process, SIGNAL(readyReadStandardOutput())));

    m_client->sendRecordingStatus(false);

    checkTraceReceived();
    checkJsHeap();

    QQmlProfilerData expected(0, QQmlProfilerDefinitions::PixmapCacheEvent);

    // image starting to load
    expected.detailType = QQmlProfilerDefinitions::PixmapLoadingStarted;
    VERIFY(MessageListPixmap, 0, expected, CheckMessageType | CheckDetailType);

    // image size
    expected.detailType = QQmlProfilerDefinitions::PixmapSizeKnown;
    expected.line = expected.column = 2; // width and height, in fact
    VERIFY(MessageListPixmap, 1, expected,
           CheckMessageType | CheckDetailType | CheckLine | CheckColumn);

    // image loaded
    expected.detailType = QQmlProfilerDefinitions::PixmapLoadingFinished;
    VERIFY(MessageListPixmap, 2, expected, CheckMessageType | CheckDetailType);

    // cache size
    expected.detailType = QQmlProfilerDefinitions::PixmapCacheCountChanged;
    VERIFY(MessageListPixmap, 3, expected, CheckMessageType | CheckDetailType);
}

void tst_QQmlProfilerService::scenegraphData()
{
    connect(true, "scenegraphTest.qml");

    m_client->sendRecordingStatus(true);

    while (!m_process->output().contains(QLatin1String("tick")))
        QVERIFY(QQmlDebugTest::waitForSignal(m_process, SIGNAL(readyReadStandardOutput())));
    m_client->sendRecordingStatus(false);

    checkTraceReceived();
    checkJsHeap();

    // Check that at least one frame was rendered.
    // There should be a SGContextFrame + SGRendererFrame + SGRenderLoopFrame sequence,
    // but we can't be sure to get the SGRenderLoopFrame in the threaded renderer.
    //
    // Since the rendering happens in a different thread, there could be other unrelated events
    // interleaved. Also, events could carry the same time stamps and be sorted in an unexpected way
    // if the clocks are acting up.
    qint64 contextFrameTime = -1;
    qint64 renderFrameTime = -1;
#ifndef QT_NO_OPENGL //Software renderer doesn't have context frames
    foreach (const QQmlProfilerData &msg, m_client->asynchronousMessages) {
        if (msg.messageType == QQmlProfilerDefinitions::SceneGraphFrame) {
            if (msg.detailType == QQmlProfilerDefinitions::SceneGraphContextFrame) {
                contextFrameTime = msg.time;
                break;
            }
        }
    }

    QVERIFY(contextFrameTime != -1);
#endif
    foreach (const QQmlProfilerData &msg, m_client->asynchronousMessages) {
        if (msg.detailType == QQmlProfilerDefinitions::SceneGraphRendererFrame) {
            QVERIFY(msg.time >= contextFrameTime);
            renderFrameTime = msg.time;
            break;
        }
    }

    QVERIFY(renderFrameTime != -1);

    foreach (const QQmlProfilerData &msg, m_client->asynchronousMessages) {
        if (msg.detailType == QQmlProfilerDefinitions::SceneGraphRenderLoopFrame) {
            if (msg.time >= contextFrameTime) {
                // Make sure SceneGraphRenderLoopFrame is not between SceneGraphContextFrame and
                // SceneGraphRendererFrame. A SceneGraphRenderLoopFrame before everything else is
                // OK as the scene graph might decide to do an initial rendering.
                QVERIFY(msg.time >= renderFrameTime);
                break;
            }
        }
    }
}

void tst_QQmlProfilerService::profileOnExit()
{
    connect(true, "exit.qml");

    m_client->sendRecordingStatus(true);

    checkTraceReceived();
    checkJsHeap();
}

void tst_QQmlProfilerService::controlFromJS()
{
    connect(true, "controlFromJS.qml");

    m_client->sendRecordingStatus(false);
    checkTraceReceived();
    checkJsHeap();
}

void tst_QQmlProfilerService::signalSourceLocation()
{
    connect(true, "signalSourceLocation.qml");

    m_client->sendRecordingStatus(true);
    while (!(m_process->output().contains(QLatin1String("500"))))
        QVERIFY(QQmlDebugTest::waitForSignal(m_process, SIGNAL(readyReadStandardOutput())));
    m_client->sendRecordingStatus(false);
    checkTraceReceived();
    checkJsHeap();

    QQmlProfilerData expected(0, QQmlProfilerDefinitions::RangeLocation,
                              QQmlProfilerDefinitions::HandlingSignal,
                              QLatin1String("signalSourceLocation.qml"));
    expected.line = 8;
    expected.column = 28;
    VERIFY(MessageListQML, 9, expected, CheckAll);

    expected.line = 7;
    expected.column = 21;
    VERIFY(MessageListQML, 11, expected, CheckAll);
}

void tst_QQmlProfilerService::javascript()
{
    connect(true, "javascript.qml");

    m_client->sendRecordingStatus(true);
    while (!(m_process->output().contains(QLatin1String("done"))))
        QVERIFY(QQmlDebugTest::waitForSignal(m_process, SIGNAL(readyReadStandardOutput())));
    m_client->sendRecordingStatus(false);
    checkTraceReceived();
    checkJsHeap();

    QQmlProfilerData expected(0, QQmlProfilerDefinitions::RangeStart,
                              QQmlProfilerDefinitions::Javascript);
    VERIFY(MessageListJavaScript, 6, expected, CheckMessageType | CheckDetailType);

    expected.messageType = QQmlProfilerDefinitions::RangeLocation;
    expected.detailData = QLatin1String("javascript.qml");
    expected.line = 4;
    expected.column = 5;
    VERIFY(MessageListJavaScript, 7, expected, CheckAll);

    expected.messageType = QQmlProfilerDefinitions::RangeData;
    expected.detailData = QLatin1String("something");
    VERIFY(MessageListJavaScript, 8, expected,
           CheckMessageType | CheckDetailType | CheckDataEndsWith);

    expected.messageType = QQmlProfilerDefinitions::RangeEnd;
    VERIFY(MessageListJavaScript, 21, expected, CheckMessageType | CheckDetailType);
}

void tst_QQmlProfilerService::flushInterval()
{
    connect(true, "timer.qml");

    m_client->sendRecordingStatus(true, -1, 1);

    // Make sure we get multiple messages
    QTRY_VERIFY(m_client->qmlMessages.length() > 0);
    QVERIFY(m_client->qmlMessages.length() < 100);
    QTRY_VERIFY(m_client->qmlMessages.length() > 100);

    m_client->sendRecordingStatus(false);
    checkTraceReceived();
    checkJsHeap();
}

QTEST_MAIN(tst_QQmlProfilerService)

#include "tst_qqmlprofilerservice.moc"
