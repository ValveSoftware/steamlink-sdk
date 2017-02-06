/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlprofilerclient_p_p.h"
#include "qqmldebugconnection_p.h"
#include <private/qqmldebugserviceinterfaces_p.h>

QT_BEGIN_NAMESPACE

QQmlProfilerClient::QQmlProfilerClient(QQmlDebugConnection *connection) :
    QQmlDebugClient(*(new QQmlProfilerClientPrivate(connection)))
{
}

QQmlProfilerClient::QQmlProfilerClient(QQmlProfilerClientPrivate &dd) :
    QQmlDebugClient(dd)
{
}

QQmlProfilerClientPrivate::QQmlProfilerClientPrivate(QQmlDebugConnection *connection) :
    QQmlDebugClientPrivate(QQmlProfilerService::s_key, connection),
    features(std::numeric_limits<quint64>::max())
{
}

void QQmlProfilerClient::setFeatures(quint64 features)
{
    Q_D(QQmlProfilerClient);
    d->features = features;
}

void QQmlProfilerClient::sendRecordingStatus(bool record, int engineId, quint32 flushInterval)
{
    Q_D(const QQmlProfilerClient);

    QPacket stream(d->connection->currentDataStreamVersion());
    stream << record << engineId << d->features << flushInterval << true;
    sendMessage(stream.data());
}

void QQmlProfilerClient::traceStarted(qint64 time, int engineId)
{
    Q_UNUSED(time);
    Q_UNUSED(engineId);
}

void QQmlProfilerClient::traceFinished(qint64 time, int engineId)
{
    Q_UNUSED(time);
    Q_UNUSED(engineId);
}

void QQmlProfilerClient::rangeStart(QQmlProfilerDefinitions::RangeType type, qint64 startTime)
{
    Q_UNUSED(type);
    Q_UNUSED(startTime);
}

void QQmlProfilerClient::rangeData(QQmlProfilerDefinitions::RangeType type, qint64 time,
                                   const QString &data)
{
    Q_UNUSED(type);
    Q_UNUSED(time);
    Q_UNUSED(data);
}

void QQmlProfilerClient::rangeLocation(QQmlProfilerDefinitions::RangeType type, qint64 time,
                                       const QQmlEventLocation &location)
{
    Q_UNUSED(type);
    Q_UNUSED(time);
    Q_UNUSED(location);
}

void QQmlProfilerClient::rangeEnd(QQmlProfilerDefinitions::RangeType type, qint64 endTime)
{
    Q_UNUSED(type);
    Q_UNUSED(endTime);
}

void QQmlProfilerClient::animationFrame(qint64 time, int frameRate, int animationCount,
                                        int threadId)
{
    Q_UNUSED(time);
    Q_UNUSED(frameRate);
    Q_UNUSED(animationCount);
    Q_UNUSED(threadId);
}

void QQmlProfilerClient::sceneGraphEvent(QQmlProfilerDefinitions::SceneGraphFrameType type,
                                         qint64 time, qint64 numericData1, qint64 numericData2,
                                         qint64 numericData3, qint64 numericData4,
                                         qint64 numericData5)
{
    Q_UNUSED(type);
    Q_UNUSED(time);
    Q_UNUSED(numericData1);
    Q_UNUSED(numericData2);
    Q_UNUSED(numericData3);
    Q_UNUSED(numericData4);
    Q_UNUSED(numericData5);
}

void QQmlProfilerClient::pixmapCacheEvent(QQmlProfilerDefinitions::PixmapEventType type,
                                          qint64 time, const QString &url, int numericData1,
                                          int numericData2)
{
    Q_UNUSED(type);
    Q_UNUSED(time);
    Q_UNUSED(url);
    Q_UNUSED(numericData1);
    Q_UNUSED(numericData2);
}

void QQmlProfilerClient::memoryAllocation(QQmlProfilerDefinitions::MemoryType type, qint64 time,
                                          qint64 amount)
{
    Q_UNUSED(type);
    Q_UNUSED(time);
    Q_UNUSED(amount);
}

