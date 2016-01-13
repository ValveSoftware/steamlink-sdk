/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd, author: <robin.burchell@jollamobile.com>
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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

#ifndef RESOURCEPOLICYINT_H
#define RESOURCEPOLICYINT_H

#include <QObject>
#include <QMap>

#include <policy/resource-set.h>
#include <policy/resource.h>
#include <private/qmediaresourceset_p.h>
#include "resourcepolicyimpl.h"

namespace ResourcePolicy {
    class ResourceSet;
};

enum ResourceStatus {
    Initial = 0,
    RequestedResource,
    GrantedResource
};

struct clientEntry {
    int id;
    ResourcePolicyImpl *client;
    ResourceStatus status;
    bool videoEnabled;
};

class ResourcePolicyInt : public QObject
{
    Q_OBJECT
public:
    ResourcePolicyInt(QObject *parent = 0);
    ~ResourcePolicyInt();

    bool isVideoEnabled(const ResourcePolicyImpl *client) const;
    void setVideoEnabled(const ResourcePolicyImpl *client, bool videoEnabled);
    void acquire(const ResourcePolicyImpl *client);
    void release(const ResourcePolicyImpl *client);
    bool isGranted(const ResourcePolicyImpl *client) const;
    bool isAvailable() const;

    void addClient(ResourcePolicyImpl *client);
    void removeClient(ResourcePolicyImpl *client);

private slots:
    void handleResourcesGranted();
    void handleResourcesDenied();
    void handleResourcesReleased();
    void handleResourcesLost();
    void handleResourcesReleasedByManager();
    void handleResourcesBecameAvailable(const QList<ResourcePolicy::ResourceType> &resources);

private:
    void availabilityChanged(bool available);

    QMap<const ResourcePolicyImpl*, clientEntry> m_clients;

    int m_acquired;
    ResourceStatus m_status;
    int m_video;
    bool m_available;
    ResourcePolicy::ResourceSet *m_resourceSet;
};

#endif // RESOURCEPOLICYINT_H
