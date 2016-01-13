/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qmlprofilerclient.h"

#include <QtCore/QStack>
#include <QtCore/QStringList>

ProfilerClient::ProfilerClient(const QString &clientName,
                             QQmlDebugConnection *client)
    : QQmlDebugClient(clientName, client),
      m_recording(false),
      m_enabled(false)
{
}

ProfilerClient::~ProfilerClient()
{
    //Disable profiling if started by client
    //Profiling data will be lost!!
    if (isRecording())
        setRecording(false);
}

void ProfilerClient::clearData()
{
    emit cleared();
}

bool ProfilerClient::isEnabled() const
{
    return m_enabled;
}

void ProfilerClient::sendRecordingStatus()
{
}

bool ProfilerClient::isRecording() const
{
    return m_recording;
}

void ProfilerClient::setRecording(bool v)
{
    if (v == m_recording)
        return;

    m_recording = v;

    if (state() == Enabled) {
        sendRecordingStatus();
    }

    emit recordingChanged(v);
}

void ProfilerClient::stateChanged(State status)
{
    if ((m_enabled && status != Enabled) ||
            (!m_enabled && status == Enabled))
        emit enabledChanged();

    m_enabled = status == Enabled;

}

class QmlProfilerClientPrivate
{
public:
    QmlProfilerClientPrivate()
        : inProgressRanges(0)
        , maximumTime(0)
    {
        ::memset(rangeCount, 0,
                 QQmlProfilerService::MaximumRangeType * sizeof(int));
    }

    qint64 inProgressRanges;
    QStack<qint64> rangeStartTimes[QQmlProfilerService::MaximumRangeType];
    QStack<QStringList> rangeDatas[QQmlProfilerService::MaximumRangeType];
    QStack<QmlEventLocation> rangeLocations[QQmlProfilerService::MaximumRangeType];
    QStack<QQmlProfilerService::BindingType> bindingTypes;
    int rangeCount[QQmlProfilerService::MaximumRangeType];
    qint64 maximumTime;
};

QmlProfilerClient::QmlProfilerClient(
        QQmlDebugConnection *client)
    : ProfilerClient(QStringLiteral("CanvasFrameRate"), client),
      d(new QmlProfilerClientPrivate)
{
}

QmlProfilerClient::~QmlProfilerClient()
{
    delete d;
}

void QmlProfilerClient::clearData()
{
    ::memset(d->rangeCount, 0,
             QQmlProfilerService::MaximumRangeType * sizeof(int));
    d->bindingTypes.clear();
    ProfilerClient::clearData();
}

void QmlProfilerClient::sendRecordingStatus()
{
    QByteArray ba;
    QDataStream stream(&ba, QIODevice::WriteOnly);
    stream << isRecording();
    sendMessage(ba);
}