void QQmlProfilerClient::inputEvent(QQmlProfilerDefinitions::InputEventType type, qint64 time,
                                    int a, int b)
{
    Q_UNUSED(type);
    Q_UNUSED(time);
    Q_UNUSED(a);
    Q_UNUSED(b);
}

void QQmlProfilerClient::complete()
{
}

void QQmlProfilerClient::unknownEvent(QQmlProfilerDefinitions::Message messageType, qint64 time,
                                      int detailType)
{
    Q_UNUSED(messageType);
    Q_UNUSED(time);
    Q_UNUSED(detailType);
}

void QQmlProfilerClient::unknownData(QPacket &stream)
{
    Q_UNUSED(stream);
}

inline QQmlProfilerDefinitions::ProfileFeature featureFromRangeType(
        QQmlProfilerDefinitions::RangeType range)
{
    switch (range) {
        case QQmlProfilerDefinitions::Painting:
            return QQmlProfilerDefinitions::ProfilePainting;
        case QQmlProfilerDefinitions::Compiling:
            return QQmlProfilerDefinitions::ProfileCompiling;
        case QQmlProfilerDefinitions::Creating:
            return QQmlProfilerDefinitions::ProfileCreating;
        case QQmlProfilerDefinitions::Binding:
            return QQmlProfilerDefinitions::ProfileBinding;
        case QQmlProfilerDefinitions::HandlingSignal:
            return QQmlProfilerDefinitions::ProfileHandlingSignal;
        case QQmlProfilerDefinitions::Javascript:
            return QQmlProfilerDefinitions::ProfileJavaScript;
        default:
            return QQmlProfilerDefinitions::MaximumProfileFeature;
    }
}

