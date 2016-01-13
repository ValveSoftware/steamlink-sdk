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

#include "qqmlprofilerservice_p.h"
#include "qqmldebugserver_p.h"
#include "qv4profileradapter_p.h"
#include "qqmlprofiler_p.h"
#include <private/qqmlengine_p.h>

#include <QtCore/qdatastream.h>
#include <QtCore/qurl.h>
#include <QtCore/qtimer.h>
#include <QtCore/qthread.h>
#include <QtCore/qcoreapplication.h>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QQmlProfilerService, profilerInstance)

QQmlProfilerService::QQmlProfilerService()
    : QQmlConfigurableDebugService(QStringLiteral("CanvasFrameRate"), 1)
{
    m_timer.start();

    QMutexLocker lock(configMutex());
    // If there is no debug server it doesn't matter as we'll never get enabled anyway.
    if (QQmlDebugServer::instance() != 0)
        moveToThread(QQmlDebugServer::instance()->thread());
}

QQmlProfilerService::~QQmlProfilerService()
{
    // No need to lock here. If any engine or global profiler is still trying to register at this
    // point we have a nasty bug anyway.
    qDeleteAll(m_engineProfilers.values());
    qDeleteAll(m_globalProfilers);
}

void QQmlProfilerService::dataReady(QQmlAbstractProfilerAdapter *profiler)
{
    QMutexLocker lock(configMutex());
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
        QList<QQmlEngine *> enginesToRelease;
        foreach (QQmlEngine *engine, m_stoppingEngines) {
            foreach (QQmlAbstractProfilerAdapter *engineProfiler, m_engineProfilers.values(engine)) {
                if (m_startTimes.values().contains(engineProfiler)) {
                    enginesToRelease.append(engine);
                    break;
                }
            }
        }
        sendMessages();
        foreach (QQmlEngine *engine, enginesToRelease) {
            m_stoppingEngines.removeOne(engine);
            emit detachedFromEngine(engine);
        }
    }
}

QQmlProfilerService *QQmlProfilerService::instance()
{
    // just make sure that the service is properly registered
    return profilerInstance();
}

void QQmlProfilerService::engineAboutToBeAdded(QQmlEngine *engine)
{
    Q_ASSERT_X(QThread::currentThread() != thread(), Q_FUNC_INFO, "QML profilers have to be added from the engine thread");

    QMutexLocker lock(configMutex());
    QQmlProfilerAdapter *qmlAdapter = new QQmlProfilerAdapter(this, QQmlEnginePrivate::get(engine));
    QV4ProfilerAdapter *v4Adapter = new QV4ProfilerAdapter(this, QV8Engine::getV4(engine->handle()));
    addEngineProfiler(qmlAdapter, engine);
    addEngineProfiler(v4Adapter, engine);
    QQmlConfigurableDebugService::engineAboutToBeAdded(engine);
}

void QQmlProfilerService::engineAdded(QQmlEngine *engine)
{
    Q_ASSERT_X(QThread::currentThread() != thread(), Q_FUNC_INFO, "QML profilers have to be added from the engine thread");

    QMutexLocker lock(configMutex());
    foreach (QQmlAbstractProfilerAdapter *profiler, m_engineProfilers.values(engine))
        profiler->stopWaiting();
}

