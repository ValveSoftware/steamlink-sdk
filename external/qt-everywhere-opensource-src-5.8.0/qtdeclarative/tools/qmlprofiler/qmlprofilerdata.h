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

#ifndef QMLPROFILERDATA_H
#define QMLPROFILERDATA_H

#include <private/qqmleventlocation_p.h>
#include <private/qqmlprofilerdefinitions_p.h>

#include <QObject>

class QmlProfilerDataPrivate;
class QmlProfilerData : public QObject
{
    Q_OBJECT
public:
    enum State {
        Empty,
        AcquiringData,
        ProcessingData,
        Done
    };

    explicit QmlProfilerData(QObject *parent = 0);
    ~QmlProfilerData();

    static QString getHashStringForQmlEvent(const QQmlEventLocation &location, int eventType);
    static QString qmlRangeTypeAsString(QQmlProfilerDefinitions::RangeType type);
    static QString qmlMessageAsString(QQmlProfilerDefinitions::Message type);

    qint64 traceStartTime() const;
    qint64 traceEndTime() const;

    bool isEmpty() const;

    void clear();
    void setTraceEndTime(qint64 time);
    void setTraceStartTime(qint64 time);
    void addQmlEvent(QQmlProfilerDefinitions::RangeType type,
                     QQmlProfilerDefinitions::BindingType bindingType,
                     qint64 startTime, qint64 duration, const QStringList &data,
                     const QQmlEventLocation &location);
    void addFrameEvent(qint64 time, int framerate, int animationcount, int threadId);
    void addSceneGraphFrameEvent(QQmlProfilerDefinitions::SceneGraphFrameType type, qint64 time,
                                 qint64 numericData1, qint64 numericData2, qint64 numericData3,
                                 qint64 numericData4, qint64 numericData5);
    void addPixmapCacheEvent(QQmlProfilerDefinitions::PixmapEventType type, qint64 time,
                             const QString &location, int numericData1, int numericData2);
    void addMemoryEvent(QQmlProfilerDefinitions::MemoryType type, qint64 time, qint64 size);
    void addInputEvent(QQmlProfilerDefinitions::InputEventType type, qint64 time, int a, int b);

    void complete();
    bool save(const QString &filename);

signals:
    void error(QString);
    void stateChanged();
    void dataReady();

private:
    void sortStartTimes();
    void computeQmlTime();
    void setState(QmlProfilerData::State state);

private:
    QmlProfilerDataPrivate *d;
};

#endif // QMLPROFILERDATA_H
