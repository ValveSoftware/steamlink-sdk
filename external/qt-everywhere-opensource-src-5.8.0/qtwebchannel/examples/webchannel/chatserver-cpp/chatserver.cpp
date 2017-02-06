/****************************************************************************
**
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebChannel module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
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

void ChatServer::sendKeepAlive()
{
    emit keepAlive();
    m_keepAliveCheckTimer->start();
}

void ChatServer::checkKeepAliveResponses()
{
    qDebug() << "Keep Alive Check" << m_stillAliveUsers;
    m_userList = m_stillAliveUsers;
    m_stillAliveUsers.clear();
    m_userList.sort();
    emit userListChanged();
}

void ChatServer::keepAliveResponse(const QString& user)
{
    m_stillAliveUsers.append(user);
}


QStringList ChatServer::userList() const
{
    return m_userList;
}

QT_END_NAMESPACE