void QQmlProfilerService::engineAboutToBeRemoved(QQmlEngine *engine)
{
    Q_ASSERT_X(QThread::currentThread() != thread(), Q_FUNC_INFO, "QML profilers have to be removed from the engine thread");

    QMutexLocker lock(configMutex());
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

void QQmlProfilerService::engineRemoved(QQmlEngine *engine)
{
    Q_ASSERT_X(QThread::currentThread() != thread(), Q_FUNC_INFO, "QML profilers have to be removed from the engine thread");

    QMutexLocker lock(configMutex());
    foreach (QQmlAbstractProfilerAdapter *profiler, m_engineProfilers.values(engine)) {
        removeProfilerFromStartTimes(profiler);
        delete profiler;
    }
    m_engineProfilers.remove(engine);
}

void QQmlProfilerService::addEngineProfiler(QQmlAbstractProfilerAdapter *profiler, QQmlEngine *engine)
{
    profiler->moveToThread(thread());
    profiler->synchronize(m_timer);
    m_engineProfilers.insert(engine, profiler);
}

void QQmlProfilerService::addGlobalProfiler(QQmlAbstractProfilerAdapter *profiler)
{
    QMutexLocker lock(configMutex());
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

void QQmlProfilerService::removeGlobalProfiler(QQmlAbstractProfilerAdapter *profiler)
{
    QMutexLocker lock(configMutex());
    removeProfilerFromStartTimes(profiler);
    m_globalProfilers.removeOne(profiler);
    delete profiler;
}

void QQmlProfilerService::removeProfilerFromStartTimes(const QQmlAbstractProfilerAdapter *profiler)
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
void QQmlProfilerService::startProfiling(QQmlEngine *engine, quint64 features)
{
    QMutexLocker lock(configMutex());

    QByteArray message;
    QQmlDebugStream d(&message, QIODevice::WriteOnly);

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
        QSet<QQmlEngine *> engines;
        for (QMultiHash<QQmlEngine *, QQmlAbstractProfilerAdapter *>::iterator i(m_engineProfilers.begin());
                i != m_engineProfilers.end(); ++i) {
            if (!i.value()->isRunning()) {
                engines << i.key();
                i.value()->startProfiling(features);
                startedAny = true;
            }
        }
        foreach (QQmlEngine *profiledEngine, engines)
            d << idForObject(profiledEngine);
    }

    if (startedAny) {
        foreach (QQmlAbstractProfilerAdapter *profiler, m_globalProfilers) {
            if (!profiler->isRunning())
                profiler->startProfiling(features);
        }
    }

    QQmlDebugService::sendMessage(message);
}

/*!
 * Stop profiling the given \a engine. If \a engine is 0, stop all currently running engine
 * profilers.
 *
 * If afterwards no more engine profilers are running, also stop all global profilers. Otherwise
 * only make them report their data.
 */
void QQmlProfilerService::stopProfiling(QQmlEngine *engine)
{
    QMutexLocker lock(configMutex());
    QList<QQmlAbstractProfilerAdapter *> stopping;
    QList<QQmlAbstractProfilerAdapter *> reporting;

    bool stillRunning = false;
    for (QMultiHash<QQmlEngine *, QQmlAbstractProfilerAdapter *>::iterator i(m_engineProfilers.begin());
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

    foreach (QQmlAbstractProfilerAdapter *profiler, reporting)
        profiler->reportData();

    foreach (QQmlAbstractProfilerAdapter *profiler, stopping)
        profiler->stopProfiling();
}

/*
    Send the queued up messages.
*/
void QQmlProfilerService::sendMessages()
{
    QList<QByteArray> messages;

    QByteArray data;
    QQmlDebugStream traceEnd(&data, QIODevice::WriteOnly);
    traceEnd << m_timer.nsecsElapsed() << (int)Event << (int)EndTrace;

    QSet<QQmlEngine *> seen;
    foreach (QQmlAbstractProfilerAdapter *profiler, m_startTimes) {
        for (QMultiHash<QQmlEngine *, QQmlAbstractProfilerAdapter *>::iterator i(m_engineProfilers.begin());
                i != m_engineProfilers.end(); ++i) {
            if (i.value() == profiler && !seen.contains(i.key())) {
                seen << i.key();
                traceEnd << idForObject(i.key());
            }
        }
    }

    while (!m_startTimes.empty()) {
        QQmlAbstractProfilerAdapter *first = m_startTimes.begin().value();
        m_startTimes.erase(m_startTimes.begin());
        if (!m_startTimes.empty()) {
            qint64 next = first->sendMessages(m_startTimes.begin().key(), messages);
            if (next != -1)
                m_startTimes.insert(next, first);
        } else {
            first->sendMessages(std::numeric_limits<qint64>::max(), messages);
        }
    }

    //indicate completion
    messages << data;
    data.clear();

    QQmlDebugStream ds(&data, QIODevice::WriteOnly);
    ds << (qint64)-1 << (int)Complete;
    messages << data;

    QQmlDebugService::sendMessages(messages);
}

void QQmlProfilerService::stateAboutToBeChanged(QQmlDebugService::State newState)
{
    QMutexLocker lock(configMutex());

    if (state() == newState)
        return;

    // Stop all profiling and send the data before we get disabled.
    if (newState != Enabled) {
        foreach (QQmlEngine *engine, m_engineProfilers.keys())
            stopProfiling(engine);
    }
}

void QQmlProfilerService::messageReceived(const QByteArray &message)
{
    QMutexLocker lock(configMutex());

    QByteArray rwData = message;
    QQmlDebugStream stream(&rwData, QIODevice::ReadOnly);

    int engineId = -1;
    quint64 features = std::numeric_limits<quint64>::max();
    bool enabled;
    stream >> enabled;
    if (!stream.atEnd())
        stream >> engineId;
    if (!stream.atEnd())
        stream >> features;

    // If engineId == -1 objectForId() and then the cast will return 0.
    if (enabled)
        startProfiling(qobject_cast<QQmlEngine *>(objectForId(engineId)), features);
    else
        stopProfiling(qobject_cast<QQmlEngine *>(objectForId(engineId)));

    stopWaiting();
}

QT_END_NAMESPACE
