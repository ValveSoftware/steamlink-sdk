/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qclipblendvalue.h"
#include "qclipblendvalue_p.h"
#include <Qt3DAnimation/qabstractanimationclip.h>
#include <Qt3DAnimation/qclipblendnodecreatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {

QClipBlendValuePrivate::QClipBlendValuePrivate()
    : QAbstractClipBlendNodePrivate()
    , m_clip(nullptr)
{
}

/*!
   \class QClipBlendValue
   \inherits Qt3DAnimation::QAbstractClipBlendNode
   \inmodule Qt3DAnimation
   \brief Class used for including a clip in a blend tree.
*/
QClipBlendValue::QClipBlendValue(Qt3DCore::QNode *parent)
    : QAbstractClipBlendNode(*new QClipBlendValuePrivate(), parent)
{
}

QClipBlendValue::QClipBlendValue(Qt3DAnimation::QAbstractAnimationClip *clip,
                                 Qt3DCore::QNode *parent)
    : QAbstractClipBlendNode(*new QClipBlendValuePrivate(), parent)
{
    setClip(clip);
}

QClipBlendValue::QClipBlendValue(QClipBlendValuePrivate &dd, Qt3DCore::QNode *parent)
    : QAbstractClipBlendNode(dd, parent)
{
}

QClipBlendValue::~QClipBlendValue()
{
}

Qt3DAnimation::QAbstractAnimationClip *QClipBlendValue::clip() const
{
    Q_D(const QClipBlendValue);
    return d->m_clip;
}

void QClipBlendValue::setClip(Qt3DAnimation::QAbstractAnimationClip *clip)
{
    Q_D(QClipBlendValue);
    if (d->m_clip == clip)
        return;

    if (d->m_clip)
        d->unregisterDestructionHelper(d->m_clip);

    if (clip && !clip->parent())
        clip->setParent(this);
    d->m_clip = clip;

    // Ensures proper bookkeeping
    if (d->m_clip)
        d->registerDestructionHelper(d->m_clip, &QClipBlendValue::setClip, d->m_clip);
    emit clipChanged(clip);
}

Qt3DCore::QNodeCreatedChangeBasePtr QClipBlendValue::createNodeCreationChange() const
{
    Q_D(const QClipBlendValue);
    auto creationChange = QClipBlendNodeCreatedChangePtr<QClipBlendValueData>::create(this);
    QClipBlendValueData &data = creationChange->data;
    data.clipId = Qt3DCore::qIdForNode(d->m_clip);
    return creationChange;
}

} // namespace Qt3DAnimation

QT_END_NAMESPACE