void QQmlProfilerClient::messageReceived(const QByteArray &data)
{
    Q_D(QQmlProfilerClient);

    QPacket stream(d->connection->currentDataStreamVersion(), data);

    // Force all the 1 << <FLAG> expressions to be done in 64 bit, to silence some warnings
    const quint64 one = static_cast<quint64>(1);

    qint64 time;
    int messageType;

    stream >> time >> messageType;

    if (messageType >= QQmlProfilerDefinitions::MaximumMessage) {
        unknownEvent(static_cast<QQmlProfilerDefinitions::Message>(messageType), time, -1);
        return;
    }

    if (messageType == QQmlProfilerDefinitions::Event) {
        int type;
        stream >> type;

        QQmlProfilerDefinitions::EventType eventType =
                static_cast<QQmlProfilerDefinitions::EventType>(type);

        if (eventType == QQmlProfilerDefinitions::EndTrace) {
            int engineId = -1;
            if (!stream.atEnd())
                stream >> engineId;
            traceFinished(time, engineId);
        } else if (eventType == QQmlProfilerDefinitions::AnimationFrame) {
            if (!(d->features & one << QQmlProfilerDefinitions::ProfileAnimations))
                return;

            int frameRate, animationCount;
            int threadId = 0;
            stream >> frameRate >> animationCount;
            if (!stream.atEnd())
                stream >> threadId;

            animationFrame(time, frameRate, animationCount, threadId);
        } else if (type == QQmlProfilerDefinitions::StartTrace) {
            int engineId = -1;
            if (!stream.atEnd())
                stream >> engineId;
            traceStarted(time, engineId);
        } else if (eventType == QQmlProfilerDefinitions::Key ||
                   eventType == QQmlProfilerDefinitions::Mouse) {

            if (!(d->features & one << QQmlProfilerDefinitions::ProfileInputEvents))
                return;

            int type;
            if (!stream.atEnd()) {
                stream >> type;
            } else {
                type = (eventType == QQmlProfilerDefinitions::Key) ?
                            QQmlProfilerDefinitions::InputKeyUnknown :
                            QQmlProfilerDefinitions::InputMouseUnknown;
            }

            int a = 0;
            if (!stream.atEnd())
                stream >> a;

            int b = 0;
            if (!stream.atEnd())
                stream >> b;

            inputEvent(static_cast<QQmlProfilerDefinitions::InputEventType>(type), time, a, b);
        } else {
            unknownEvent(QQmlProfilerDefinitions::Event, time, type);
        }
    } else if (messageType == QQmlProfilerDefinitions::Complete) {
        complete();
    } else if (messageType == QQmlProfilerDefinitions::SceneGraphFrame) {
        if (!(d->features & one << QQmlProfilerDefinitions::ProfileSceneGraph))
            return;

        int type;
        int count = 0;
        qint64 params[5];

        stream >> type;
        while (!stream.atEnd())
            stream >> params[count++];

        while (count < 5)
            params[count++] = 0;

        sceneGraphEvent(static_cast<QQmlProfilerDefinitions::SceneGraphFrameType>(type), time,
                        params[0], params[1], params[2], params[3], params[4]);
    } else if (messageType == QQmlProfilerDefinitions::PixmapCacheEvent) {
        if (!(d->features & one << QQmlProfilerDefinitions::ProfilePixmapCache))
            return;

        int type, param1 = 0, param2 = 0;
        QString pixUrl;
        stream >> type >> pixUrl;

        QQmlProfilerDefinitions::PixmapEventType pixmapEventType =
                static_cast<QQmlProfilerDefinitions::PixmapEventType>(type);

        if (pixmapEventType == QQmlProfilerDefinitions::PixmapReferenceCountChanged ||
                pixmapEventType == QQmlProfilerDefinitions::PixmapCacheCountChanged) {
            stream >> param1;
        } else if (pixmapEventType == QQmlProfilerDefinitions::PixmapSizeKnown) {
            stream >> param1 >> param2;
        }

        pixmapCacheEvent(pixmapEventType, time, pixUrl, param1, param2);
    } else if (messageType == QQmlProfilerDefinitions::MemoryAllocation) {
        if (!(d->features & one << QQmlProfilerDefinitions::ProfileMemory))
            return;
        int type;
        qint64 delta;
        stream >> type >> delta;
        memoryAllocation((QQmlProfilerDefinitions::MemoryType)type, time, delta);
    } else {
        int range;
        stream >> range;

        QQmlProfilerDefinitions::RangeType rangeType =
                static_cast<QQmlProfilerDefinitions::RangeType>(range);

        if (range >= QQmlProfilerDefinitions::MaximumRangeType ||
                !(d->features & one << featureFromRangeType(rangeType)))
            return;

        qint64 typeId = 0;
        if (messageType == QQmlProfilerDefinitions::RangeStart) {
            rangeStart(rangeType, time);
            if (!stream.atEnd()) {
                stream >> typeId;
                auto i = d->types.constFind(typeId);
                if (i != d->types.constEnd()) {
                    rangeLocation(rangeType, time, i->location);
                    rangeData(rangeType, time, i->name);
                }
            }
        } else if (messageType == QQmlProfilerDefinitions::RangeData) {
            QString data;
            stream >> data;
            rangeData(rangeType, time, data);
            if (!stream.atEnd()) {
                stream >> typeId;
                d->types[typeId].name = data;
            }
        } else if (messageType == QQmlProfilerDefinitions::RangeLocation) {
            QQmlEventLocation location;
            stream >> location.filename >> location.line;

            if (!stream.atEnd())
                stream >> location.column;

            rangeLocation(rangeType, time, location);
            if (!stream.atEnd()) {
                stream >> typeId;
                d->types[typeId].location = location;
            }
        } else if (messageType == QQmlProfilerDefinitions::RangeEnd) {
            rangeEnd(rangeType, time);
        } else {
            unknownEvent(static_cast<QQmlProfilerDefinitions::Message>(messageType), time, range);
        }
    }

    if (!stream.atEnd())
        unknownData(stream);
}
QT_END_NAMESPACE
