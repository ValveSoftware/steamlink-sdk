/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QMLPROFILERCLIENT_H
#define QMLPROFILERCLIENT_H

#include <private/qqmleventlocation_p.h>
#include <private/qqmlprofilerclient_p.h>
#include <private/qqmlprofilerdefinitions_p.h>

class QmlProfilerData;
class QmlProfilerClientPrivate;
class QmlProfilerClient : public QQmlProfilerClient
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QmlProfilerClient)

public:
    QmlProfilerClient(QQmlDebugConnection *connection, QmlProfilerData *data);
    void clearPendingData();

signals:
    void enabledChanged(bool enabled);
    void recordingStarted();
    void error(const QString &error);

private:
    virtual void stateChanged(State state);

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
};

#endif // QMLPROFILERCLIENT_H
