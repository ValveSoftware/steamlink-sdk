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

#include "qqmlnotifier_p.h"
#include "qqmlproperty_p.h"
#include <QtCore/qdebug.h>
#include <private/qthread_p.h>

QT_BEGIN_NAMESPACE

typedef void (*Callback)(QQmlNotifierEndpoint *, void **);

void QQmlBoundSignal_callback(QQmlNotifierEndpoint *, void **);
void QQmlJavaScriptExpressionGuard_callback(QQmlNotifierEndpoint *, void **);
void QQmlVMEMetaObjectEndpoint_callback(QQmlNotifierEndpoint *, void **);

static Callback QQmlNotifier_callbacks[] = {
    0,
    QQmlBoundSignal_callback,
    QQmlJavaScriptExpressionGuard_callback,
    QQmlVMEMetaObjectEndpoint_callback
};

void QQmlNotifier::emitNotify(QQmlNotifierEndpoint *endpoint, void **a)
{
    qintptr originalSenderPtr;
    qintptr *disconnectWatch;

    if (!endpoint->isNotifying()) {
        originalSenderPtr = endpoint->senderPtr;
        disconnectWatch = &originalSenderPtr;
        endpoint->senderPtr = qintptr(disconnectWatch) | 0x1;
    } else {
        disconnectWatch = (qintptr *)(endpoint->senderPtr & ~0x1);
    }

    if (endpoint->next)
        emitNotify(endpoint->next, a);

    if (*disconnectWatch) {

        Q_ASSERT(QQmlNotifier_callbacks[endpoint->callback]);
        QQmlNotifier_callbacks[endpoint->callback](endpoint, a);

        if (disconnectWatch == &originalSenderPtr && originalSenderPtr) {
            // End of notifying, restore values
            endpoint->senderPtr = originalSenderPtr;
        }
    }
}

/*! \internal
    \a sourceSignal MUST be in the signal index range (see QObjectPrivate::signalIndex()).
    This is different from QMetaMethod::methodIndex().
*/
void QQmlNotifierEndpoint::connect(QObject *source, int sourceSignal, QQmlEngine *engine)
{
    disconnect();

    Q_ASSERT(engine);
    if (QObjectPrivate::get(source)->threadData->threadId !=
        QObjectPrivate::get(engine)->threadData->threadId) {

        QString sourceName;
        QDebug(&sourceName) << source;
        sourceName = sourceName.left(sourceName.length() - 1);
        QString engineName;
        QDebug(&engineName).nospace() << engine;
        engineName = engineName.left(engineName.length() - 1);

        qFatal("QQmlEngine: Illegal attempt to connect to %s that is in"
               " a different thread than the QML engine %s.", qPrintable(sourceName),
               qPrintable(engineName));
    }

    senderPtr = qintptr(source);
    this->sourceSignal = sourceSignal;
    QQmlPropertyPrivate::flushSignal(source, sourceSignal);
    QQmlData *ddata = QQmlData::get(source, true);
    ddata->addNotify(sourceSignal, this);
    QObjectPrivate * const priv = QObjectPrivate::get(source);
    priv->connectNotify(QMetaObjectPrivate::signal(source->metaObject(), sourceSignal));
}

QT_END_NAMESPACE

