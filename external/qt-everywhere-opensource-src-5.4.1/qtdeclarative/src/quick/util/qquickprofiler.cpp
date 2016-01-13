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

#include "qquickprofiler_p.h"
#include <QCoreApplication>
#include <private/qqmldebugservice_p.h>
#include <private/qqmlprofilerservice_p.h>

QT_BEGIN_NAMESPACE

// instance will be set, unset in constructor. Allows static methods to be inlined.
QQuickProfiler *QQuickProfiler::s_instance = 0;
quint64 QQuickProfiler::featuresEnabled = 0;

// convert to QByteArrays that can be sent to the debug client
// use of QDataStream can skew results
//     (see tst_qqmldebugtrace::trace() benchmark)
void QQuickProfilerData::toByteArrays(QList<QByteArray> &messages) const
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
            case QQuickProfiler::Event:
                if (decodedDetailType == (int)QQuickProfiler::AnimationFrame)
                    ds << framerate << count << threadId;
                break;
            case QQuickProfiler::PixmapCacheEvent:
                ds << detailUrl.toString();
                switch (decodedDetailType) {
                    case QQuickProfiler::PixmapSizeKnown: ds << x << y; break;
                    case QQuickProfiler::PixmapReferenceCountChanged: ds << count; break;
                    case QQuickProfiler::PixmapCacheCountChanged: ds << count; break;
                    default: break;
                }
                break;
            case QQuickProfiler::SceneGraphFrame:
                switch (decodedDetailType) {
                    // RendererFrame: preprocessTime, updateTime, bindingTime, renderTime
                    case QQuickProfiler::SceneGraphRendererFrame: ds << subtime_1 << subtime_2 << subtime_3 << subtime_4; break;
                    // AdaptationLayerFrame: glyphCount (which is an integer), glyphRenderTime, glyphStoreTime
                    case QQuickProfiler::SceneGraphAdaptationLayerFrame: ds << subtime_1 << subtime_2 << subtime_3; break;
                    // ContextFrame: compiling material time
                    case QQuickProfiler::SceneGraphContextFrame: ds << subtime_1; break;
                    // RenderLoop: syncTime, renderTime, swapTime
                    case QQuickProfiler::SceneGraphRenderLoopFrame: ds << subtime_1 << subtime_2 << subtime_3; break;
                    // TexturePrepare: bind, convert, swizzle, upload, mipmap
                    case QQuickProfiler::SceneGraphTexturePrepare: ds << subtime_1 << subtime_2 << subtime_3 << subtime_4 << subtime_5; break;
                    // TextureDeletion: deletionTime
                    case QQuickProfiler::SceneGraphTextureDeletion: ds << subtime_1; break;
                    // PolishAndSync: polishTime, waitTime, syncTime, animationsTime,
                    case QQuickProfiler::SceneGraphPolishAndSync: ds << subtime_1 << subtime_2 << subtime_3 << subtime_4; break;
                    // WindowsRenderLoop: GL time, make current time, SceneGraph time
                    case QQuickProfiler::SceneGraphWindowsRenderShow: ds << subtime_1 << subtime_2 << subtime_3; break;
                    // WindowsAnimations: update time
                    case QQuickProfiler::SceneGraphWindowsAnimations: ds << subtime_1; break;
                    // non-threaded rendering: polish time
                    case QQuickProfiler::SceneGraphPolishFrame: ds << subtime_1; break;
                    default:break;
                }
                break;
            default:
                Q_ASSERT_X(false, Q_FUNC_INFO, "Invalid message type.");
                break;
            }
            messages << data;
            data.clear();
        }
    }
}

qint64 QQuickProfiler::sendMessages(qint64 until, QList<QByteArray> &messages)
{
    QMutexLocker lock(&m_dataMutex);
    while (next < m_data.size() && m_data[next].time <= until) {
        m_data[next++].toByteArrays(messages);
    }
    return next < m_data.size() ? m_data[next].time : -1;
}

void QQuickProfiler::initialize()
{
    Q_ASSERT(s_instance == 0);
    QQmlProfilerService *service = QQmlProfilerService::instance();
    s_instance = new QQuickProfiler(service);
    QQmlProfilerService::instance()->addGlobalProfiler(s_instance);
}

void animationTimerCallback(qint64 delta)
{
    Q_QUICK_PROFILE(QQuickProfiler::ProfileAnimations, animationFrame(delta,
            QThread::currentThread() == QCoreApplication::instance()->thread() ?
            QQuickProfiler::GuiThread : QQuickProfiler::RenderThread));
}

void QQuickProfiler::registerAnimationCallback()
{
    QUnifiedTimer::instance()->registerProfilerCallback(&animationTimerCallback);
}

class CallbackRegistrationHelper : public QObject {
    Q_OBJECT
public slots:
    void registerAnimationTimerCallback()
    {
        QQuickProfiler::registerAnimationCallback();
        delete this;
    }
};

#include "qquickprofiler.moc"

QQuickProfiler::QQuickProfiler(QQmlProfilerService *service) :
    QQmlAbstractProfilerAdapter(service), next(0)
{
    // This is safe because at this point the m_instance isn't initialized, yet.
    m_timer.start();

    // We can always do DirectConnection here as all methods are protected by mutexes
    connect(this, SIGNAL(profilingEnabled(quint64)), this, SLOT(startProfilingImpl(quint64)),
            Qt::DirectConnection);
    connect(this, SIGNAL(profilingEnabledWhileWaiting(quint64)),
            this, SLOT(startProfilingImpl(quint64)), Qt::DirectConnection);
    connect(this, SIGNAL(referenceTimeKnown(QElapsedTimer)), this, SLOT(setTimer(QElapsedTimer)),
            Qt::DirectConnection);
    connect(this, SIGNAL(profilingDisabled()), this, SLOT(stopProfilingImpl()),
            Qt::DirectConnection);
    connect(this, SIGNAL(profilingDisabledWhileWaiting()), this, SLOT(stopProfilingImpl()),
            Qt::DirectConnection);
    connect(this, SIGNAL(dataRequested()), this, SLOT(reportDataImpl()),
            Qt::DirectConnection);

    CallbackRegistrationHelper *helper = new CallbackRegistrationHelper; // will delete itself
    helper->moveToThread(QCoreApplication::instance()->thread());
    QMetaObject::invokeMethod(helper, "registerAnimationTimerCallback", Qt::QueuedConnection);
}

QQuickProfiler::~QQuickProfiler()
{
    QMutexLocker lock(&m_dataMutex);
    featuresEnabled = 0;
    s_instance = 0;
}

void QQuickProfiler::startProfilingImpl(quint64 features)
{
    QMutexLocker lock(&m_dataMutex);
    next = 0;
    m_data.clear();
    featuresEnabled = features;
}

void QQuickProfiler::stopProfilingImpl()
{
    {
        QMutexLocker lock(&m_dataMutex);
        featuresEnabled = 0;
        next = 0;
    }
    service->dataReady(this);
}

void QQuickProfiler::reportDataImpl()
{
    {
        QMutexLocker lock(&m_dataMutex);
        next = 0;
    }
    service->dataReady(this);
}

void QQuickProfiler::setTimer(const QElapsedTimer &t)
{
    QMutexLocker lock(&m_dataMutex);
    m_timer = t;
}

QT_END_NAMESPACE
