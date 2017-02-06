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

#include "qqmlprofilerservice.h"
#include "qv4profileradapter.h"
#include "qqmlprofileradapter.h"
#include "qqmlprofilerservicefactory.h"
#include "qqmldebugpacket.h"

#include <private/qjsengine_p.h>
#include <private/qqmldebugpluginmanager_p.h>

#include <QtCore/qurl.h>
#include <QtCore/qtimer.h>
#include <QtCore/qthread.h>
#include <QtCore/qcoreapplication.h>

QT_BEGIN_NAMESPACE

Q_QML_DEBUG_PLUGIN_LOADER(QQmlAbstractProfilerAdapter)

QQmlProfilerServiceImpl::QQmlProfilerServiceImpl(QObject *parent) :
    QQmlConfigurableDebugService<QQmlProfilerService>(1, parent),
    m_waitingForStop(false), m_useMessageTypes(false)
{
    m_timer.start();
    QQmlAbstractProfilerAdapter *quickAdapter =
            loadQQmlAbstractProfilerAdapter(QLatin1String("QQuickProfilerAdapter"));
    if (quickAdapter) {
        addGlobalProfiler(quickAdapter);
        quickAdapter->setService(this);
    }
}

QQmlProfilerServiceImpl::~QQmlProfilerServiceImpl()
{
    // No need to lock here. If any engine or global profiler is still trying to register at this
    // point we have a nasty bug anyway.
    qDeleteAll(m_engineProfilers);
    qDeleteAll(m_globalProfilers);
}

void QQmlProfilerServiceImpl::dataReady(QQmlAbstractProfilerAdapter *profiler)
{
    QMutexLocker lock(&m_configMutex);
    bool dataComplete = true;
    for (QMultiMap<qint64, QQmlAbstractProfilerAdapter *>::iterator i(m_startTimes.begin()); i != m_startTimes.end();) {
        if (i.value() == profiler) {
            m_startTimes.erase(i++);
        } else {
            if (i.key() == -1)
                dataComplete = false;
            ++i;
        }
    }
    m_startTimes.insert(0, profiler);
    if (dataComplete) {
        QList<QJSEngine *> enginesToRelease;
        foreach (QJSEngine *engine, m_stoppingEngines) {
            foreach (QQmlAbstractProfilerAdapter *engineProfiler, m_engineProfilers.values(engine)) {
                if (m_startTimes.values().contains(engineProfiler)) {
                    enginesToRelease.append(engine);
                    break;
                }
            }
        }
        sendMessages();
        foreach (QJSEngine *engine, enginesToRelease) {
            m_stoppingEngines.removeOne(engine);
            emit detachedFromEngine(engine);
        }
    }
}

void QQmlProfilerServiceImpl::engineAboutToBeAdded(QJSEngine *engine)
{
    Q_ASSERT_X(QThread::currentThread() == engine->thread(), Q_FUNC_INFO,
               "QML profilers have to be added from the engine thread");

    QMutexLocker lock(&m_configMutex);
    if (QQmlEngine *qmlEngine = qobject_cast<QQmlEngine *>(engine)) {
        QQmlProfilerAdapter *qmlAdapter =
                new QQmlProfilerAdapter(this, QQmlEnginePrivate::get(qmlEngine));
        addEngineProfiler(qmlAdapter, engine);
    }
    QV4ProfilerAdapter *v4Adapter = new QV4ProfilerAdapter(this, QV8Engine::getV4(engine->handle()));
    addEngineProfiler(v4Adapter, engine);
    QQmlConfigurableDebugService<QQmlProfilerService>::engineAboutToBeAdded(engine);
}

void QQmlProfilerServiceImpl::engineAdded(QJSEngine *engine)
{
    Q_ASSERT_X(QThread::currentThread() == engine->thread(), Q_FUNC_INFO,
               "QML profilers have to be added from the engine thread");

    QMutexLocker lock(&m_configMutex);
    foreach (QQmlAbstractProfilerAdapter *profiler, m_engineProfilers.values(engine))
        profiler->stopWaiting();
}

void QQmlProfilerServiceImpl::engineAboutToBeRemoved(QJSEngine *engine)
{
    Q_ASSERT_X(QThread::currentThread() == engine->thread(), Q_FUNC_INFO,
               "QML profilers have to be removed from the engine thread");

    QMutexLocker lock(&m_configMutex);
    bool isRunning = false;
    foreach (QQmlAbstractProfilerAdapter *profiler, m_engineProfilers.values(engine)) {
        if (profiler->isRunning())
            isRunning = true;
        profiler->startWaiting();
    }
    if (isRunning) {
        m_stoppingEngines.append(engine);
        stopProfiling(engine);
    } else {
        emit detachedFromEngine(engine);
    }
}

void QQmlProfilerServiceImpl::engineRemoved(QJSEngine *engine)
{
    Q_ASSERT_X(QThread::currentThread() == engine->thread(), Q_FUNC_INFO,
               "QML profilers have to be removed from the engine thread");

    QMutexLocker lock(&m_configMutex);
    foreach (QQmlAbstractProfilerAdapter *profiler, m_engineProfilers.values(engine)) {
        removeProfilerFromStartTimes(profiler);
        delete profiler;
    }
    m_engineProfilers.remove(engine);
}