void QmlProfilerClient::messageReceived(const QByteArray &data)
{
    QByteArray rwData = data;
    QDataStream stream(&rwData, QIODevice::ReadOnly);

    qint64 time;
    int messageType;

    stream >> time >> messageType;

    if (messageType >= QQmlProfilerService::MaximumMessage)
        return;

    if (messageType == QQmlProfilerService::Event) {
        int event;
        stream >> event;

        if (event == QQmlProfilerService::EndTrace) {
            emit this->traceFinished(time);
            d->maximumTime = time;
            d->maximumTime = qMax(time, d->maximumTime);
        } else if (event == QQmlProfilerService::AnimationFrame) {
            int frameRate, animationCount;
            int threadId = 0;
            stream >> frameRate >> animationCount;
            if (!stream.atEnd())
                stream >> threadId;
            emit this->frame(time, frameRate, animationCount, threadId);
            d->maximumTime = qMax(time, d->maximumTime);
        } else if (event == QQmlProfilerService::StartTrace) {
            emit this->traceStarted(time);
            d->maximumTime = time;
        } else if (event < QQmlProfilerService::MaximumEventType) {
            d->maximumTime = qMax(time, d->maximumTime);
        }
    } else if (messageType == QQmlProfilerService::Complete) {
        emit complete();
    } else if (messageType == QQmlProfilerService::SceneGraphFrame) {
        int sgEventType;
        int count = 0;
        qint64 params[5];

        stream >> sgEventType;
        while (!stream.atEnd()) {
            stream >> params[count++];
        }
        while (count<5)
            params[count++] = 0;
        emit sceneGraphFrame((QQmlProfilerService::SceneGraphFrameType)sgEventType, time,
                             params[0], params[1], params[2], params[3], params[4]);
        d->maximumTime = qMax(time, d->maximumTime);
    } else if (messageType == QQmlProfilerService::PixmapCacheEvent) {
        int pixEvTy, width = 0, height = 0, refcount = 0;
        QString pixUrl;
        stream >> pixEvTy >> pixUrl;
        if (pixEvTy == (int)QQmlProfilerService::PixmapReferenceCountChanged ||
                pixEvTy == (int)QQmlProfilerService::PixmapCacheCountChanged) {
            stream >> refcount;
        } else if (pixEvTy == (int)QQmlProfilerService::PixmapSizeKnown) {
            stream >> width >> height;
            refcount = 1;
        }
        emit pixmapCache((QQmlProfilerService::PixmapEventType)pixEvTy, time,
                         QmlEventLocation(pixUrl,0,0), width, height, refcount);
        d->maximumTime = qMax(time, d->maximumTime);
    } else if (messageType == QQmlProfilerService::MemoryAllocation) {
        int type;
        qint64 delta;
        stream >> type >> delta;
        emit memoryAllocation((QQmlProfilerService::MemoryType)type, time, delta);
        d->maximumTime = qMax(time, d->maximumTime);
    } else {
        int range;
        stream >> range;

        if (range >= QQmlProfilerService::MaximumRangeType)
            return;

        if (messageType == QQmlProfilerService::RangeStart) {
            d->rangeStartTimes[range].push(time);
            d->inProgressRanges |= (static_cast<qint64>(1) << range);
            ++d->rangeCount[range];

            // read binding type
            if (range == (int)QQmlProfilerService::Binding) {
                int bindingType = (int)QQmlProfilerService::QmlBinding;
                if (!stream.atEnd())
                    stream >> bindingType;
                d->bindingTypes.push((QQmlProfilerService::BindingType)bindingType);
            }
        } else if (messageType == QQmlProfilerService::RangeData) {
            QString data;
            stream >> data;

            int count = d->rangeCount[range];
            if (count > 0) {
                while (d->rangeDatas[range].count() < count)
                    d->rangeDatas[range].push(QStringList());
                d->rangeDatas[range][count-1] << data;
            }

        } else if (messageType == QQmlProfilerService::RangeLocation) {
            QString fileName;
            int line;
            int column = -1;
            stream >> fileName >> line;

            if (!stream.atEnd())
                stream >> column;

            if (d->rangeCount[range] > 0) {
                d->rangeLocations[range].push(QmlEventLocation(fileName, line,
                                                            column));
            }
        } else {
            if (d->rangeCount[range] > 0) {
                --d->rangeCount[range];
                if (d->inProgressRanges & (static_cast<qint64>(1) << range))
                    d->inProgressRanges &= ~(static_cast<qint64>(1) << range);

                d->maximumTime = qMax(time, d->maximumTime);
                QStringList data = d->rangeDatas[range].count() ?
                            d->rangeDatas[range].pop() : QStringList();
                QmlEventLocation location = d->rangeLocations[range].count() ?
                            d->rangeLocations[range].pop() : QmlEventLocation();

                qint64 startTime = d->rangeStartTimes[range].pop();
                QQmlProfilerService::BindingType bindingType = QQmlProfilerService::QmlBinding;
                if (range == (int)QQmlProfilerService::Binding)
                    bindingType = d->bindingTypes.pop();
                emit this->range((QQmlProfilerService::RangeType)range,
                                 bindingType, startTime, time - startTime, data, location);
                if (d->rangeCount[range] == 0) {
                    int count = d->rangeDatas[range].count() +
                                d->rangeStartTimes[range].count() +
                                d->rangeLocations[range].count();
                    if (count != 0)
                        qWarning() << "incorrectly nested data";
                }
            }
        }
    }
}

V8ProfilerClient::V8ProfilerClient(QQmlDebugConnection *client)
    : ProfilerClient(QStringLiteral("V8Profiler"), client)
{
}

V8ProfilerClient::~V8ProfilerClient()
{
}

void V8ProfilerClient::sendRecordingStatus()
{
    QByteArray ba;
    QDataStream stream(&ba, QIODevice::WriteOnly);
    QByteArray cmd("V8PROFILER");
    QByteArray option("");
    QByteArray title("");

    if (m_recording) {
        option = "start";
    } else {
        option = "stop";
    }
    stream << cmd << option << title;
    sendMessage(ba);
}

void V8ProfilerClient::messageReceived(const QByteArray &data)
{
    QByteArray rwData = data;
    QDataStream stream(&rwData, QIODevice::ReadOnly);

    int messageType;

    stream >> messageType;

    if (messageType == V8Complete) {
        emit complete();
    } else if (messageType == V8Entry) {
        QString filename;
        QString function;
        int lineNumber;
        double totalTime;
        double selfTime;
        int depth;

        stream  >> filename >> function >> lineNumber >> totalTime >>
                   selfTime >> depth;
        emit this->range(depth, function, filename, lineNumber, totalTime,
                         selfTime);
    }
}

