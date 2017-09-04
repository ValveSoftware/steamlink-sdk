/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd, author: <robin.burchell@jollamobile.com>
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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
