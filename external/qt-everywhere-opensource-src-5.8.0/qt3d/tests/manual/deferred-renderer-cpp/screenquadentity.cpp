/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "screenquadentity.h"
#include "finaleffect.h"
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QParameter>
#include <Qt3DRender/QLayer>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QPlaneMesh>

QT_BEGIN_NAMESPACE

ScreenQuadEntity::ScreenQuadEntity(Qt3DCore::QNode *parent)
    : Qt3DCore::QEntity(parent)
    , m_layer(new Qt3DRender::QLayer(this))
    , m_effect(new FinalEffect(this))
{
    Qt3DRender::QMaterial *screenQuadMaterial = new Qt3DRender::QMaterial(this);
    screenQuadMaterial->setEffect(m_effect);

    Qt3DCore::QTransform *screenPlaneTransform = new Qt3DCore::QTransform(this);
    screenPlaneTransform->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(1.0f, 0.0f, 0.0f), 90.0f));

    Qt3DExtras::QPlaneMesh *planeMesh = new Qt3DExtras::QPlaneMesh();
    planeMesh->setMeshResolution(QSize(2, 2));
    planeMesh->setWidth(2.0f);
    planeMesh->setHeight(2.0f);

    addComponent(m_layer);
    addComponent(screenPlaneTransform);
    addComponent(screenQuadMaterial);
    addComponent(planeMesh);
}

Qt3DRender::QLayer *ScreenQuadEntity::layer() const
{
    return m_layer;
}

FinalEffect *ScreenQuadEntity::effect() const
{
    return m_effect;
}

QT_END_NAMESPACE
