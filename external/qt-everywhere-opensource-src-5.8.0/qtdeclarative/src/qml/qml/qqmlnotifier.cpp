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

namespace {
    struct NotifyListTraversalData {
        NotifyListTraversalData(QQmlNotifierEndpoint *ep = 0)
            : originalSenderPtr(0)
            , disconnectWatch(0)
            , endpoint(ep)
        {}

        qintptr originalSenderPtr;
        qintptr *disconnectWatch;
        QQmlNotifierEndpoint *endpoint;
    };
}

void QQmlNotifier::notify(QQmlData *ddata, int notifierIndex)
{
    if (QQmlNotifierEndpoint *ep = ddata->notify(notifierIndex))
        emitNotify(ep, Q_NULLPTR);
}

void QQmlNotifier::emitNotify(QQmlNotifierEndpoint *endpoint, void **a)
{
    QVarLengthArray<NotifyListTraversalData> stack;
    while (endpoint) {
        stack.append(NotifyListTraversalData(endpoint));
        endpoint = endpoint->next;
    }

    int i = 0;
    for (; i < stack.size(); ++i) {
        NotifyListTraversalData &data = stack[i];

        if (!data.endpoint->isNotifying()) {
            data.originalSenderPtr = data.endpoint->senderPtr;
            data.disconnectWatch = &data.originalSenderPtr;
            data.endpoint->senderPtr = qintptr(data.disconnectWatch) | 0x1;
        } else {
            data.disconnectWatch = (qintptr *)(data.endpoint->senderPtr & ~0x1);
        }
    }

    while (--i >= 0) {
        const NotifyListTraversalData &data = stack.at(i);
        if (*data.disconnectWatch) {

            Q_ASSERT(QQmlNotifier_callbacks[data.endpoint->callback]);
            QQmlNotifier_callbacks[data.endpoint->callback](data.endpoint, a);

            if (data.disconnectWatch == &data.originalSenderPtr && data.originalSenderPtr) {
                // End of notifying, restore values
                data.endpoint->senderPtr = data.originalSenderPtr;
            }
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

