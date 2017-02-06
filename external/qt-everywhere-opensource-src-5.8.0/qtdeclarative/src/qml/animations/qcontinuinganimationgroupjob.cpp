/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd.
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

#include "private/qcontinuinganimationgroupjob_p.h"
#include "private/qanimationjobutil_p.h"

QT_BEGIN_NAMESPACE

QContinuingAnimationGroupJob::QContinuingAnimationGroupJob()
    : QAnimationGroupJob()
{
}

QContinuingAnimationGroupJob::~QContinuingAnimationGroupJob()
{
}

void QContinuingAnimationGroupJob::updateCurrentTime(int /*currentTime*/)
{
    Q_ASSERT(firstChild());

    for (QAbstractAnimationJob *animation = firstChild(); animation; animation = animation->nextSibling()) {
        if (animation->state() == state()) {
            RETURN_IF_DELETED(animation->setCurrentTime(m_currentTime));
        }
    }
}

void QContinuingAnimationGroupJob::updateState(QAbstractAnimationJob::State newState,
                                          QAbstractAnimationJob::State oldState)
{
    QAnimationGroupJob::updateState(newState, oldState);

    switch (newState) {
    case Stopped:
        for (QAbstractAnimationJob *animation = firstChild(); animation; animation = animation->nextSibling())
            animation->stop();
        break;
    case Paused:
        for (QAbstractAnimationJob *animation = firstChild(); animation; animation = animation->nextSibling())
            if (animation->isRunning())
                animation->pause();
        break;
    case Running:
        if (!firstChild()) {
            stop();
            return;
        }
        for (QAbstractAnimationJob *animation = firstChild(); animation; animation = animation->nextSibling()) {
            resetUncontrolledAnimationFinishTime(animation);
            animation->setDirection(m_direction);
            animation->start();
        }
        break;
    }
}

void QContinuingAnimationGroupJob::updateDirection(QAbstractAnimationJob::Direction direction)
{
    if (!isStopped()) {
        for (QAbstractAnimationJob *animation = firstChild(); animation; animation = animation->nextSibling()) {
            animation->setDirection(direction);
        }
    }
}

void QContinuingAnimationGroupJob::uncontrolledAnimationFinished(QAbstractAnimationJob *animation)
{
    Q_ASSERT(animation && (animation->duration() == -1));
    int uncontrolledRunningCount = 0;

    for (QAbstractAnimationJob *child = firstChild(); child; child = child->nextSibling()) {
        if (child == animation)
            setUncontrolledAnimationFinishTime(animation, animation->currentTime());
        else if (uncontrolledAnimationFinishTime(child) == -1)
            ++uncontrolledRunningCount;
    }

    if (uncontrolledRunningCount > 0)
        return;

    setUncontrolledAnimationFinishTime(this, currentTime());
    stop();
}

void QContinuingAnimationGroupJob::debugAnimation(QDebug d) const
{
    d << "ContinuingAnimationGroupJob(" << hex << (const void *) this << dec << ")";

    debugChildren(d);
}

QT_END_NAMESPACE

