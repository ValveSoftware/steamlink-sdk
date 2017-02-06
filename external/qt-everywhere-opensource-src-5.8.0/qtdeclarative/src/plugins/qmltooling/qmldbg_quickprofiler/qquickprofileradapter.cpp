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

#include "qquickprofileradapter.h"
#include "qqmldebugpacket.h"
#include <QCoreApplication>
#include <private/qqmldebugserviceinterfaces_p.h>
#include <private/qquickprofiler_p.h>

QT_BEGIN_NAMESPACE

QQuickProfilerAdapter::QQuickProfilerAdapter(QObject *parent) :
    QQmlAbstractProfilerAdapter(parent), next(0)
{
    QQuickProfiler::initialize(this);

    // We can always do DirectConnection here as all methods are protected by mutexes
    connect(this, &QQmlAbstractProfilerAdapter::profilingEnabled,
            QQuickProfiler::s_instance, &QQuickProfiler::startProfilingImpl, Qt::DirectConnection);
    connect(this, &QQmlAbstractProfilerAdapter::profilingEnabledWhileWaiting,
            QQuickProfiler::s_instance, &QQuickProfiler::startProfilingImpl, Qt::DirectConnection);
    connect(this, &QQmlAbstractProfilerAdapter::referenceTimeKnown,
            QQuickProfiler::s_instance, &QQuickProfiler::setTimer, Qt::DirectConnection);
    connect(this, &QQmlAbstractProfilerAdapter::profilingDisabled,
            QQuickProfiler::s_instance, &QQuickProfiler::stopProfilingImpl, Qt::DirectConnection);
    connect(this, &QQmlAbstractProfilerAdapter::profilingDisabledWhileWaiting,
            QQuickProfiler::s_instance, &QQuickProfiler::stopProfilingImpl, Qt::DirectConnection);
    connect(this, &QQmlAbstractProfilerAdapter::dataRequested,
            QQuickProfiler::s_instance, &QQuickProfiler::reportDataImpl, Qt::DirectConnection);
    connect(QQuickProfiler::s_instance, &QQuickProfiler::dataReady,
            this, &QQuickProfilerAdapter::receiveData, Qt::DirectConnection);
}

QQuickProfilerAdapter::~QQuickProfilerAdapter()
{
    if (service)
        service->removeGlobalProfiler(this);
}

// convert to QByteArrays that can be sent to the debug client
// use of QDataStream can skew results
//     (see tst_qqmldebugtrace::trace() benchmark)
static void qQuickProfilerDataToByteArrays(const QQuickProfilerData &data,
                                           QList<QByteArray> &messages)
{
    QQmlDebugPacket ds;
    Q_ASSERT_X(((data.messageType | data.detailType) & (1 << 31)) == 0, Q_FUNC_INFO,
               "You can use at most 31 message types and 31 detail types.");
    for (uint decodedMessageType = 0; (data.messageType >> decodedMessageType) != 0;
         ++decodedMessageType) {
        if ((data.messageType & (1 << decodedMessageType)) == 0)
            continue;

        for (uint decodedDetailType = 0; (data.detailType >> decodedDetailType) != 0;
             ++decodedDetailType) {
            if ((data.detailType & (1 << decodedDetailType)) == 0)
                continue;

            ds << data.time << decodedMessageType << decodedDetailType;

            switch (decodedMessageType) {
            case QQuickProfiler::Event:
                switch (decodedDetailType) {
                case QQuickProfiler::AnimationFrame:
                    ds << data.framerate << data.count << data.threadId;
                    break;
                case QQuickProfiler::Key:
                case QQuickProfiler::Mouse:
                    ds << data.inputType << data.inputA << data.inputB;
                    break;
                }
                break;
            case QQuickProfiler::PixmapCacheEvent:
                ds << data.detailUrl.toString();
                switch (decodedDetailType) {
                    case QQuickProfiler::PixmapSizeKnown: ds << data.x << data.y; break;
                    case QQuickProfiler::PixmapReferenceCountChanged: ds << data.count; break;
                    case QQuickProfiler::PixmapCacheCountChanged: ds << data.count; break;
                    default: break;
                }
                break;
            case QQuickProfiler::SceneGraphFrame:
                switch (decodedDetailType) {
                    // RendererFrame: preprocessTime, updateTime, bindingTime, renderTime
                    case QQuickProfiler::SceneGraphRendererFrame: ds << data.subtime_1 << data.subtime_2 << data.subtime_3 << data.subtime_4; break;
                    // AdaptationLayerFrame: glyphCount (which is an integer), glyphRenderTime, glyphStoreTime
                    case QQuickProfiler::SceneGraphAdaptationLayerFrame: ds << data.subtime_3 << data.subtime_1 << data.subtime_2; break;
                    // ContextFrame: compiling material time
                    case QQuickProfiler::SceneGraphContextFrame: ds << data.subtime_1; break;
                    // RenderLoop: syncTime, renderTime, swapTime
                    case QQuickProfiler::SceneGraphRenderLoopFrame: ds << data.subtime_1 << data.subtime_2 << data.subtime_3; break;
                    // TexturePrepare: bind, convert, swizzle, upload, mipmap
                    case QQuickProfiler::SceneGraphTexturePrepare: ds << data.subtime_1 << data.subtime_2 << data.subtime_3 << data.subtime_4 << data.subtime_5; break;
                    // TextureDeletion: deletionTime
                    case QQuickProfiler::SceneGraphTextureDeletion: ds << data.subtime_1; break;
                    // PolishAndSync: polishTime, waitTime, syncTime, animationsTime,
                    case QQuickProfiler::SceneGraphPolishAndSync: ds << data.subtime_1 << data.subtime_2 << data.subtime_3 << data.subtime_4; break;
                    // WindowsRenderLoop: GL time, make current time, SceneGraph time
                    case QQuickProfiler::SceneGraphWindowsRenderShow: ds << data.subtime_1 << data.subtime_2 << data.subtime_3; break;
                    // WindowsAnimations: update time
                    case QQuickProfiler::SceneGraphWindowsAnimations: ds << data.subtime_1; break;
                    // non-threaded rendering: polish time
                    case QQuickProfiler::SceneGraphPolishFrame: ds << data.subtime_1; break;
                    default:break;
                }
                break;
            default:
                Q_ASSERT_X(false, Q_FUNC_INFO, "Invalid message type.");
                break;
            }
            messages.append(ds.squeezedData());
            ds.clear();
        }
    }
}

qint64 QQuickProfilerAdapter::sendMessages(qint64 until, QList<QByteArray> &messages,
                                           bool trackLocations)
{
    Q_UNUSED(trackLocations);
    while (next < m_data.size()) {
        if (m_data[next].time <= until && messages.length() <= s_numMessagesPerBatch)
            qQuickProfilerDataToByteArrays(m_data[next++], messages);
        else
            return m_data[next].time;
    }
    m_data.clear();
    next = 0;
    return -1;
}

void QQuickProfilerAdapter::receiveData(const QVector<QQuickProfilerData> &new_data)
{
    if (m_data.isEmpty())
        m_data = new_data;
    else
        m_data.append(new_data);
    service->dataReady(this);
}

QT_END_NAMESPACE
