/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "private/qanimationgroupjob_p.h"

QT_BEGIN_NAMESPACE

QAnimationGroupJob::QAnimationGroupJob()
    : QAbstractAnimationJob(), m_firstChild(0), m_lastChild(0)
{
    m_isGroup = true;
}

QAnimationGroupJob::~QAnimationGroupJob()
{
    clear();
}

void QAnimationGroupJob::topLevelAnimationLoopChanged()
{
    for (QAbstractAnimationJob *animation = firstChild(); animation; animation = animation->nextSibling())
        animation->fireTopLevelAnimationLoopChanged();
}

void QAnimationGroupJob::appendAnimation(QAbstractAnimationJob *animation)
{
    if (QAnimationGroupJob *oldGroup = animation->m_group)
        oldGroup->removeAnimation(animation);

    Q_ASSERT(!animation->previousSibling() && !animation->nextSibling());

    if (m_lastChild)
        m_lastChild->m_nextSibling = animation;
    else
        m_firstChild = animation;
    animation->m_previousSibling = m_lastChild;
    m_lastChild = animation;

    animation->m_group = this;
    animationInserted(animation);
}

void QAnimationGroupJob::prependAnimation(QAbstractAnimationJob *animation)
{
    if (QAnimationGroupJob *oldGroup = animation->m_group)
        oldGroup->removeAnimation(animation);

    Q_ASSERT(!previousSibling() && !nextSibling());

    if (m_firstChild)
        m_firstChild->m_previousSibling = animation;
    else
        m_lastChild = animation;
    animation->m_nextSibling = m_firstChild;
    m_firstChild = animation;

    animation->m_group = this;
    animationInserted(animation);
}

void QAnimationGroupJob::removeAnimation(QAbstractAnimationJob *animation)
{
    Q_ASSERT(animation);
    Q_ASSERT(animation->m_group == this);
    QAbstractAnimationJob *prev = animation->previousSibling();
    QAbstractAnimationJob *next = animation->nextSibling();

    if (prev)
        prev->m_nextSibling = next;
    else
        m_firstChild = next;

    if (next)
        next->m_previousSibling = prev;
    else
        m_lastChild = prev;

    animation->m_previousSibling = 0;
    animation->m_nextSibling = 0;

    animation->m_group = 0;
    animationRemoved(animation, prev, next);
}

void QAnimationGroupJob::clear()
{
    QAbstractAnimationJob *child = firstChild();
    QAbstractAnimationJob *nextSibling = 0;
    while (child != 0) {
         child->m_group = 0;
         nextSibling = child->nextSibling();
         delete child;
         child = nextSibling;
    }
    m_firstChild = 0;
    m_lastChild = 0;
}

void QAnimationGroupJob::resetUncontrolledAnimationsFinishTime()
{
    for (QAbstractAnimationJob *animation = firstChild(); animation; animation = animation->nextSibling()) {
        if (animation->duration() == -1 || animation->loopCount() < 0) {
            resetUncontrolledAnimationFinishTime(animation);
        }
    }
}

void QAnimationGroupJob::resetUncontrolledAnimationFinishTime(QAbstractAnimationJob *anim)
{
    setUncontrolledAnimationFinishTime(anim, -1);
}

void QAnimationGroupJob::setUncontrolledAnimationFinishTime(QAbstractAnimationJob *anim, int time)
{
    anim->m_uncontrolledFinishTime = time;
}

void QAnimationGroupJob::uncontrolledAnimationFinished(QAbstractAnimationJob *animation)
{
    Q_UNUSED(animation);
}

void QAnimationGroupJob::animationRemoved(QAbstractAnimationJob* anim, QAbstractAnimationJob* , QAbstractAnimationJob* )
{
    resetUncontrolledAnimationFinishTime(anim);
    if (!firstChild()) {
        m_currentTime = 0;
        stop();
    }
}

void QAnimationGroupJob::debugChildren(QDebug d) const
{
    int indentLevel = 1;
    const QAnimationGroupJob *group = this;
    while ((group = group->m_group))
        ++indentLevel;

    QByteArray ind(indentLevel, ' ');
    for (QAbstractAnimationJob *child = firstChild(); child; child = child->nextSibling())
        d << "\n" << ind.constData() << child;
}

QT_END_NAMESPACE
