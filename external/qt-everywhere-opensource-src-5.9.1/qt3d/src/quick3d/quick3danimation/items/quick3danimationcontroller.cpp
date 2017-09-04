/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "quick3danimationcontroller_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {
namespace Quick {

QQuick3DAnimationController::QQuick3DAnimationController(QObject *parent)
    : QObject(parent)
{
}

QQmlListProperty<QAnimationGroup> QQuick3DAnimationController::animationGroups()
{
    return QQmlListProperty<QAnimationGroup>(this, 0,
                                       &QQuick3DAnimationController::appendAnimationGroup,
                                       &QQuick3DAnimationController::animationGroupCount,
                                       &QQuick3DAnimationController::animationGroupAt,
                                       &QQuick3DAnimationController::clearAnimationGroups);
}


void QQuick3DAnimationController::appendAnimationGroup(QQmlListProperty<QAnimationGroup> *list, QAnimationGroup *bar)
{
    QQuick3DAnimationController *controller = qobject_cast<QQuick3DAnimationController *>(list->object);
    if (controller)
        controller->parentAnimationController()->addAnimationGroup(bar);
}

int QQuick3DAnimationController::animationGroupCount(QQmlListProperty<QAnimationGroup> *list)
{
    QQuick3DAnimationController *controller = qobject_cast<QQuick3DAnimationController *>(list->object);
    if (controller)
        return controller->parentAnimationController()->animationGroupList().count();
    return 0;
}

QAnimationGroup *QQuick3DAnimationController::animationGroupAt(QQmlListProperty<QAnimationGroup> *list, int index)
{
    QQuick3DAnimationController *controller = qobject_cast<QQuick3DAnimationController *>(list->object);
    if (controller)
        return qobject_cast<QAnimationGroup *>(controller->parentAnimationController()->getGroup(index));
    return nullptr;
}

void QQuick3DAnimationController::clearAnimationGroups(QQmlListProperty<QAnimationGroup> *list)
{
    QQuick3DAnimationController *controller = qobject_cast<QQuick3DAnimationController *>(list->object);
    if (controller) {
        QVector<QAnimationGroup *> emptyList;
        controller->parentAnimationController()->setAnimationGroups(emptyList);
    }
}


} // namespace Quick
} // namespace Qt3DAnimation

QT_END_NAMESPACE
