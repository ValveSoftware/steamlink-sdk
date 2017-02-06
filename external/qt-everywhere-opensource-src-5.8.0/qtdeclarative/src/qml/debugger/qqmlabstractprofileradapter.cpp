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

#include "qqmlabstractprofileradapter_p.h"

QT_BEGIN_NAMESPACE

/*!
 * \internal
 * \class QQmlAbstractProfilerAdapter
 * \inmodule QtQml
 * Abstract base class for all adapters between profilers and the QQmlProfilerService. Adapters have
 * to retrieve profiler-specific data and convert it to the format sent over the wire. Adapters must
 * live in the QDebugServer thread but the actual profilers can live in different threads. The
 * recommended way to deal with this is passing the profiling data through a signal/slot connection.
 */

/*!
 * \internal
 * \fn void QQmlAbstractProfilerAdapter::dataRequested()
 * Signals that data has been requested by the \c QQmlProfilerService. This signal should be
 * connected to a slot in the profiler and the profiler should then transfer its currently available
 * profiling data to the adapter as soon as possible.
 */

/*!
 * \internal
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
 * \internal
 * Emits either \c profilingEnabled(quint64) or \c profilingEnabledWhileWaiting(quint64), depending
 * on \c waiting. If the profiler's thread is waiting for an initial start signal, we can emit the
 * signal over a Qt::DirectConnection to avoid the delay of the event loop. The \a features are
 * passed on to the signal.
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
 * \internal
 * Emits either \c profilingDisabled() or \c profilingDisabledWhileWaiting(), depending on
 * \c waiting. If the profiler's thread is waiting for an initial start signal, we can emit the
 * signal over a Qt::DirectConnection to avoid the delay of the event loop. This should trigger
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
 * \internal
 * \fn bool QQmlAbstractProfilerAdapter::isRunning() const
 * Returns if the profiler is currently running. The profiler is considered to be running after
 * \c startProfiling(quint64) has been called until \c stopProfiling() is called. That is
 * independent of \c waiting. The profiler may be running and waiting at the same time.
 */

/*!
 * \internal
 * \fn void QQmlAbstractProfilerAdapter::profilingDisabled()
 * This signal is emitted if \c stopProfiling() is called while the profiler is not considered to
 * be waiting. The profiler is expected to handle the signal asynchronously.
 */

/*!
 * \internal
 * \fn void QQmlAbstractProfilerAdapter::profilingDisabledWhileWaiting()
 * This signal is emitted if \c stopProfiling() is called while the profiler is considered to be
 * waiting. In many cases this signal can be connected with a Qt::DirectConnection.
 */

/*!
 * \internal
 * \fn void QQmlAbstractProfilerAdapter::profilingEnabled(quint64 features)
 * This signal is emitted if \c startProfiling(quint64) is called while the profiler is not
 * considered to be waiting. The profiler is expected to handle the signal asynchronously. The
 * \a features are passed on from \c startProfiling(quint64).
 */

/*!
 * \internal
 * \fn void QQmlAbstractProfilerAdapter::profilingEnabledWhileWaiting(quint64 features)
 * This signal is emitted if \c startProfiling(quint64) is called while the profiler is considered
 * to be waiting. In many cases this signal can be connected with a Qt::DirectConnection. By
 * starting the profiler synchronously when the QML engine starts instead of waiting for the first
 * iteration of the event loop the engine startup can be profiled. The \a features are passed on
 * from \c startProfiling(quint64).
 */

/*!
 * \internal
 * \fn void QQmlAbstractProfilerAdapter::referenceTimeKnown(const QElapsedTimer &timer)
 * This signal is used to synchronize the profiler's timer to the QQmlProfilerservice's. The
 * profiler is expected to save \a timer and use it for timestamps on its data.
 */

/*!
 * \internal
 * \fn void QQmlAbstractProfilerAdapter::synchronize(const QElapsedTimer &timer)
 * Synchronize the profiler to \a timer. This emits \c referenceTimeKnown().
 */

/*!
 * \internal
 * \fn void QQmlAbstractProfilerAdapter::reportData()
 * Make the profiler report its current data without stopping the collection. The same (and
 * additional) data can later be requested again with \c stopProfiling() or \c reportData().
 */

/*!
 * \internal
 * \fn void QQmlAbstractProfilerAdapter::startWaiting()
 * Consider the profiler to be waiting from now on. While the profiler is waiting it can be directly
 * accessed even if it is in a different thread. This method should only be called if it is actually
 * safe to do so.
 */

/*!
 * \internal
 * \fn void QQmlAbstractProfilerAdapter::stopWaiting()
 * Consider the profiler not to be waiting anymore. If it lives in a different threads any requests
 * for it have to be done via a queued connection then.
 */

QT_END_NAMESPACE
