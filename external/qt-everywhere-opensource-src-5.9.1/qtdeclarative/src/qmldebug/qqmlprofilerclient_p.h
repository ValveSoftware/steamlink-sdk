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

#ifndef QQMLPROFILERCLIENT_P_H
#define QQMLPROFILERCLIENT_P_H

#include "qqmldebugclient_p.h"
#include "qqmleventlocation_p.h"
#include <private/qqmlprofilerdefinitions_p.h>
#include <private/qpacket_p.h>

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

QT_BEGIN_NAMESPACE

class QQmlProfilerClientPrivate;
class QQmlProfilerClient : public QQmlDebugClient
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QQmlProfilerClient)

public:
    QQmlProfilerClient(QQmlDebugConnection *connection);
    void setFeatures(quint64 features);
    void sendRecordingStatus(bool record, int engineId = -1, quint32 flushInterval = 0);

protected:
    QQmlProfilerClient(QQmlProfilerClientPrivate &dd);

private:
    void messageReceived(const QByteArray &message) override;

    virtual void traceStarted(qint64 time, int engineId);
    virtual void traceFinished(qint64 time, int engineId);

    virtual void rangeStart(QQmlProfilerDefinitions::RangeType type, qint64 startTime);
    virtual void rangeData(QQmlProfilerDefinitions::RangeType type, qint64 time,
                           const QString &data);
    virtual void rangeLocation(QQmlProfilerDefinitions::RangeType type, qint64 time,
                               const QQmlEventLocation &location);
    virtual void rangeEnd(QQmlProfilerDefinitions::RangeType type, qint64 endTime);

    virtual void animationFrame(qint64 time, int frameRate, int animationCount, int threadId);

    virtual void sceneGraphEvent(QQmlProfilerDefinitions::SceneGraphFrameType type, qint64 time,
                                 qint64 numericData1, qint64 numericData2, qint64 numericData3,
                                 qint64 numericData4, qint64 numericData5);

    virtual void pixmapCacheEvent(QQmlProfilerDefinitions::PixmapEventType type, qint64 time,
                                  const QString &url, int numericData1, int numericData2);

    virtual void memoryAllocation(QQmlProfilerDefinitions::MemoryType type, qint64 time,
                                  qint64 amount);

    virtual void inputEvent(QQmlProfilerDefinitions::InputEventType type, qint64 time, int a,
                            int b);

    virtual void complete();

    virtual void unknownEvent(QQmlProfilerDefinitions::Message messageType, qint64 time,
                              int detailType);
    virtual void unknownData(QPacket &stream);
};

QT_END_NAMESPACE

#endif // QQMLPROFILERCLIENT_P_H
