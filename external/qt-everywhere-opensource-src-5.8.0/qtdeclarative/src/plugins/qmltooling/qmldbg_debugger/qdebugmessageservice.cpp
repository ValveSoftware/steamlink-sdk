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

#include "qdebugmessageservice.h"
#include "qqmldebugpacket.h"
#include <private/qqmldebugconnector_p.h>

QT_BEGIN_NAMESPACE

void DebugMessageHandler(QtMsgType type, const QMessageLogContext &ctxt,
                         const QString &buf)
{
    QQmlDebugConnector::service<QDebugMessageServiceImpl>()->sendDebugMessage(type, ctxt, buf);
}

QDebugMessageServiceImpl::QDebugMessageServiceImpl(QObject *parent) :
    QDebugMessageService(2, parent), oldMsgHandler(0),
    prevState(QQmlDebugService::NotConnected)
{
    // don't execute stateChanged() in parallel
    QMutexLocker lock(&initMutex);
    timer.start();
    if (state() == Enabled) {
        oldMsgHandler = qInstallMessageHandler(DebugMessageHandler);
        prevState = Enabled;
    }
}

void QDebugMessageServiceImpl::sendDebugMessage(QtMsgType type,
                                            const QMessageLogContext &ctxt,
                                            const QString &buf)
{
    //We do not want to alter the message handling mechanism
    //We just eavesdrop and forward the messages to a port
    //only if a client is connected to it.
    QQmlDebugPacket ws;
    ws << QByteArray("MESSAGE") << int(type) << buf.toUtf8();
    ws << QByteArray(ctxt.file) << ctxt.line << QByteArray(ctxt.function);
    ws << QByteArray(ctxt.category) << timer.nsecsElapsed();

    emit messageToClient(name(), ws.data());
    if (oldMsgHandler)
        (*oldMsgHandler)(type, ctxt, buf);
}

void QDebugMessageServiceImpl::stateChanged(State state)
{
    QMutexLocker lock(&initMutex);

    if (state != Enabled && prevState == Enabled) {
        QtMessageHandler handler = qInstallMessageHandler(oldMsgHandler);
        // has our handler been overwritten in between?
        if (handler != DebugMessageHandler)
            qInstallMessageHandler(handler);

    } else if (state == Enabled && prevState != Enabled) {
        oldMsgHandler = qInstallMessageHandler(DebugMessageHandler);
    }

    prevState = state;
}

void QDebugMessageServiceImpl::synchronizeTime(const QElapsedTimer &otherTimer)
{
    QMutexLocker lock(&initMutex);
    timer = otherTimer;
}

QT_END_NAMESPACE
