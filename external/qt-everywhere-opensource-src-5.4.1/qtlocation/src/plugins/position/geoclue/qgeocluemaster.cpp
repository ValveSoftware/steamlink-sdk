/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd, author: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include "qgeocluemaster.h"

#include <QtCore/QByteArray>
#include <QtCore/QMetaMethod>

QT_BEGIN_NAMESPACE

namespace
{

void position_provider_changed(GeoclueMasterClient *client, char *name, char *description,
                               char *service, char *path, QObject *handler)
{
    Q_UNUSED(client)
    Q_UNUSED(name)
    Q_UNUSED(description)

    const QByteArray pService = QByteArray(service);
    const QByteArray pPath = QByteArray(path);

    QMetaObject::invokeMethod(handler, "positionProviderChanged", Qt::QueuedConnection,
                              Q_ARG(QByteArray, pService), Q_ARG(QByteArray, pPath));
}

}

QGeoclueMaster::QGeoclueMaster(QObject *handler)
:   m_client(0), m_masterPosition(0), m_handler(handler)
{
#if !defined(GLIB_VERSION_2_36)
    g_type_init();
#endif
}

QGeoclueMaster::~QGeoclueMaster()
{
    releaseMasterClient();
}

bool QGeoclueMaster::hasMasterClient() const
{
    return m_client && m_masterPosition;
}

bool QGeoclueMaster::createMasterClient(GeoclueAccuracyLevel accuracy, GeoclueResourceFlags resourceFlags)
{
    Q_ASSERT(!m_client && !m_masterPosition);

    GeoclueMaster *master = geoclue_master_get_default();
    if (!master) {
        qCritical("QGeoclueMaster error creating GeoclueMaster");
        return false;
    }

    GError *error = 0;

    m_client = geoclue_master_create_client(master, 0, &error);
    g_object_unref (master);

    if (!m_client) {
        qCritical("QGeoclueMaster error creating GeoclueMasterClient.");
        if (error) {
            qCritical("Geoclue error: %s", error->message);
            g_error_free(error);
        }
        return false;
    }

    g_signal_connect(G_OBJECT(m_client), "position-provider-changed",
                     G_CALLBACK(position_provider_changed), m_handler);

    if (!geoclue_master_client_set_requirements(m_client, accuracy, 0, true,
                                                resourceFlags, &error)) {
        qCritical("QGeoclueMaster geoclue set_requirements failed.");
        if (error) {
            qCritical ("Geoclue error: %s", error->message);
            g_error_free (error);
        }
        g_object_unref(m_client);
        m_client = 0;
        return false;
    }

    // Need to create the master position interface even though it will not be used, otherwise
    // GetPositionProvider always returns empty strings.
    m_masterPosition = geoclue_master_client_create_position(m_client, 0);
    if (!m_masterPosition) {
        qCritical("QGeoclueMaster failed to get master position object");
        g_object_unref(m_client);
        m_client = 0;
        return false;
    }

    return true;
}

void QGeoclueMaster::releaseMasterClient()
{
    if (m_masterPosition) {
        g_object_unref(m_masterPosition);
        m_masterPosition = 0;
    }
    if (m_client) {
        g_signal_handlers_disconnect_by_func(G_OBJECT(m_client), (void *)position_provider_changed,
                                             m_handler);
        g_object_unref(m_client);
        m_client = 0;
    }
}

QT_END_NAMESPACE