void QQmlProfilerServiceImpl::addEngineProfiler(QQmlAbstractProfilerAdapter *profiler, QJSEngine *engine)
{
    profiler->moveToThread(thread());
    profiler->synchronize(m_timer);
    m_engineProfilers.insert(engine, profiler);
}

void QQmlProfilerServiceImpl::addGlobalProfiler(QQmlAbstractProfilerAdapter *profiler)
{
    QMutexLocker lock(&m_configMutex);
    profiler->synchronize(m_timer);
    m_globalProfilers.append(profiler);
    // Global profiler, not connected to a specific engine.
    // Global profilers are started whenever any engine profiler is started and stopped when
    // all engine profilers are stopped.
    quint64 features = 0;
    foreach (QQmlAbstractProfilerAdapter *engineProfiler, m_engineProfilers)
        features |= engineProfiler->features();

    if (features != 0)
        profiler->startProfiling(features);
}

void QQmlProfilerServiceImpl::removeGlobalProfiler(QQmlAbstractProfilerAdapter *profiler)
{
    QMutexLocker lock(&m_configMutex);
    removeProfilerFromStartTimes(profiler);
    m_globalProfilers.removeOne(profiler);
}

void QQmlProfilerServiceImpl::removeProfilerFromStartTimes(const QQmlAbstractProfilerAdapter *profiler)
{
    for (QMultiMap<qint64, QQmlAbstractProfilerAdapter *>::iterator i(m_startTimes.begin());
            i != m_startTimes.end();) {
        if (i.value() == profiler) {
            m_startTimes.erase(i++);
            break;
        } else {
            ++i;
        }
    }
}

/*!
 * Start profiling the given \a engine. If \a engine is 0, start all engine profilers that aren't
 * currently running.
 *
 * If any engine profiler is started like that also start all global profilers.
 */
void QQmlProfilerServiceImpl::startProfiling(QJSEngine *engine, quint64 features)
{
    QMutexLocker lock(&m_configMutex);

    if (features & static_cast<quint64>(1) << ProfileDebugMessages) {
        if (QDebugMessageService *messageService =
                QQmlDebugConnector::instance()->service<QDebugMessageService>())
            messageService->synchronizeTime(m_timer);
    }

    QQmlDebugPacket d;

    d << m_timer.nsecsElapsed() << (int)Event << (int)StartTrace;
    bool startedAny = false;
    if (engine != 0) {
        foreach (QQmlAbstractProfilerAdapter *profiler, m_engineProfilers.values(engine)) {
            if (!profiler->isRunning()) {
                profiler->startProfiling(features);
                startedAny = true;
            }
        }
        if (startedAny)
            d << idForObject(engine);
    } else {
        QSet<QJSEngine *> engines;
        for (QMultiHash<QJSEngine *, QQmlAbstractProfilerAdapter *>::iterator i(m_engineProfilers.begin());
                i != m_engineProfilers.end(); ++i) {
            if (!i.value()->isRunning()) {
                engines << i.key();
                i.value()->startProfiling(features);
                startedAny = true;
            }
        }
        foreach (QJSEngine *profiledEngine, engines)
            d << idForObject(profiledEngine);
    }

    if (startedAny) {
        foreach (QQmlAbstractProfilerAdapter *profiler, m_globalProfilers) {
            if (!profiler->isRunning())
                profiler->startProfiling(features);
        }

        emit startFlushTimer();
    }

    emit messageToClient(name(), d.data());
}

/*!
 * Stop profiling the given \a engine. If \a engine is 0, stop all currently running engine
 * profilers.
 *
 * If afterwards no more engine profilers are running, also stop all global profilers. Otherwise
 * only make them report their data.
 */
void QQmlProfilerServiceImpl::stopProfiling(QJSEngine *engine)
{
    QMutexLocker lock(&m_configMutex);
    QList<QQmlAbstractProfilerAdapter *> stopping;
    QList<QQmlAbstractProfilerAdapter *> reporting;

    bool stillRunning = false;
    for (QMultiHash<QJSEngine *, QQmlAbstractProfilerAdapter *>::iterator i(m_engineProfilers.begin());
            i != m_engineProfilers.end(); ++i) {
        if (i.value()->isRunning()) {
            if (engine == 0 || i.key() == engine) {
                m_startTimes.insert(-1, i.value());
                stopping << i.value();
            } else {
                stillRunning = true;
            }
        }
    }

    if (stopping.isEmpty())
        return;

    foreach (QQmlAbstractProfilerAdapter *profiler, m_globalProfilers) {
        if (!profiler->isRunning())
            continue;
        m_startTimes.insert(-1, profiler);
        if (stillRunning) {
            reporting << profiler;
        } else {
            stopping << profiler;
        }
    }

    emit stopFlushTimer();
    m_waitingForStop = true;

    foreach (QQmlAbstractProfilerAdapter *profiler, reporting)
        profiler->reportData(m_useMessageTypes);

    foreach (QQmlAbstractProfilerAdapter *profiler, stopping)
        profiler->stopProfiling();
}

