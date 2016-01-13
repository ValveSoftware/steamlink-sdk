/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "private/qdeclarativenotifier_p.h"
#include "private/qdeclarativeproperty_p.h"

QT_BEGIN_NAMESPACE

void QDeclarativeNotifier::emitNotify(QDeclarativeNotifierEndpoint *endpoint)
{
    QDeclarativeNotifierEndpoint::Notifier *n = endpoint->asNotifier();

    QDeclarativeNotifierEndpoint **oldDisconnected = n->disconnected;
    n->disconnected = &endpoint;

    if (n->next)
        emitNotify(n->next);

    if (endpoint) {
        void *args[] = { 0 };

        QMetaObject::metacall(endpoint->target, QMetaObject::InvokeMetaMethod,
                              endpoint->targetMethod, args);

        if (endpoint)
            endpoint->asNotifier()->disconnected = oldDisconnected;
    }

    if (oldDisconnected) *oldDisconnected = endpoint;
}

void QDeclarativeNotifierEndpoint::connect(QObject *source, int sourceSignal)
{
    Signal *s = toSignal();

    if (s->source == source && s->sourceSignal == sourceSignal)
        return;

    disconnect();

    QDeclarativePropertyPrivate::connect(source, sourceSignal, target, targetMethod);

    s->source = source;
    s->sourceSignal = sourceSignal;
}

void QDeclarativeNotifierEndpoint::copyAndClear(QDeclarativeNotifierEndpoint &other)
{
    other.disconnect();

    other.target = target;
    other.targetMethod = targetMethod;

    if (!isConnected())
        return;

    if (SignalType == type) {
        Signal *other_s = other.toSignal();
        Signal *s = asSignal();

        other_s->source = s->source;
        other_s->sourceSignal = s->sourceSignal;
        s->source = 0;
    } else if(NotifierType == type) {
        Notifier *other_n = other.toNotifier();
        Notifier *n = asNotifier();

        other_n->notifier = n->notifier;
        other_n->disconnected = n->disconnected;
        if (other_n->disconnected) *other_n->disconnected = &other;

        if (n->next) {
            other_n->next = n->next;
            n->next->asNotifier()->prev = &other_n->next;
        }
        other_n->prev = n->prev;
        *other_n->prev = &other;

        n->prev = 0;
        n->next = 0;
        n->disconnected = 0;
        n->notifier = 0;
    }
}


QT_END_NAMESPACE

