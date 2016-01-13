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

#include <QQmlEngine>
#include "qqmldebug.h"
#include "qqmlenginecontrolservice_p.h"

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QQmlEngineControlService, qmlEngineControlService)

QQmlEngineControlService::QQmlEngineControlService() :
    QQmlDebugService(QStringLiteral("EngineControl"), 1)
{
    QMutexLocker lock(&dataMutex);
    registerService();
}

QQmlEngineControlService *QQmlEngineControlService::instance()
{
    return qmlEngineControlService();
}

void QQmlEngineControlService::messageReceived(const QByteArray &message)
{
    QMutexLocker lock(&dataMutex);
    QQmlDebugStream d(message);
    int command;
    int engineId;
    d >> command >> engineId;
    QQmlEngine *engine = qobject_cast<QQmlEngine *>(objectForId(engineId));
    if (command == StartWaitingEngine && startingEngines.contains(engine)) {
        startingEngines.removeOne(engine);
        emit attachedToEngine(engine);
    } else if (command == StopWaitingEngine && stoppingEngines.contains(engine)) {
        stoppingEngines.removeOne(engine);
        emit detachedFromEngine(engine);
    }
}

void QQmlEngineControlService::engineAboutToBeAdded(QQmlEngine *engine)
{
    QMutexLocker lock(&dataMutex);
    if (state() == Enabled) {
        Q_ASSERT(!stoppingEngines.contains(engine));
        Q_ASSERT(!startingEngines.contains(engine));
        startingEngines.append(engine);
        sendMessage(EngineAboutToBeAdded, engine);
    } else {
        emit attachedToEngine(engine);
    }
}

void QQmlEngineControlService::engineAboutToBeRemoved(QQmlEngine *engine)
{
    QMutexLocker lock(&dataMutex);
    if (state() == Enabled) {
        Q_ASSERT(!stoppingEngines.contains(engine));
        Q_ASSERT(!startingEngines.contains(engine));
        stoppingEngines.append(engine);
        sendMessage(EngineAboutToBeRemoved, engine);
    } else {
        emit detachedFromEngine(engine);
    }
}

void QQmlEngineControlService::engineAdded(QQmlEngine *engine)
{
    if (state() == Enabled) {
        QMutexLocker lock(&dataMutex);
        Q_ASSERT(!startingEngines.contains(engine));
        Q_ASSERT(!stoppingEngines.contains(engine));
        sendMessage(EngineAdded, engine);
    }
}

void QQmlEngineControlService::engineRemoved(QQmlEngine *engine)
{
    if (state() == Enabled) {
        QMutexLocker lock(&dataMutex);
        Q_ASSERT(!startingEngines.contains(engine));
        Q_ASSERT(!stoppingEngines.contains(engine));
        sendMessage(EngineRemoved, engine);
    }
}

void QQmlEngineControlService::sendMessage(QQmlEngineControlService::MessageType type, QQmlEngine *engine)
{
    QByteArray message;
    QQmlDebugStream d(&message, QIODevice::WriteOnly);
    d << type << idForObject(engine);
    QQmlDebugService::sendMessage(message);
}

void QQmlEngineControlService::stateChanged(State)
{
    // We flush everything for any kind of state change, to avoid complicated timing issues.
    QMutexLocker lock(&dataMutex);
    foreach (QQmlEngine *engine, startingEngines)
        emit attachedToEngine(engine);
    startingEngines.clear();
    foreach (QQmlEngine *engine, stoppingEngines)
        emit detachedFromEngine(engine);
    stoppingEngines.clear();
}

QT_END_NAMESPACE
