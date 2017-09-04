/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#include "qremoteobjectpendingcall.h"
#include "qremoteobjectpendingcall_p.h"

#include "qremoteobjectreplica_p.h"

#include <QCoreApplication>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

QRemoteObjectPendingCallData::QRemoteObjectPendingCallData(int serialId, QRemoteObjectReplicaPrivate *replica)
    : replica(replica)
    , serialId(serialId)
    , error(QRemoteObjectPendingCall::InvalidMessage)
    , watcherHelper(nullptr)
{
}

QRemoteObjectPendingCallData::~QRemoteObjectPendingCallData()
{
}

void QRemoteObjectPendingCallWatcherHelper::add(QRemoteObjectPendingCallWatcher *watcher)
{
    connect(this, SIGNAL(finished()), watcher, SLOT(_q_finished()), Qt::QueuedConnection);
}

void QRemoteObjectPendingCallWatcherHelper::emitSignals()
{
    emit finished();
}

QRemoteObjectPendingCall::QRemoteObjectPendingCall()
    : d(new QRemoteObjectPendingCallData)
{
}

QRemoteObjectPendingCall::~QRemoteObjectPendingCall()
{
}

QRemoteObjectPendingCall::QRemoteObjectPendingCall(const QRemoteObjectPendingCall& other)
    : d(other.d)
{
}

QRemoteObjectPendingCall::QRemoteObjectPendingCall(QRemoteObjectPendingCallData *dd)
    : d(dd)
{
}

QRemoteObjectPendingCall &QRemoteObjectPendingCall::operator=(const QRemoteObjectPendingCall &other)
{
    d = other.d;
    return *this;
}

QVariant QRemoteObjectPendingCall::returnValue() const
{
    if (!d)
        return QVariant();

    QMutexLocker locker(&d->mutex);
    return d->returnValue;
}

QRemoteObjectPendingCall::Error QRemoteObjectPendingCall::error() const
{
    if (!d)
        return QRemoteObjectPendingCall::InvalidMessage;

    QMutexLocker locker(&d->mutex);
    return d->error;
}

bool QRemoteObjectPendingCall::isFinished() const
{
    if (!d)
        return true; // considered finished

    QMutexLocker locker(&d->mutex);
    return d->error != InvalidMessage;
}

bool QRemoteObjectPendingCall::waitForFinished(int timeout)
{
    if (!d)
        return false;

    if (d->error != QRemoteObjectPendingCall::InvalidMessage)
        return true; // already finished

    QMutexLocker locker(&d->mutex);
    if (!d->replica)
        return false;

    return d->replica->waitForFinished(*this, timeout);
}

QRemoteObjectPendingCall QRemoteObjectPendingCall::fromCompletedCall(const QVariant &returnValue)
{
    QRemoteObjectPendingCallData *data = new QRemoteObjectPendingCallData;
    data->returnValue = returnValue;
    data->error = NoError;
    return QRemoteObjectPendingCall(data);
}

class QRemoteObjectPendingCallWatcherPrivate: public QObjectPrivate
{
public:
    void _q_finished();

    Q_DECLARE_PUBLIC(QRemoteObjectPendingCallWatcher)
};

inline void QRemoteObjectPendingCallWatcherPrivate::_q_finished()
{
    Q_Q(QRemoteObjectPendingCallWatcher);
    emit q->finished(q);
}

QRemoteObjectPendingCallWatcher::QRemoteObjectPendingCallWatcher(const QRemoteObjectPendingCall &call, QObject *parent)
    : QObject(*new QRemoteObjectPendingCallWatcherPrivate, parent)
    , QRemoteObjectPendingCall(call)
{
    if (d) {
        QMutexLocker locker(&d->mutex);
        if (!d->watcherHelper) {
            d->watcherHelper.reset(new QRemoteObjectPendingCallWatcherHelper);
            if (d->error != QRemoteObjectPendingCall::InvalidMessage) {
                // cause a signal emission anyways
                QMetaObject::invokeMethod(d->watcherHelper.data(), "finished", Qt::QueuedConnection);
            }
        }
        d->watcherHelper->add(this);
    }
}

QRemoteObjectPendingCallWatcher::~QRemoteObjectPendingCallWatcher()
{
}

bool QRemoteObjectPendingCallWatcher::isFinished() const
{
    if (!d)
        return true; // considered finished

    QMutexLocker locker(&d->mutex);
    return d->error != QRemoteObjectPendingCall::InvalidMessage;
}

void QRemoteObjectPendingCallWatcher::waitForFinished()
{
    if (d) {
        QRemoteObjectPendingCall::waitForFinished();

        // our signals were queued, so deliver them
        QCoreApplication::sendPostedEvents(d->watcherHelper.data(), QEvent::MetaCall);
        QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
    }
}

QT_END_NAMESPACE

#include "moc_qremoteobjectpendingcall.cpp"
