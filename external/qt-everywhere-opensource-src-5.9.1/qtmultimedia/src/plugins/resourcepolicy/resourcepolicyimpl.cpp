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
