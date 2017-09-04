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

#include "qadditiveclipblend.h"
#include "qadditiveclipblend_p.h"
#include <Qt3DAnimation/qclipblendnodecreatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {


/*!
    \qmltype AdditiveClipBlend
    \instantiates Qt3DAnimation::QAdditiveClipBlend
    \inqmlmodule Qt3D.Animation

    \since 5.9

    \brief Performs an additive blend of two animation clips based on an additive factor.

    QAdditiveClipBlend can be useful to create advanced animation effects based on
    individual animation clips. For example, if you:

    \list
    \li set the baseClip property to a normal walk cycle animation clip and
    \li set the additiveClip property to a shaking head difference clip,
    \endlist

    then adjusting the additiveFactor property will control how much of the additiveClip gets added
    on to the baseClip. This has he effect that with an additiveFactor of zero, this blend node will
    yield the original walk cycle clip. With an additiveFactor of 1, it will yield the walk cycle
    including a shaking head animation.

    The blending operation implemented by this class is:

    \badcode
        resultClip = baseClip + additiveFactor * additiveClip
    \endcode

    There is nothing stopping you from using values for the additiveFacor property outside the 0 to
    1 range, but please be aware that the input animation clips may not be authored in such a way
    for this to make sense.

    \sa BlendedClipAnimator
*/

/*!
    \class Qt3DAnimation::QAdditiveClipBlend
    \inherits Qt3DAnimation::QAbstractClipBlendNode

    \inmodule Qt3DAnimation
    \since 5.9

    \brief Performs an additive blend of two animation clips based on an additive factor.

    QAdditiveClipBlend can be useful to create advanced animation effects based on
    individual animation clips. For example, if you:

    \list
    \li set the baseClip property to a normal walk cycle animation clip and
    \li set the additiveClip property to a shaking head difference clip,
    \endlist

    then adjusting the additiveFactor property will control how much of the additiveClip gets added
    on to the baseClip. This has he effect that with an additiveFactor of zero, this blend node will
    yield the original walk cycle clip. With an additiveFactor of 1, it will yield the walk cycle
    including a shaking head animation.

    The blending operation implemented by this class is:

    \badcode
        resultClip = baseClip + additiveFactor * additiveClip
    \endcode

    There is nothing stopping you from using values for the additiveFacor property outside the 0 to
    1 range, but please be aware that the input animation clips may not be authored in such a way
    for this to make sense.

    \sa QBlendedClipAnimator
*/

QAdditiveClipBlendPrivate::QAdditiveClipBlendPrivate()
    : QAbstractClipBlendNodePrivate()
    , m_baseClip(nullptr)
    , m_additiveClip(nullptr)
    , m_additiveFactor(0.0f)
{
}

QAdditiveClipBlend::QAdditiveClipBlend(Qt3DCore::QNode *parent)
    : QAbstractClipBlendNode(*new QAdditiveClipBlendPrivate(), parent)
{
}

QAdditiveClipBlend::QAdditiveClipBlend(QAdditiveClipBlendPrivate &dd, Qt3DCore::QNode *parent)
    : QAbstractClipBlendNode(dd, parent)
{
}

QAdditiveClipBlend::~QAdditiveClipBlend()
{
}

Qt3DCore::QNodeCreatedChangeBasePtr QAdditiveClipBlend::createNodeCreationChange() const
{
    Q_D(const QAdditiveClipBlend);
    auto creationChange = QClipBlendNodeCreatedChangePtr<QAdditiveClipBlendData>::create(this);
    QAdditiveClipBlendData &data = creationChange->data;
    data.baseClipId = Qt3DCore::qIdForNode(d->m_baseClip);
    data.additiveClipId = Qt3DCore::qIdForNode(d->m_additiveClip);
    data.additiveFactor = d->m_additiveFactor;
    return creationChange;
}

/*!
    \qmlproperty real AdditiveClipBlend::additiveFactor

    Specifies the blending factor, typically between 0 and 1, to control the blending of
    two animation clips.
*/
/*!
    \property QAdditiveClipBlend::additiveFactor

    Specifies the blending factor, typically between 0 and 1, to control the blending of
    two animation clips.
 */
float QAdditiveClipBlend::additiveFactor() const
{
    Q_D(const QAdditiveClipBlend);
    return d->m_additiveFactor;
}

/*!
    \qmlproperty AbstractClipBlendNode baseClip

    This property holds the base animation clip. When the additiveFacgtor is zero the baseClip will
    also be the resulting clip of this blend node.
*/
/*!
    \property baseClip

    This property holds the base animation clip. When the additiveFacgtor is zero the baseClip will
    also be the resulting clip of this blend node.
*/
QAbstractClipBlendNode *QAdditiveClipBlend::baseClip() const
{
    Q_D(const QAdditiveClipBlend);
    return d->m_baseClip;
}

/*!
    \qmlproperty AbstractClipBlendNode additiveClip

    This property holds the additive clip to be blended with the baseClip. The amount of blending
    is controlled by the additiveFactor property.
*/
/*!
    \property additiveClip

    This property holds the additive clip to be blended with the baseClip. The amount of blending
    is controlled by the additiveFactor property.
*/
QAbstractClipBlendNode *QAdditiveClipBlend::additiveClip() const
{
    Q_D(const QAdditiveClipBlend);
    return d->m_additiveClip;
}

void QAdditiveClipBlend::setAdditiveFactor(float additiveFactor)
{
    Q_D(QAdditiveClipBlend);
    if (d->m_additiveFactor == additiveFactor)
        return;

    d->m_additiveFactor = additiveFactor;
    emit additiveFactorChanged(additiveFactor);
}

void QAdditiveClipBlend::setBaseClip(QAbstractClipBlendNode *baseClip)
{
    Q_D(QAdditiveClipBlend);
    if (d->m_baseClip == baseClip)
        return;

    if (d->m_baseClip)
        d->unregisterDestructionHelper(d->m_baseClip);

    if (baseClip && !baseClip->parent())
        baseClip->setParent(this);
    d->m_baseClip = baseClip;

    // Ensures proper bookkeeping
    if (d->m_baseClip)
        d->registerDestructionHelper(d->m_baseClip, &QAdditiveClipBlend::setBaseClip, d->m_baseClip);

    emit baseClipChanged(baseClip);
}

void QAdditiveClipBlend::setAdditiveClip(QAbstractClipBlendNode *additiveClip)
{
    Q_D(QAdditiveClipBlend);
    if (d->m_additiveClip == additiveClip)
        return;

    if (d->m_additiveClip)
        d->unregisterDestructionHelper(d->m_additiveClip);

    if (additiveClip && !additiveClip->parent())
        additiveClip->setParent(this);
    d->m_additiveClip = additiveClip;

    // Ensures proper bookkeeping
    if (d->m_additiveClip)
        d->registerDestructionHelper(d->m_additiveClip, &QAdditiveClipBlend::setAdditiveClip, d->m_additiveClip);
    emit additiveClipChanged(additiveClip);
}

} // Qt3DAnimation

QT_END_NAMESPACE
