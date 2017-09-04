/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "testaspect.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

TestAspect::TestAspect(Qt3DCore::QNode *root)
    : Qt3DRender::QRenderAspect(Qt3DRender::QRenderAspect::Synchronous)
    , m_jobManager(new Qt3DCore::QAspectJobManager())
{
    Qt3DCore::QAbstractAspectPrivate::get(this)->m_jobManager = m_jobManager.data();
    QRenderAspect::onRegistered();

    const Qt3DCore::QNodeCreatedChangeGenerator generator(root);
    const QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = generator.creationChanges();

    for (const Qt3DCore::QNodeCreatedChangeBasePtr change : creationChanges)
        d_func()->createBackendNode(change);
}

TestAspect::~TestAspect()
{
    QRenderAspect::onUnregistered();
}

Render::NodeManagers *TestAspect::nodeManagers() const
{
    return d_func()->m_renderer->nodeManagers();
}

void TestAspect::onRegistered()
{
    QRenderAspect::onRegistered();
}

void TestAspect::onUnregistered()
{
    QRenderAspect::onUnregistered();
}

} // Qt3DRender

QT_END_NAMESPACE
