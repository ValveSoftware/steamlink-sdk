/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
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

#include "eventconnection_p.h"

QT_BEGIN_NAMESPACE

QScxmlEventConnection::QScxmlEventConnection(QObject *parent) :
    QObject(parent), m_stateMachine(nullptr)
{
}

QStringList QScxmlEventConnection::events() const
{
    return m_events;
}

void QScxmlEventConnection::setEvents(const QStringList &events)
{
    if (events != m_events) {
        m_events = events;
        doConnect();
        emit eventsChanged();
    }
}

QScxmlStateMachine *QScxmlEventConnection::stateMachine() const
{
    return m_stateMachine;
}

void QScxmlEventConnection::setStateMachine(QScxmlStateMachine *stateMachine)
{
    if (stateMachine != m_stateMachine) {
        m_stateMachine = stateMachine;
        doConnect();
        emit stateMachineChanged();
    }
}

void QScxmlEventConnection::doConnect()
{
    for (const QMetaObject::Connection &connection : qAsConst(m_connections))
        disconnect(connection);
    m_connections.clear();
    if (m_stateMachine) {
        for (const QString &event : qAsConst(m_events)) {
            m_connections.append(m_stateMachine->connectToEvent(event, this,
                                                                &QScxmlEventConnection::occurred));
        }

    }

}

void QScxmlEventConnection::classBegin()
{
}

void QScxmlEventConnection::componentComplete()
{
    if (!m_stateMachine) {
        if ((m_stateMachine = qobject_cast<QScxmlStateMachine *>(parent())))
            doConnect();
    }
}

QT_END_NAMESPACE
