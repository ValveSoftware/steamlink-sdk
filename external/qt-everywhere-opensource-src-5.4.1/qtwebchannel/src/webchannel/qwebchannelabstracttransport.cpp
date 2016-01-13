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

#include "qwebchannelabstracttransport.h"

QT_BEGIN_NAMESPACE

/*!
    \class QWebChannelAbstractTransport

    \inmodule QtWebChannel
    \brief Communication channel between the C++ QWebChannel server and a HTML/JS client.
    \since 5.4

    Users of the QWebChannel must implement this interface and connect instances of it
    to the QWebChannel server for every client that should be connected to the QWebChannel.
    The {Qt WebChannel Standalone Example}{Standalone Example} shows how this can be done
    using Qt WebSockets. Qt WebKit implements this interface internally and uses the native
    WebKit IPC mechanism to transmit messages to HTML clients.

    \note The JSON message protocol is considered internal and might change over time.

    \sa {Qt WebChannel Standalone Example}
*/

/*!
    \fn QWebChannelAbstractTransport::messageReceived(const QJsonObject &message, QWebChannelAbstractTransport *transport)

    This signal must be emitted when a new JSON \a message was received from the remote client. The
    \a transport argument should be set to this transport object.
*/

/*!
    \fn QWebChannelAbstractTransport::sendMessage(const QJsonObject &message)

    Send a JSON \a message to the remote client. An implementation would serialize the message and
    transmit it to the remote JavaScript client.
*/

/*!
    Constructs a transport object with the given \a parent.
*/
QWebChannelAbstractTransport::QWebChannelAbstractTransport(QObject *parent)
: QObject(parent)
{

}

/*!
    Destroys the transport object.
*/
QWebChannelAbstractTransport::~QWebChannelAbstractTransport()
{

}

QT_END_NAMESPACE
