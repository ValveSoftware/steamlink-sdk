/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebChannel module of the Qt Toolkit.
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

#include "qwebchannel.h"
#include "qwebchannel_p.h"
#include "qmetaobjectpublisher_p.h"
#include "qwebchannelabstracttransport.h"

#include <QJsonDocument>
#include <QJsonObject>

QT_BEGIN_NAMESPACE

/*!
    \class QWebChannel

    \inmodule QtWebChannel
    \brief Exposes QObjects to remote HTML clients.
    \since 5.4

    The QWebChannel fills the gap between C++ applications and HTML/JavaScript
    applications. By publishing a QObject derived object to a QWebChannel and
    using the \l{Qt WebChannel JavaScript API}{qwebchannel.js} on the HTML side, one can transparently access
    properties and public slots and methods of the QObject. No manual message
    passing and serialization of data is required, property updates and signal emission
    on the C++ side get automatically transmitted to the potentially remotely running HTML clients.
    On the client side, a JavaScript object will be created for any published C++ QObject. It mirrors the
    C++ object's API and thus is intuitively useable.

    The C++ QWebChannel API makes it possible to talk to any HTML client, which could run on a local
    or even remote machine. The only limitation is that the HTML client supports the JavaScript
    features used by \c{qwebchannel.js}. As such, one can interact
    with basically any modern HTML browser or standalone JavaScript runtime, such as node.js.

    There also exists a declarative \l{Qt WebChannel QML Types}{WebChannel API}.

    \sa {Qt WebChannel Standalone Example}, {Qt WebChannel JavaScript API}{JavaScript API}
*/

/*!
    \internal

    Remove a destroyed transport object from the list of known transports.
*/
void QWebChannelPrivate::_q_transportDestroyed(QObject *object)
{
    QWebChannelAbstractTransport *transport = static_cast<QWebChannelAbstractTransport*>(object);
    const int idx = transports.indexOf(transport);
    if (idx != -1) {
        transports.remove(idx);
        publisher->transportRemoved(transport);
    }
}

/*!
    \internal

    Shared code to initialize the QWebChannel from both constructors.
*/
void QWebChannelPrivate::init()
{
    Q_Q(QWebChannel);
    publisher = new QMetaObjectPublisher(q);
    QObject::connect(publisher, SIGNAL(blockUpdatesChanged(bool)),
                     q, SIGNAL(blockUpdatesChanged(bool)));
}

/*!
    Constructs the QWebChannel object with the given \a parent.

    Note that a QWebChannel is only fully operational once you connect it to a
    QWebChannelAbstractTransport. The HTML clients also need to be setup appropriately
    using \l{qtwebchannel-javascript.html}{\c qwebchannel.js}.
*/
QWebChannel::QWebChannel(QObject *parent)
: QObject(*(new QWebChannelPrivate), parent)
{
    Q_D(QWebChannel);
    d->init();
}

/*!
    \internal

    Construct a QWebChannel from an ancestor class with the given \a parent.

    \sa QQmlWebChannel
*/
QWebChannel::QWebChannel(QWebChannelPrivate &dd, QObject *parent)
: QObject(dd, parent)
{
    Q_D(QWebChannel);
    d->init();
}

/*!
    Destroys the QWebChannel.
*/
QWebChannel::~QWebChannel()
{
}

/*!
    Registers a group of objects to the QWebChannel.

    The properties, signals and public invokable methods of the objects are published to the remote clients.
    There, an object with the identifier used as key in the \a objects map is then constructed.

    \note A current limitation is that objects must be registered before any client is initialized.

    \sa QWebChannel::registerObject(), QWebChannel::deregisterObject(), QWebChannel::registeredObjects()
*/
void QWebChannel::registerObjects(const QHash< QString, QObject * > &objects)
{
    Q_D(QWebChannel);
    const QHash<QString, QObject *>::const_iterator end = objects.constEnd();
    for (QHash<QString, QObject *>::const_iterator it = objects.constBegin(); it != end; ++it) {
        d->publisher->registerObject(it.key(), it.value());
    }
}

/*!
    Returns the map of registered objects that are published to remote clients.

    \sa QWebChannel::registerObjects(), QWebChannel::registerObject(), QWebChannel::deregisterObject()
*/
QHash<QString, QObject *> QWebChannel::registeredObjects() const
{
    Q_D(const QWebChannel);
    return d->publisher->registeredObjects;
}

/*!
    Registers a single object to the QWebChannel.

    The properties, signals and public methods of the \a object are published to the remote clients.
    There, an object with the identifier \a id is then constructed.

    \note A current limitation is that objects must be registered before any client is initialized.

    \sa QWebChannel::registerObjects(), QWebChannel::deregisterObject(), QWebChannel::registeredObjects()
*/
void QWebChannel::registerObject(const QString &id, QObject *object)
{
    Q_D(QWebChannel);
    d->publisher->registerObject(id, object);
}

/*!
    Deregisters the given \a object from the QWebChannel.

    Remote clients will receive a \c destroyed signal for the given object.

    \sa QWebChannel::registerObjects(), QWebChannel::registerObject(), QWebChannel::registeredObjects()
*/
void QWebChannel::deregisterObject(QObject *object)
{
    Q_D(QWebChannel);
    // handling of deregistration is analogously to handling of a destroyed signal
    d->publisher->signalEmitted(object, s_destroyedSignalIndex, QVariantList() << QVariant::fromValue(object));
}

/*!
    \property QWebChannel::blockUpdates

    \brief When set to true, updates are blocked and remote clients will not be notified about property changes.

    The changes are recorded and sent to the clients once updates become unblocked again by setting
    this property to false. By default, updates are not blocked.
*/


bool QWebChannel::blockUpdates() const
{
    Q_D(const QWebChannel);
    return d->publisher->blockUpdates;
}

void QWebChannel::setBlockUpdates(bool block)
{
    Q_D(QWebChannel);
    d->publisher->setBlockUpdates(block);
}

/*!
    Connects the QWebChannel to the given \a transport object.

    The transport object then handles the communication between the C++ application and a remote
    HTML client.

    \sa QWebChannelAbstractTransport, QWebChannel::disconnectFrom()
*/
void QWebChannel::connectTo(QWebChannelAbstractTransport *transport)
{
    Q_D(QWebChannel);
    Q_ASSERT(transport);
    if (!d->transports.contains(transport)) {
        d->transports << transport;
        connect(transport, &QWebChannelAbstractTransport::messageReceived,
                d->publisher, &QMetaObjectPublisher::handleMessage,
                Qt::UniqueConnection);
        connect(transport, SIGNAL(destroyed(QObject*)),
                this, SLOT(_q_transportDestroyed(QObject*)));
    }
}

/*!
    Disconnects the QWebChannel from the \a transport object.

    \sa QWebChannel::connectTo()
*/
void QWebChannel::disconnectFrom(QWebChannelAbstractTransport *transport)
{
    Q_D(QWebChannel);
    const int idx = d->transports.indexOf(transport);
    if (idx != -1) {
        disconnect(transport, 0, this, 0);
        disconnect(transport, 0, d->publisher, 0);
        d->transports.remove(idx);
        d->publisher->transportRemoved(transport);
    }
}

QT_END_NAMESPACE

#include "moc_qwebchannel.cpp"
