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

#include <QGlobalStatic>

#include <policy/resource.h>
#include <policy/resources.h>
#include <policy/resource-set.h>

#include "resourcepolicyimpl.h"
#include "resourcepolicyint.h"

Q_GLOBAL_STATIC(ResourcePolicyInt, globalResourcePolicyInt);

ResourcePolicyImpl::ResourcePolicyImpl(QObject *parent)
    : QMediaPlayerResourceSetInterface(parent)
{
    ResourcePolicyInt *set = globalResourcePolicyInt;
    set->addClient(this);
}

ResourcePolicyImpl::~ResourcePolicyImpl()
{
    ResourcePolicyInt *set = globalResourcePolicyInt;
    if (!globalResourcePolicyInt.isDestroyed())
        set->removeClient(this);
}

bool ResourcePolicyImpl::isVideoEnabled() const
{
    ResourcePolicyInt *set = globalResourcePolicyInt;
    return set->isVideoEnabled(this);
}

void ResourcePolicyImpl::setVideoEnabled(bool videoEnabled)
{
    ResourcePolicyInt *set = globalResourcePolicyInt;
    set->setVideoEnabled(this, videoEnabled);
}

void ResourcePolicyImpl::acquire()
{
    ResourcePolicyInt *set = globalResourcePolicyInt;
    set->acquire(this);
}

void ResourcePolicyImpl::release()
{
    ResourcePolicyInt *set = globalResourcePolicyInt;
    set->release(this);
}

bool ResourcePolicyImpl::isGranted() const
{
    ResourcePolicyInt *set = globalResourcePolicyInt;
    return set->isGranted(this);
}

bool ResourcePolicyImpl::isAvailable() const
{
    ResourcePolicyInt *set = globalResourcePolicyInt;
    return set->isAvailable();
}
