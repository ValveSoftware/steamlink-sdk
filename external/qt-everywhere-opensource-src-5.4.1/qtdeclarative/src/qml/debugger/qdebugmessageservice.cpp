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

#include "qdebugmessageservice_p.h"
#include "qqmldebugservice_p_p.h"

#include <QDataStream>
#include <QMutex>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QDebugMessageService, qmlDebugMessageService)

void DebugMessageHandler(QtMsgType type, const QMessageLogContext &ctxt,
                         const QString &buf)
{
    QDebugMessageService::instance()->sendDebugMessage(type, ctxt, buf);
}

class QDebugMessageServicePrivate : public QQmlDebugServicePrivate
{
public:
    QDebugMessageServicePrivate()
        : oldMsgHandler(0)
        , prevState(QQmlDebugService::NotConnected)
    {
    }

    QtMessageHandler oldMsgHandler;
    QQmlDebugService::State prevState;
    QMutex initMutex;
};

QDebugMessageService::QDebugMessageService(QObject *parent) :
    QQmlDebugService(*(new QDebugMessageServicePrivate()),
                                   QStringLiteral("DebugMessages"), 2, parent)
{
    Q_D(QDebugMessageService);

    // don't execute stateChanged() in parallel
    QMutexLocker lock(&d->initMutex);
    registerService();
    if (state() == Enabled) {
        d->oldMsgHandler = qInstallMessageHandler(DebugMessageHandler);
        d->prevState = Enabled;
    }
}

QDebugMessageService *QDebugMessageService::instance()
{
    return qmlDebugMessageService();
}

void QDebugMessageService::sendDebugMessage(QtMsgType type,
                                            const QMessageLogContext &ctxt,
                                            const QString &buf)
{
    Q_D(QDebugMessageService);

    //We do not want to alter the message handling mechanism
    //We just eavesdrop and forward the messages to a port
    //only if a client is connected to it.
    QByteArray message;
    QQmlDebugStream ws(&message, QIODevice::WriteOnly);
    ws << QByteArray("MESSAGE") << type << buf.toUtf8();
    ws << QString::fromLatin1(ctxt.file).toUtf8();
    ws << ctxt.line << QString::fromLatin1(ctxt.function).toUtf8();

    sendMessage(message);
    if (d->oldMsgHandler)
        (*d->oldMsgHandler)(type, ctxt, buf);
}

void QDebugMessageService::stateChanged(State state)
{
    Q_D(QDebugMessageService);
    QMutexLocker lock(&d->initMutex);

    if (state != Enabled && d->prevState == Enabled) {
        QtMessageHandler handler = qInstallMessageHandler(d->oldMsgHandler);
        // has our handler been overwritten in between?
        if (handler != DebugMessageHandler)
            qInstallMessageHandler(handler);

    } else if (state == Enabled && d->prevState != Enabled) {
        d->oldMsgHandler = qInstallMessageHandler(DebugMessageHandler);
    }

    d->prevState = state;
}

QT_END_NAMESPACE
