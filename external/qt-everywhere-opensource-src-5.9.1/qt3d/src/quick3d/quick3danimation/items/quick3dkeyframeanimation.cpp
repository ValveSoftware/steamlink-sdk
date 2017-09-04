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

#include "quick3dkeyframeanimation_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {
namespace Quick {

QQuick3DKeyframeAnimation::QQuick3DKeyframeAnimation(QObject *parent)
    : QObject(parent)
{
}

QQmlListProperty<Qt3DCore::QTransform> QQuick3DKeyframeAnimation::keyframes()
{
    return QQmlListProperty<Qt3DCore::QTransform>(this, 0,
                                       &QQuick3DKeyframeAnimation::appendKeyframe,
                                       &QQuick3DKeyframeAnimation::keyframeCount,
                                       &QQuick3DKeyframeAnimation::keyframeAt,
                                       &QQuick3DKeyframeAnimation::clearKeyframes);
}

void QQuick3DKeyframeAnimation::appendKeyframe(QQmlListProperty<Qt3DCore::QTransform> *list,
                                               Qt3DCore::QTransform *transform)
{
    QQuick3DKeyframeAnimation *keyframeAnimation
            = qobject_cast<QQuick3DKeyframeAnimation *>(list->object);
    if (keyframeAnimation)
        keyframeAnimation->parentKeyframeAnimation()->addKeyframe(transform);
}

int QQuick3DKeyframeAnimation::keyframeCount(QQmlListProperty<Qt3DCore::QTransform> *list)
{
    QQuick3DKeyframeAnimation *keyframeAnimation
            = qobject_cast<QQuick3DKeyframeAnimation *>(list->object);
    if (keyframeAnimation)
        return keyframeAnimation->parentKeyframeAnimation()->keyframeList().count();
    return 0;
}

Qt3DCore::QTransform *QQuick3DKeyframeAnimation::keyframeAt(QQmlListProperty<Qt3DCore::QTransform> *list,
                                                            int index)
{
    QQuick3DKeyframeAnimation *keyframeAnimation
            = qobject_cast<QQuick3DKeyframeAnimation *>(list->object);
    if (keyframeAnimation) {
        return qobject_cast<Qt3DCore::QTransform *>(
                    keyframeAnimation->parentKeyframeAnimation()->keyframeList().at(index));
    }
    return nullptr;
}

void QQuick3DKeyframeAnimation::clearKeyframes(QQmlListProperty<Qt3DCore::QTransform> *list)
{
    QQuick3DKeyframeAnimation *keyframeAnimation
            = qobject_cast<QQuick3DKeyframeAnimation *>(list->object);
    if (keyframeAnimation) {
        QVector<Qt3DCore::QTransform *> emptyList;
        keyframeAnimation->parentKeyframeAnimation()->setKeyframes(emptyList);
    }
}

} // namespace Quick
} // namespace Qt3DAnimation

QT_END_NAMESPACE
