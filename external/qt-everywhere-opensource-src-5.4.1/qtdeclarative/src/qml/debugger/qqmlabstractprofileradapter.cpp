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

#include "qqmlabstractprofileradapter_p.h"

QT_BEGIN_NAMESPACE

/*!
 * \class QQmlAbstractProfilerAdapter
 * \inmodule QtQml
 * Abstract base class for all adapters between profilers and the QQmlProfilerService. Adapters have
 * to retrieve profiler-specific data and convert it to the format sent over the wire. Adapters must
 * live in the QDebugServer thread but the actual profilers can live in different threads. The
 * recommended way to deal with this is passing the profiling data through a signal/slot connection.
 */

/*!
 * \fn void QQmlAbstractProfilerAdapter::dataRequested()
 * Signals that data has been requested by the \c QQmlProfilerService. This signal should be
 * connected to a slot in the profiler and the profiler should then transfer its currently available
 * profiling data to the adapter as soon as possible.
 */

/*!
 * \fn qint64 QQmlAbstractProfilerAdapter::sendMessages(qint64 until, QList<QByteArray> &messages)
 * Append the messages up to the timestamp \a until, chronologically sorted, to \a messages. Keep
 * track of the messages already sent and with each subsequent call to this method start with the
 * first one not yet sent. Messages that have been sent can be deleted. When new data from the
 * profiler arrives the information about the last sent message must be reset. Return the timestamp
 * of the next message after \a until or \c -1 if there is no such message.
 * The profiler service keeps a list of adapters, sorted by time of next message and keeps querying
 * the first one to send messages up to the time of the second one. Like that we get chronologically
 * sorted messages and can occasionally post the messages to exploit parallelism and save memory.
 */

/*!
 * \fn void QQmlAbstractProfilerAdapter::startProfiling()
 * Emits either \c profilingEnabled() or \c profilingEnabledWhileWaiting(), depending on \c waiting.
 * If the profiler's thread is waiting for an initial start signal we can emit the signal over a
 * \c Qt::DirectConnection to avoid the delay of the event loop.
 */
void QQmlAbstractProfilerAdapter::startProfiling(quint64 features)
{
    if (waiting)
        emit profilingEnabledWhileWaiting(features);
    else
        emit profilingEnabled(features);
    featuresEnabled = features;
}

/*!
 * \fn void QQmlAbstractProfilerAdapter::stopProfiling()
 * Emits either \c profilingDisabled() or \c profilingDisabledWhileWaiting(), depending on
 * \c waiting. If the profiler's thread is waiting for an initial start signal we can emit the
 * signal over a \c Qt::DirectConnection to avoid the delay of the event loop. This should trigger
 * the profiler to report its collected data and subsequently delete it.
 */
void QQmlAbstractProfilerAdapter::stopProfiling() {
    if (waiting)
        emit profilingDisabledWhileWaiting();
    else
        emit profilingDisabled();
    featuresEnabled = 0;
}

/*!
 * \fn bool QQmlAbstractProfilerAdapter::isRunning() const
 * Returns if the profiler is currently running. The profiler is considered to be running after
 * \c startProfiling() has been called until \c stopProfiling() is called. That is independent of
 * \c waiting. The profiler may be running and waiting at the same time.
 */

/*!
 * \fn void QQmlAbstractProfilerAdapter::profilingDisabled()
 * This signal is emitted if \c stopProfiling() is called while the profiler is not considered to
 * be waiting. The profiler is expected to handle the signal asynchronously.
 */

/*!
 * \fn void QQmlAbstractProfilerAdapter::profilingDisabledWhileWaiting()
 * This signal is emitted if \c stopProfiling() is called while the profiler is considered to be
 * waiting. In many cases this signal can be connected with a \c Qt::DirectConnection.
 */

/*!
 * \fn void QQmlAbstractProfilerAdapter::profilingEnabled()
 * This signal is emitted if \c startProfiling() is called while the profiler is not considered to
 * be waiting. The profiler is expected to handle the signal asynchronously.
 */

/*!
 * \fn void QQmlAbstractProfilerAdapter::profilingEnabledWhileWaiting()
 * This signal is emitted if \c startProfiling() is called while the profiler is considered to be
 * waiting. In many cases this signal can be connected with a \c Qt::DirectConnection. By starting
 * the profiler synchronously when the QML engine starts instead of waiting for the first iteration
 * of the event loop the engine startup can be profiled.
 */

/*!
 * \fn void QQmlAbstractProfilerAdapter::referenceTimeKnown(const QElapsedTimer &timer)
 * This signal is used to synchronize the profiler's timer to the QQmlProfilerservice's. The
 * profiler is expected to save \a timer and use it for timestamps on its data.
 */

/*!
 * \fn void QQmlAbstractProfilerAdapter::synchronize(const QElapsedTimer &timer)
 * Synchronize the profiler to \a timer. This emits \c referenceTimeKnown().
 */

/*!
 * \fn void QQmlAbstractProfilerAdapter::reportData()
 * Make the profiler report its current data without stopping the collection. The same (and
 * additional) data can later be requested again with \c stopProfiling() or \c reportData().
 */

/*!
 * \fn void QQmlAbstractProfilerAdapter::startWaiting()
 * Consider the profiler to be waiting from now on. While the profiler is waiting it can be directly
 * accessed even if it is in a different thread. This method should only be called if it is actually
 * safe to do so.
 */

/*!
 * \fn void QQmlAbstractProfilerAdapter::stopWaiting()
 * Consider the profiler not to be waiting anymore. If it lives in a different threads any requests
 * for it have to be done via a queued connection then.
 */

QT_END_NAMESPACE
