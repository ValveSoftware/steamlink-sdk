/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
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

#include "qt3dquick3dscene2d_p.h"
#include <Qt3DCore/qentity.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {
namespace Quick {

QQuick3DScene2D::QQuick3DScene2D(QObject *parent)
    : QObject(parent)
{
}

QQmlListProperty<Qt3DCore::QEntity> QQuick3DScene2D::entities()
{
    return QQmlListProperty<Qt3DCore::QEntity>(this, 0,
                                               &QQuick3DScene2D::appendEntity,
                                               &QQuick3DScene2D::entityCount,
                                               &QQuick3DScene2D::entityAt,
                                               &QQuick3DScene2D::clearEntities);
}

void QQuick3DScene2D::appendEntity(QQmlListProperty<Qt3DCore::QEntity> *list,
                                   Qt3DCore::QEntity *entity)
{
    QQuick3DScene2D *scene2d = qobject_cast<QQuick3DScene2D *>(list->object);
    if (scene2d)
        scene2d->parentScene2D()->addEntity(entity);
}

int QQuick3DScene2D::entityCount(QQmlListProperty<Qt3DCore::QEntity> *list)
{
    QQuick3DScene2D *scene2d = qobject_cast<QQuick3DScene2D *>(list->object);
    if (scene2d)
        return scene2d->parentScene2D()->entities().count();
    return 0;
}

Qt3DCore::QEntity *QQuick3DScene2D::entityAt(QQmlListProperty<Qt3DCore::QEntity> *list, int index)
{
    QQuick3DScene2D *scene2d = qobject_cast<QQuick3DScene2D *>(list->object);
    if (scene2d) {
        return qobject_cast<Qt3DCore::QEntity *>(
                    scene2d->parentScene2D()->entities().at(index));
    }
    return nullptr;
}

void QQuick3DScene2D::clearEntities(QQmlListProperty<Qt3DCore::QEntity> *list)
{
    QQuick3DScene2D *scene2d = qobject_cast<QQuick3DScene2D *>(list->object);
    if (scene2d) {
        QVector<Qt3DCore::QEntity*> entities = scene2d->parentScene2D()->entities();
        for (Qt3DCore::QEntity *e : entities)
            scene2d->parentScene2D()->removeEntity(e);
    }
}

} // namespace Quick
} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
