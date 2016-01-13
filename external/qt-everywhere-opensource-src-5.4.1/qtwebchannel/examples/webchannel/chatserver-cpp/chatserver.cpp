/****************************************************************************
**
** Copyright (C) 2014 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebChannel module of the Qt Toolkit.
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

#include "chatserver.h"

#include <QtCore/QDebug>
#include <QTimer>
#include <QTime>

QT_BEGIN_NAMESPACE

ChatServer::ChatServer(QObject *parent)
    : QObject(parent)
{
    QTimer* t = new QTimer(this);
    connect(t, SIGNAL(timeout()), this, SLOT(sendKeepAlive()));
    t->start(10000);

    m_keepAliveCheckTimer = new QTimer(this);
    m_keepAliveCheckTimer->setSingleShot(true);
    m_keepAliveCheckTimer->setInterval(2000);
    connect(m_keepAliveCheckTimer, SIGNAL(timeout()), this, SLOT(checkKeepAliveResponses()));
}

ChatServer::~ChatServer()
{}


bool ChatServer::login(const QString& userName)
{
    //stop keepAliveCheck, when a new user logged in
    if (m_keepAliveCheckTimer->isActive()) {
        m_keepAliveCheckTimer->stop();
        m_stillAliveUsers.clear();
    }

    if (m_userList.contains(userName)) {
        return false;
    }

    qDebug() << "User logged in:" << userName;
    m_userList.append(userName);
    m_userList.sort();
    emit userListChanged();
    emit userCountChanged();
    return true;
}

bool ChatServer::logout(const QString& userName)
{
    if (!m_userList.contains(userName)) {
        return false;
    } else {
        m_userList.removeAt(m_userList.indexOf(userName));
        emit userListChanged();
        emit userCountChanged();
        return true;
    }
}

bool ChatServer::sendMessage(const QString& user, const QString& msg)
{
    if (m_userList.contains(user)) {
        emit newMessage(QTime::currentTime().toString("HH:mm:ss"), user, msg);
        return true;
    } else {
        return false;
    }
}

void ChatServer::sendKeepAlive() {
    emit keepAlive();
    m_keepAliveCheckTimer->start();
}

void ChatServer::checkKeepAliveResponses() {
    qDebug() << "Keep Alive Check" << m_stillAliveUsers;
    m_userList = m_stillAliveUsers;
    m_stillAliveUsers.clear();
    m_userList.sort();
    emit userListChanged();
}

void ChatServer::keepAliveResponse(const QString& user) {
    m_stillAliveUsers.append(user);
}


QStringList ChatServer::userList() const
{
    return m_userList;
}

QT_END_NAMESPACE