/*
    Send the queued up messages.
*/
void QQmlProfilerServiceImpl::sendMessages()
{
    QList<QByteArray> messages;

    QQmlDebugPacket traceEnd;
    if (m_waitingForStop) {
        traceEnd << m_timer.nsecsElapsed() << (int)Event << (int)EndTrace;

        QSet<QJSEngine *> seen;
        foreach (QQmlAbstractProfilerAdapter *profiler, m_startTimes) {
            for (QMultiHash<QJSEngine *, QQmlAbstractProfilerAdapter *>::iterator i(m_engineProfilers.begin());
                    i != m_engineProfilers.end(); ++i) {
                if (i.value() == profiler && !seen.contains(i.key())) {
                    seen << i.key();
                    traceEnd << idForObject(i.key());
                }
            }
        }
    }

    while (!m_startTimes.empty()) {
        QQmlAbstractProfilerAdapter *first = m_startTimes.begin().value();
        m_startTimes.erase(m_startTimes.begin());
        qint64 next = first->sendMessages(m_startTimes.isEmpty() ?
                                              std::numeric_limits<qint64>::max() :
                                              m_startTimes.begin().key(), messages,
                                          m_useMessageTypes);
        if (next != -1)
            m_startTimes.insert(next, first);

        if (messages.length() >= QQmlAbstractProfilerAdapter::s_numMessagesPerBatch) {
            emit messagesToClient(name(), messages);
            messages.clear();
        }
    }

    if (m_waitingForStop) {
        //indicate completion
        messages << traceEnd.data();

        QQmlDebugPacket ds;
        ds << (qint64)-1 << (int)Complete;
        messages << ds.data();
        m_waitingForStop = false;
    }

    emit messagesToClient(name(), messages);

    // Restart flushing if any profilers are still running
    foreach (const QQmlAbstractProfilerAdapter *profiler, m_engineProfilers) {
        if (profiler->isRunning()) {
            emit startFlushTimer();
            break;
        }
    }
}

void QQmlProfilerServiceImpl::stateAboutToBeChanged(QQmlDebugService::State newState)
{
    QMutexLocker lock(&m_configMutex);

    if (state() == newState)
        return;

    // Stop all profiling and send the data before we get disabled.
    if (newState != Enabled) {
        for (auto it = m_engineProfilers.keyBegin(), end = m_engineProfilers.keyEnd();
             it != end; ++it) {
            stopProfiling(*it);
        }
    }
}

void QQmlProfilerServiceImpl::messageReceived(const QByteArray &message)
{
    QMutexLocker lock(&m_configMutex);

    QQmlDebugPacket stream(message);

    int engineId = -1;
    quint64 features = std::numeric_limits<quint64>::max();
    bool enabled;
    uint flushInterval = 0;
    stream >> enabled;
    if (!stream.atEnd())
        stream >> engineId;
    if (!stream.atEnd())
        stream >> features;
    if (!stream.atEnd()) {
        stream >> flushInterval;
        m_flushTimer.setInterval(flushInterval);
        auto timerStart = static_cast<void(QTimer::*)()>(&QTimer::start);
        if (flushInterval > 0) {
            connect(&m_flushTimer, &QTimer::timeout, this, &QQmlProfilerServiceImpl::flush);
            connect(this, &QQmlProfilerServiceImpl::startFlushTimer, &m_flushTimer, timerStart);
            connect(this, &QQmlProfilerServiceImpl::stopFlushTimer, &m_flushTimer, &QTimer::stop);
        } else {
            disconnect(&m_flushTimer, &QTimer::timeout, this, &QQmlProfilerServiceImpl::flush);
            disconnect(this, &QQmlProfilerServiceImpl::startFlushTimer, &m_flushTimer, timerStart);
            disconnect(this, &QQmlProfilerServiceImpl::stopFlushTimer,
                       &m_flushTimer, &QTimer::stop);
        }
    }
    if (!stream.atEnd())
        stream >> m_useMessageTypes;

    // If engineId == -1 objectForId() and then the cast will return 0.
    if (enabled)
        startProfiling(qobject_cast<QJSEngine *>(objectForId(engineId)), features);
    else
        stopProfiling(qobject_cast<QJSEngine *>(objectForId(engineId)));

    stopWaiting();
}

void QQmlProfilerServiceImpl::flush()
{
    QMutexLocker lock(&m_configMutex);
    QList<QQmlAbstractProfilerAdapter *> reporting;

    foreach (QQmlAbstractProfilerAdapter *profiler, m_engineProfilers) {
        if (profiler->isRunning()) {
            m_startTimes.insert(-1, profiler);
            reporting.append(profiler);
        }
    }

    foreach (QQmlAbstractProfilerAdapter *profiler, m_globalProfilers) {
        if (profiler->isRunning()) {
            m_startTimes.insert(-1, profiler);
            reporting.append(profiler);
        }
    }

    foreach (QQmlAbstractProfilerAdapter *profiler, reporting)
        profiler->reportData(m_useMessageTypes);
}

QT_END_NAMESPACE
