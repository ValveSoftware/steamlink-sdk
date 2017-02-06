/****************************************************************************
**
** Copyright (C) 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwaylandclient.h"
#include <QtCore/private/qobject_p.h>

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/private/qwaylandcompositor_p.h>


#include <wayland-server.h>
#include <wayland-util.h>

QT_BEGIN_NAMESPACE

class QWaylandClientPrivate : public QObjectPrivate
{
public:
    QWaylandClientPrivate(QWaylandCompositor *compositor, wl_client *_client)
        : compositor(compositor)
        , client(_client)
    {
        // Save client credentials
        wl_client_get_credentials(client, &pid, &uid, &gid);
    }

    ~QWaylandClientPrivate()
    {
    }

    static void client_destroy_callback(wl_listener *listener, void *data)
    {
        Q_UNUSED(data);

        QWaylandClient *client = reinterpret_cast<Listener *>(listener)->parent;
        Q_ASSERT(client != 0);
        delete client;
    }

    QWaylandCompositor *compositor;
    wl_client *client;

    uid_t uid;
    gid_t gid;
    pid_t pid;

    struct Listener {
        wl_listener listener;
        QWaylandClient *parent;
    };
    Listener listener;
};

/*!
 * \qmltype WaylandClient
 * \inqmlmodule QtWayland.Compositor
 * \since 5.8
 * \brief Represents a client connecting to the WaylandCompositor.
 *
 * This type represents a client connecting to the compositor using the Wayland protocol.
 * It corresponds to the Wayland interface wl_client.
 */

/*!
 * \class QWaylandClient
 * \inmodule QtWaylandCompositor
 * \since 5.8
 * \brief The QWaylandClient class represents a client connecting to the QWaylandCompositor.
 *
 * This class corresponds to a client connecting to the compositor using the Wayland protocol.
 * It corresponds to the Wayland interface wl_client.
 */

/*!
 * Constructs a QWaylandClient for the \a compositor and the Wayland \a client.
 */
QWaylandClient::QWaylandClient(QWaylandCompositor *compositor, wl_client *client)
    : QObject(*new QWaylandClientPrivate(compositor, client))
{
    Q_D(QWaylandClient);

    // Destroy wrapper when the client goes away
    d->listener.parent = this;
    d->listener.listener.notify = QWaylandClientPrivate::client_destroy_callback;
    wl_client_add_destroy_listener(client, &d->listener.listener);

    QWaylandCompositorPrivate::get(compositor)->addClient(this);
}

/*!
 * Destroys the QWaylandClient.
 */
QWaylandClient::~QWaylandClient()
{
    Q_D(QWaylandClient);

    // Remove listener from signal
    wl_list_remove(&d->listener.listener.link);

    QWaylandCompositorPrivate::get(d->compositor)->removeClient(this);
}

/*!
 * Returns the QWaylandClient corresponding to the Wayland client \a wlClient and \a compositor.
 * If a QWaylandClient has not already been created for a client, it is
 * created and returned.
 */
QWaylandClient *QWaylandClient::fromWlClient(QWaylandCompositor *compositor, wl_client *wlClient)
{
    if (!wlClient)
        return 0;

    QWaylandClient *client = Q_NULLPTR;

    wl_listener *l = wl_client_get_destroy_listener(wlClient,
        QWaylandClientPrivate::client_destroy_callback);
    if (l)
        client = reinterpret_cast<QWaylandClientPrivate::Listener *>(
            wl_container_of(l, (QWaylandClientPrivate::Listener *)0, listener))->parent;

    if (!client) {
        // The original idea was to create QWaylandClient instances when
        // a client bound wl_compositor, but it's legal for a client to
        // bind several times resulting in multiple QWaylandClient
        // instances for the same wl_client therefore we create it from
        // here on demand
        client = new QWaylandClient(compositor, wlClient);
    }

    return client;
}

/*!
 * \qmlproperty object QtWaylandCompositor::WaylandClient::compositor
 *
 * This property holds the compositor of this WaylandClient.
 */

/*!
 * \property QWaylandClient::compositor
 *
 * This property holds the compositor of this QWaylandClient.
 */
QWaylandCompositor *QWaylandClient::compositor() const
{
    Q_D(const QWaylandClient);

    return d->compositor;
}

/*!
 * Returns the Wayland client of this QWaylandClient.
 */
wl_client *QWaylandClient::client() const
{
    Q_D(const QWaylandClient);

    return d->client;
}

/*!
 * \qmlproperty int QtWaylandCompositor::WaylandClient::userId
 *
 * This property holds the user id of this WaylandClient.
 */

/*!
 * \property QWaylandClient::userId
 * \readonly
 *
 * This property holds the user id of this QWaylandClient.
 */
qint64 QWaylandClient::userId() const
{
    Q_D(const QWaylandClient);

    return d->uid;
}

/*!
 * \qmlproperty int QtWaylandCompositor::WaylandClient::groupId
 * \readonly
 *
 * This property holds the group id of this WaylandClient.
 */

/*!
 * \property int QWaylandClient::groupId
 *
 * This property holds the group id of this QWaylandClient.
 */
qint64 QWaylandClient::groupId() const
{
    Q_D(const QWaylandClient);

    return d->gid;
}

/*!
 * \qmlproperty int QtWaylandCompositor::WaylandClient::processId
 * \readonly
 *
 * This property holds the process id of this WaylandClient.
 */

/*!
 * \property QWaylandClient::processId
 *
 * This property holds the process id of this QWaylandClient.
 */
qint64 QWaylandClient::processId() const
{
    Q_D(const QWaylandClient);

    return d->pid;
}

/*!
 * \qmlmethod void QtWaylandCompositor::WaylandClient::kill(signal)
 *
 * Kills the client with the specified \a signal.
 */

/*!
 * Kills the client with the specified \a signal.
 */
void QWaylandClient::kill(int signal)
{
    Q_D(QWaylandClient);

    ::kill(d->pid, signal);
}

/*!
 * \qmlmethod void QtWaylandCompositor::WaylandClient::close()
 *
 * Closes the client
 */

/*!
 * Closes the client.
 */
void QWaylandClient::close()
{
    Q_D(QWaylandClient);
    d->compositor->destroyClient(this);
}

QT_END_NAMESPACE
