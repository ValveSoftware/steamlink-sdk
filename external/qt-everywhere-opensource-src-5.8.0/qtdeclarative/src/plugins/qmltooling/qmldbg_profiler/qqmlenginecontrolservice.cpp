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

#include "qqmlenginecontrolservice.h"
#include "qqmldebugpacket.h"
#include <QJSEngine>

QT_BEGIN_NAMESPACE

QQmlEngineControlServiceImpl::QQmlEngineControlServiceImpl(QObject *parent) :
    QQmlEngineControlService(1, parent)
{
}

void QQmlEngineControlServiceImpl::messageReceived(const QByteArray &message)
{
    QMutexLocker lock(&dataMutex);
    QQmlDebugPacket d(message);
    int command;
    int engineId;
    d >> command >> engineId;
    QJSEngine *engine = qobject_cast<QJSEngine *>(objectForId(engineId));
    if (command == StartWaitingEngine && startingEngines.contains(engine)) {
        startingEngines.removeOne(engine);
        emit attachedToEngine(engine);
    } else if (command == StopWaitingEngine && stoppingEngines.contains(engine)) {
        stoppingEngines.removeOne(engine);
        emit detachedFromEngine(engine);
    }
}

void QQmlEngineControlServiceImpl::engineAboutToBeAdded(QJSEngine *engine)
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

void QQmlEngineControlServiceImpl::engineAboutToBeRemoved(QJSEngine *engine)
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

void QQmlEngineControlServiceImpl::engineAdded(QJSEngine *engine)
{
    if (state() == Enabled) {
        QMutexLocker lock(&dataMutex);
        Q_ASSERT(!startingEngines.contains(engine));
        Q_ASSERT(!stoppingEngines.contains(engine));
        sendMessage(EngineAdded, engine);
    }
}

void QQmlEngineControlServiceImpl::engineRemoved(QJSEngine *engine)
{
    if (state() == Enabled) {
        QMutexLocker lock(&dataMutex);
        Q_ASSERT(!startingEngines.contains(engine));
        Q_ASSERT(!stoppingEngines.contains(engine));
        sendMessage(EngineRemoved, engine);
    }
}

void QQmlEngineControlServiceImpl::sendMessage(QQmlEngineControlServiceImpl::MessageType type, QJSEngine *engine)
{
    QQmlDebugPacket d;
    d << int(type) << idForObject(engine);
    emit messageToClient(name(), d.data());
}

void QQmlEngineControlServiceImpl::stateChanged(State)
{
    // We flush everything for any kind of state change, to avoid complicated timing issues.
    QMutexLocker lock(&dataMutex);
    foreach (QJSEngine *engine, startingEngines)
        emit attachedToEngine(engine);
    startingEngines.clear();
    foreach (QJSEngine *engine, stoppingEngines)
        emit detachedFromEngine(engine);
    stoppingEngines.clear();
}

QT_END_NAMESPACE
