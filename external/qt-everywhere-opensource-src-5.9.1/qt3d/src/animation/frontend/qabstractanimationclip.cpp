/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qabstractanimationclip.h"
#include "qabstractanimationclip_p.h"
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {

QAbstractAnimationClipPrivate::QAbstractAnimationClipPrivate()
    : Qt3DCore::QNodePrivate()
    , m_duration(0.0f)
{
}

void QAbstractAnimationClipPrivate::setDuration(float duration)
{
    if (qFuzzyCompare(duration, m_duration))
        return;

    Q_Q(QAbstractAnimationClip);
    bool wasBlocked = q->blockNotifications(true);
    m_duration = duration;
    emit q->durationChanged(duration);
    q->blockNotifications(wasBlocked);
}

/*!
    \class Qt3DAnimation::QAbstractAnimationClip
    \inherits Qt3DCore::QNode

    \inmodule Qt3DAnimation
    \since 5.9

    \brief QAbstractAnimationClip is the base class for types providing key frame animation data.

    To utilise the key frame animation framework in the Qt 3D Animation module
    the animator component in use needs to be provided with the key frame animation data. The
    animation data is provided by one of the concrete subclasses of QAbstractAnimationClip:

    \list
    \li Qt3DAnimation::QAnimationClip
    \li Qt3DAnimation::QAnimationClipLoader
    \endlist

    QAnimationClip should be used when you want to create the animation data
    programmatically within your application. The actual data is set with a
    QAnimationClipData value type.

    If you are loading baked animation data from a file, e.g. as created by an
    artist, then use the QAnimationClipLoader class and set its \c source property.

    Once the animation clip has been populated with data using the above
    methods, the read-only duration property will be updated by the Qt 3D Animation
    backend.

    The typical usage of animation clips is:

    \code
    auto animator = new QClipAnimator();
    auto clip = new QAnimationClipLoader();
    clip->setSource(QUrl::fromLocalFile("bounce.json"));
    animator->setClip(clip);
    animator->setChannelMapper(...);
    animator->setRunning(true);
    \endcode

    Animation clips are also used as the leaf node values in animation blend trees:

    \code
    // Create leaf nodes of blend tree
    auto slideClipValue = new QClipBlendValue(
        new QAnimationClipLoader(QUrl::fromLocalFile("slide.json")));
    auto bounceClipValue = new QClipBlendValue(
        new QAnimationClipLoader(QUrl::fromLocalFile("bounce.json")));

    // Create blend tree inner node
    auto additiveNode = new QAdditiveClipBlend();
    additiveNode->setBaseClip(slideClipValue);
    additiveNode->setAdditiveClip(bounceClipValue);
    additiveNode->setAdditiveFactor(0.5f);

    // Run the animator
    auto animator = new QBlendedClipAnimator();
    animator->setBlendTree(additiveNode);
    animator->setChannelMapper(...);
    animator->setRunning(true);
    \endcode

    \sa QAnimationClip, QAnimationClipLoader
*/

/*!
    \internal
*/
QAbstractAnimationClip::QAbstractAnimationClip(QAbstractAnimationClipPrivate &dd,
                                               Qt3DCore::QNode *parent)
    : Qt3DCore::QNode(dd, parent)
{
}

/*!
    Destroys this animation clip.
*/
QAbstractAnimationClip::~QAbstractAnimationClip()
{
}

/*!
    \property QAbstractAnimationClip::duration

    Holds the duration of the animation clip in seconds. Gets updated once the
    animation data is provided to Qt 3D using one of the concrete subclasses.
*/
float QAbstractAnimationClip::duration() const
{
    Q_D(const QAbstractAnimationClip);
    return d->m_duration;
}

/*!
    \internal
*/
void QAbstractAnimationClip::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &change)
{
    Q_D(QAbstractAnimationClip);
    if (change->type() == Qt3DCore::PropertyUpdated) {
        Qt3DCore::QPropertyUpdatedChangePtr e = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(change);
        if (e->propertyName() == QByteArrayLiteral("duration"))
            d->setDuration(e->value().toFloat());
    }
}

} // namespace Qt3DAnimation

QT_END_NAMESPACE
