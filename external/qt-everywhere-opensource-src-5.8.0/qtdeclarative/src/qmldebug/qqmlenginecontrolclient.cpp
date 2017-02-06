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

#include "qqmlenginecontrolclient_p.h"
#include "qqmlenginecontrolclient_p_p.h"
#include "qqmldebugconnection_p.h"

#include <private/qqmldebugserviceinterfaces_p.h>
#include <private/qpacket_p.h>

QT_BEGIN_NAMESPACE

QQmlEngineControlClient::QQmlEngineControlClient(QQmlDebugConnection *connection) :
    QQmlDebugClient(*(new QQmlEngineControlClientPrivate(connection)))
{
}

QQmlEngineControlClient::QQmlEngineControlClient(QQmlEngineControlClientPrivate &dd) :
    QQmlDebugClient(dd)
{
}

/*!
 * Block the starting or stopping of the engine with id \a engineId for now. By calling
 * releaseEngine later the block can be lifted again. In the debugged application the engine control
 * server waits until a message is received before continuing. So by not sending a message here we
 * delay the process. Blocks add up. You have to call releaseEngine() as often as you've called
 * blockEngine before. The intention of that is to allow different debug clients to use the same
 * engine control and communicate with their respective counterparts before the QML engine starts or
 * shuts down.
 */
void QQmlEngineControlClient::blockEngine(int engineId)
{
    Q_D(QQmlEngineControlClient);
    Q_ASSERT(d->blockedEngines.contains(engineId));
    d->blockedEngines[engineId].blockers++;
}

/*!
 * Release the engine with id \a engineId. If no other blocks are present, depending on what the
 * engine is waiting for, the start or stop command is sent to the process being debugged.
 */
void QQmlEngineControlClient::releaseEngine(int engineId)
{
    Q_D(QQmlEngineControlClient);
    Q_ASSERT(d->blockedEngines.contains(engineId));

    QQmlEngineControlClientPrivate::EngineState &state = d->blockedEngines[engineId];
    if (--state.blockers == 0) {
        Q_ASSERT(state.releaseCommand != QQmlEngineControlClientPrivate::InvalidCommand);
        d->sendCommand(state.releaseCommand, engineId);
        d->blockedEngines.remove(engineId);
    }
}

QList<int> QQmlEngineControlClient::blockedEngines() const
{
    Q_D(const QQmlEngineControlClient);
    return d->blockedEngines.keys();
}

void QQmlEngineControlClient::messageReceived(const QByteArray &data)
{
    Q_D(QQmlEngineControlClient);
    QPacket stream(d->connection->currentDataStreamVersion(), data);
    int message;
    int id;
    QString name;

    stream >> message >> id;

    if (!stream.atEnd())
        stream >> name;

    QQmlEngineControlClientPrivate::EngineState &state = d->blockedEngines[id];
    Q_ASSERT(state.blockers == 0);
    Q_ASSERT(state.releaseCommand == QQmlEngineControlClientPrivate::InvalidCommand);

    switch (message) {
    case QQmlEngineControlClientPrivate::EngineAboutToBeAdded:
        state.releaseCommand = QQmlEngineControlClientPrivate::StartWaitingEngine;
        emit engineAboutToBeAdded(id, name);
        break;
    case QQmlEngineControlClientPrivate::EngineAdded:
        emit engineAdded(id, name);
        break;
    case QQmlEngineControlClientPrivate::EngineAboutToBeRemoved:
        state.releaseCommand = QQmlEngineControlClientPrivate::StopWaitingEngine;
        emit engineAboutToBeRemoved(id, name);
        break;
    case QQmlEngineControlClientPrivate::EngineRemoved:
        emit engineRemoved(id, name);
        break;
    }

    if (state.blockers == 0 &&
            state.releaseCommand != QQmlEngineControlClientPrivate::InvalidCommand) {
        d->sendCommand(state.releaseCommand, id);
        d->blockedEngines.remove(id);
    }
}

QQmlEngineControlClientPrivate::QQmlEngineControlClientPrivate(QQmlDebugConnection *connection) :
    QQmlDebugClientPrivate(QQmlEngineControlService::s_key, connection)
{
}

void QQmlEngineControlClientPrivate::sendCommand(
        QQmlEngineControlClientPrivate::CommandType command, int engineId)
{
    Q_Q(QQmlEngineControlClient);
    QPacket stream(connection->currentDataStreamVersion());
    stream << int(command) << engineId;
    q->sendMessage(stream.data());
}

QT_END_NAMESPACE
