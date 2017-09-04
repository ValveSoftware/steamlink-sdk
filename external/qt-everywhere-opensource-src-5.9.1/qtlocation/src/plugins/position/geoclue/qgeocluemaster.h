/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd, author: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#ifndef QGEOCLUEMASTER_H
#define QGEOCLUEMASTER_H

#include "geocluetypes.h"

#include <QtCore/QObject>

class OrgFreedesktopGeoclueMasterInterface;
class OrgFreedesktopGeoclueInterface;
class OrgFreedesktopGeoclueMasterClientInterface;

QT_BEGIN_NAMESPACE

class QGeoclueMaster : public QObject
{
    Q_OBJECT

public:
    QGeoclueMaster(QObject *parent = 0);
    ~QGeoclueMaster();

    enum ResourceFlag
    {
        ResourceNone = 0,
        ResourceNetwork = 1 << 0,
        ResourceCell = 1 << 1,
        ResourceGps = 1 << 2,
        ResourceAll = (1 << 10) - 1
    };

    Q_DECLARE_FLAGS(ResourceFlags, ResourceFlag)

    bool hasMasterClient() const;
    bool createMasterClient(Accuracy::Level accuracyLevel, ResourceFlags resourceFlags);
    void releaseMasterClient();

signals:
    void positionProviderChanged(const QString &name, const QString &description,
                                 const QString &service, const QString &path);

private:
    OrgFreedesktopGeoclueMasterInterface *m_master;
    OrgFreedesktopGeoclueInterface *m_provider;
    OrgFreedesktopGeoclueMasterClientInterface *m_client;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QGeoclueMaster::ResourceFlags)

QT_END_NAMESPACE

#endif // QGEOCLUEMASTER_H
