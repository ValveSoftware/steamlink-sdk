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

#include "qqmlprofiler_p.h"
#include "qqmlprofilerservice_p.h"
#include "qqmldebugservice_p.h"

QT_BEGIN_NAMESPACE

// convert to QByteArrays that can be sent to the debug client
// use of QDataStream can skew results
//     (see tst_qqmldebugtrace::trace() benchmark)
void QQmlProfilerData::toByteArrays(QList<QByteArray> &messages) const
{
    QByteArray data;
    Q_ASSERT_X(((messageType | detailType) & (1 << 31)) == 0, Q_FUNC_INFO, "You can use at most 31 message types and 31 detail types.");
    for (uint decodedMessageType = 0; (messageType >> decodedMessageType) != 0; ++decodedMessageType) {
        if ((messageType & (1 << decodedMessageType)) == 0)
            continue;

        for (uint decodedDetailType = 0; (detailType >> decodedDetailType) != 0; ++decodedDetailType) {
            if ((detailType & (1 << decodedDetailType)) == 0)
                continue;

            //### using QDataStream is relatively expensive
            QQmlDebugStream ds(&data, QIODevice::WriteOnly);
            ds << time << decodedMessageType << decodedDetailType;

            switch (decodedMessageType) {
            case QQmlProfilerDefinitions::RangeStart:
                if (decodedDetailType == (int)QQmlProfilerDefinitions::Binding)
                    ds << QQmlProfilerDefinitions::QmlBinding;
                break;
            case QQmlProfilerDefinitions::RangeData:
                ds << detailString;
                break;
            case QQmlProfilerDefinitions::RangeLocation:
                ds << (detailUrl.isEmpty() ? detailString : detailUrl.toString()) << x << y;
                break;
            case QQmlProfilerDefinitions::RangeEnd: break;
            default:
                Q_ASSERT_X(false, Q_FUNC_INFO, "Invalid message type.");
                break;
            }
            messages << data;
            data.clear();
        }
    }
}

QQmlProfilerAdapter::QQmlProfilerAdapter(QQmlProfilerService *service, QQmlEnginePrivate *engine) :
    QQmlAbstractProfilerAdapter(service)
{
    engine->enableProfiler();
    connect(this, SIGNAL(profilingEnabled(quint64)), engine->profiler, SLOT(startProfiling(quint64)));
    connect(this, SIGNAL(profilingEnabledWhileWaiting(quint64)),
            engine->profiler, SLOT(startProfiling(quint64)), Qt::DirectConnection);
    connect(this, SIGNAL(profilingDisabled()), engine->profiler, SLOT(stopProfiling()));
    connect(this, SIGNAL(profilingDisabledWhileWaiting()),
            engine->profiler, SLOT(stopProfiling()), Qt::DirectConnection);
    connect(this, SIGNAL(dataRequested()), engine->profiler, SLOT(reportData()));
    connect(this, SIGNAL(referenceTimeKnown(QElapsedTimer)),
            engine->profiler, SLOT(setTimer(QElapsedTimer)));
    connect(engine->profiler, SIGNAL(dataReady(QList<QQmlProfilerData>)),
            this, SLOT(receiveData(QList<QQmlProfilerData>)));
}

qint64 QQmlProfilerAdapter::sendMessages(qint64 until, QList<QByteArray> &messages)
{
    while (!data.empty() && data.front().time <= until) {
        data.front().toByteArrays(messages);
        data.pop_front();
    }
    return data.empty() ? -1 : data.front().time;
}

void QQmlProfilerAdapter::receiveData(const QList<QQmlProfilerData> &new_data)
{
    data = new_data;
    service->dataReady(this);
}


QQmlProfiler::QQmlProfiler() : featuresEnabled(0)
{
    static int metatype = qRegisterMetaType<QList<QQmlProfilerData> >();
    Q_UNUSED(metatype);
    m_timer.start();
}

void QQmlProfiler::startProfiling(quint64 features)
{
    featuresEnabled = features;
}

void QQmlProfiler::stopProfiling()
{
    featuresEnabled = false;
    reportData();
    m_data.clear();
}

void QQmlProfiler::reportData()
{
    QList<QQmlProfilerData> result;
    result.reserve(m_data.size());
    for (int i = 0; i < m_data.size(); ++i)
        result.append(m_data[i]);
    emit dataReady(result);
}

QT_END_NAMESPACE
