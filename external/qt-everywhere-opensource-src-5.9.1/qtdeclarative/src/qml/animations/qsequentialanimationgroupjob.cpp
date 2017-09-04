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

#include "private/qsequentialanimationgroupjob_p.h"
#include "private/qpauseanimationjob_p.h"
#include "private/qanimationjobutil_p.h"

QT_BEGIN_NAMESPACE

QSequentialAnimationGroupJob::QSequentialAnimationGroupJob()
    : QAnimationGroupJob()
    , m_currentAnimation(0)
    , m_previousLoop(0)
{
}

QSequentialAnimationGroupJob::~QSequentialAnimationGroupJob()
{
}

bool QSequentialAnimationGroupJob::atEnd() const
{
    // we try to detect if we're at the end of the group
    //this is true if the following conditions are true:
    // 1. we're in the last loop
    // 2. the direction is forward
    // 3. the current animation is the last one
    // 4. the current animation has reached its end

    const int animTotalCurrentTime = m_currentAnimation->currentTime();
    return (m_currentLoop == m_loopCount - 1
        && m_direction == Forward
        && !m_currentAnimation->nextSibling()
        && animTotalCurrentTime == animationActualTotalDuration(m_currentAnimation));
}

int QSequentialAnimationGroupJob::animationActualTotalDuration(QAbstractAnimationJob *anim) const
{
    int ret = anim->totalDuration();
    if (ret == -1) {
        int done = uncontrolledAnimationFinishTime(anim);
        // If the animation has reached the end, use the uncontrolledFinished value.
        if (done >= 0 && (anim->loopCount() - 1 == anim->currentLoop() || anim->state() == Stopped))
            return done;
    }
    return ret;
}

QSequentialAnimationGroupJob::AnimationIndex QSequentialAnimationGroupJob::indexForCurrentTime() const
{
    Q_ASSERT(firstChild());

    AnimationIndex ret;
    QAbstractAnimationJob *anim = 0;
    int duration = 0;

    for (anim = firstChild(); anim; anim = anim->nextSibling()) {
        duration = animationActualTotalDuration(anim);

        // 'animation' is the current animation if one of these reasons is true:
        // 1. it's duration is undefined
        // 2. it ends after msecs
        // 3. it is the last animation (this can happen in case there is at least 1 uncontrolled animation)
        // 4. it ends exactly in msecs and the direction is backwards
        if (duration == -1 || m_currentTime < (ret.timeOffset + duration)
            || (m_currentTime == (ret.timeOffset + duration) && m_direction == QAbstractAnimationJob::Backward)) {
            ret.animation = anim;
            return ret;
        }

        if (anim == m_currentAnimation) {
            ret.afterCurrent = true;
        }

        // 'animation' has a non-null defined duration and is not the one at time 'msecs'.
        ret.timeOffset += duration;
    }

    // this can only happen when one of those conditions is true:
    // 1. the duration of the group is undefined and we passed its actual duration
    // 2. there are only 0-duration animations in the group
    ret.timeOffset -= duration;
    ret.animation = lastChild();
    return ret;
}

void QSequentialAnimationGroupJob::restart()
{
    // restarting the group by making the first/last animation the current one
    if (m_direction == Forward) {
        m_previousLoop = 0;
        if (m_currentAnimation == firstChild())
            activateCurrentAnimation();
        else
            setCurrentAnimation(firstChild());
    }
    else { // direction == Backward
        m_previousLoop = m_loopCount - 1;
        if (m_currentAnimation == lastChild())
            activateCurrentAnimation();
        else
            setCurrentAnimation(lastChild());
    }
}

void QSequentialAnimationGroupJob::advanceForwards(const AnimationIndex &newAnimationIndex)
{
    if (m_previousLoop < m_currentLoop) {
        // we need to fast forward to the end
        for (QAbstractAnimationJob *anim = m_currentAnimation; anim; anim = anim->nextSibling()) {
            RETURN_IF_DELETED(setCurrentAnimation(anim, true));
            RETURN_IF_DELETED(anim->setCurrentTime(animationActualTotalDuration(anim)));
        }
        // this will make sure the current animation is reset to the beginning
        if (firstChild() && !firstChild()->nextSibling()) {   //count == 1
            // we need to force activation because setCurrentAnimation will have no effect
            RETURN_IF_DELETED(activateCurrentAnimation());
        } else {
            RETURN_IF_DELETED(setCurrentAnimation(firstChild(), true));
        }
    }

    // and now we need to fast forward from the current position to
    for (QAbstractAnimationJob *anim = m_currentAnimation; anim && anim != newAnimationIndex.animation; anim = anim->nextSibling()) {     //### WRONG,
        RETURN_IF_DELETED(setCurrentAnimation(anim, true));
        RETURN_IF_DELETED(anim->setCurrentTime(animationActualTotalDuration(anim)));
    }
    // setting the new current animation will happen later
}

void QSequentialAnimationGroupJob::rewindForwards(const AnimationIndex &newAnimationIndex)
{
    if (m_previousLoop > m_currentLoop) {
        // we need to fast rewind to the beginning
        for (QAbstractAnimationJob *anim = m_currentAnimation; anim; anim = anim->previousSibling()) {
            RETURN_IF_DELETED(setCurrentAnimation(anim, true));
            RETURN_IF_DELETED(anim->setCurrentTime(0));
        }
        // this will make sure the current animation is reset to the end
        if (lastChild() && !lastChild()->previousSibling()) {   //count == 1
            // we need to force activation because setCurrentAnimation will have no effect
            RETURN_IF_DELETED(activateCurrentAnimation());
        } else {
            RETURN_IF_DELETED(setCurrentAnimation(lastChild(), true));
        }
    }

    // and now we need to fast rewind from the current position to
    for (QAbstractAnimationJob *anim = m_currentAnimation; anim && anim != newAnimationIndex.animation; anim = anim->previousSibling()) {
        RETURN_IF_DELETED(setCurrentAnimation(anim, true));
        RETURN_IF_DELETED(anim->setCurrentTime(0));
    }
    // setting the new current animation will happen later
}

int QSequentialAnimationGroupJob::duration() const
{
    int ret = 0;

    for (QAbstractAnimationJob *anim = firstChild(); anim; anim = anim->nextSibling()) {
        const int currentDuration = anim->totalDuration();
        if (currentDuration == -1)
            return -1; // Undetermined length

        ret += currentDuration;
    }

    return ret;
}

void QSequentialAnimationGroupJob::updateCurrentTime(int currentTime)
{
    if (!m_currentAnimation)
        return;

    const QSequentialAnimationGroupJob::AnimationIndex newAnimationIndex = indexForCurrentTime();

    // newAnimationIndex.index is the new current animation
    if (m_previousLoop < m_currentLoop
        || (m_previousLoop == m_currentLoop && m_currentAnimation != newAnimationIndex.animation && newAnimationIndex.afterCurrent)) {
            // advancing with forward direction is the same as rewinding with backwards direction
            RETURN_IF_DELETED(advanceForwards(newAnimationIndex));

    } else if (m_previousLoop > m_currentLoop
        || (m_previousLoop == m_currentLoop && m_currentAnimation != newAnimationIndex.animation && !newAnimationIndex.afterCurrent)) {
            // rewinding with forward direction is the same as advancing with backwards direction
            RETURN_IF_DELETED(rewindForwards(newAnimationIndex));
    }

    RETURN_IF_DELETED(setCurrentAnimation(newAnimationIndex.animation));

    const int newCurrentTime = currentTime - newAnimationIndex.timeOffset;

    if (m_currentAnimation) {
        RETURN_IF_DELETED(m_currentAnimation->setCurrentTime(newCurrentTime));
        if (atEnd()) {
            //we make sure that we don't exceed the duration here
            m_currentTime += m_currentAnimation->currentTime() - newCurrentTime;
            RETURN_IF_DELETED(stop());
        }
    } else {
        //the only case where currentAnimation could be null
        //is when all animations have been removed
        Q_ASSERT(!firstChild());
        m_currentTime = 0;
        RETURN_IF_DELETED(stop());
    }

    m_previousLoop = m_currentLoop;
}

void QSequentialAnimationGroupJob::updateState(QAbstractAnimationJob::State newState,
                                            QAbstractAnimationJob::State oldState)
{
    QAnimationGroupJob::updateState(newState, oldState);

    if (!m_currentAnimation)
        return;

    switch (newState) {
    case Stopped:
        m_currentAnimation->stop();
        break;
    case Paused:
        if (oldState == m_currentAnimation->state() && oldState == Running)
            m_currentAnimation->pause();
        else
            restart();
        break;
    case Running:
        if (oldState == m_currentAnimation->state() && oldState == Paused)
            m_currentAnimation->start();
        else
            restart();
        break;
    }
}

void QSequentialAnimationGroupJob::updateDirection(QAbstractAnimationJob::Direction direction)
{
    // we need to update the direction of the current animation
    if (!isStopped() && m_currentAnimation)
        m_currentAnimation->setDirection(direction);
}

void QSequentialAnimationGroupJob::setCurrentAnimation(QAbstractAnimationJob *anim, bool intermediate)
{
    if (!anim) {
        Q_ASSERT(!firstChild());
        m_currentAnimation = 0;
        return;
    }

    if (anim == m_currentAnimation)
        return;

    // stop the old current animation
    if (m_currentAnimation)
        m_currentAnimation->stop();

    m_currentAnimation = anim;

    activateCurrentAnimation(intermediate);
}

void QSequentialAnimationGroupJob::activateCurrentAnimation(bool intermediate)
{
    if (!m_currentAnimation || isStopped())
        return;

    m_currentAnimation->stop();

    // we ensure the direction is consistent with the group's direction
    m_currentAnimation->setDirection(m_direction);

    // reset the finish time of the animation if it is uncontrolled
    if (m_currentAnimation->totalDuration() == -1)
        resetUncontrolledAnimationFinishTime(m_currentAnimation);

    RETURN_IF_DELETED(m_currentAnimation->start());
    if (!intermediate && isPaused())
        m_currentAnimation->pause();
}

void QSequentialAnimationGroupJob::uncontrolledAnimationFinished(QAbstractAnimationJob *animation)
{
    Q_UNUSED(animation);
    Q_ASSERT(animation == m_currentAnimation);

    setUncontrolledAnimationFinishTime(m_currentAnimation, m_currentAnimation->currentTime());

    int totalTime = currentTime();
    if (m_direction == Forward) {
        // set the current animation to be the next one
        if (m_currentAnimation->nextSibling())
            setCurrentAnimation(m_currentAnimation->nextSibling());

        for (QAbstractAnimationJob *a = animation->nextSibling(); a; a = a->nextSibling()) {
            int dur = a->duration();
            if (dur == -1) {
                totalTime = -1;
                break;
            } else {
                totalTime += dur;
            }
        }

    } else {
        // set the current animation to be the previous one
        if (m_currentAnimation->previousSibling())
            setCurrentAnimation(m_currentAnimation->previousSibling());

        for (QAbstractAnimationJob *a = animation->previousSibling(); a; a = a->previousSibling()) {
            int dur = a->duration();
            if (dur == -1) {
                totalTime = -1;
                break;
            } else {
                totalTime += dur;
            }
        }
    }
    if (totalTime >= 0)
        setUncontrolledAnimationFinishTime(this, totalTime);
    if (atEnd())
        stop();
}

void QSequentialAnimationGroupJob::animationInserted(QAbstractAnimationJob *anim)
{
    if (m_currentAnimation == 0)
        setCurrentAnimation(firstChild()); // initialize the current animation

    if (m_currentAnimation == anim->nextSibling()
        && m_currentAnimation->currentTime() == 0 && m_currentAnimation->currentLoop() == 0) {
            //in this case we simply insert the animation before the current one has actually started
            setCurrentAnimation(anim);
    }

//    TODO
//    if (index < m_currentAnimationIndex || m_currentLoop != 0) {
//        qWarning("QSequentialGroup::insertAnimation only supports to add animations after the current one.");
//        return; //we're not affected because it is added after the current one
//    }
}

void QSequentialAnimationGroupJob::animationRemoved(QAbstractAnimationJob *anim, QAbstractAnimationJob *prev, QAbstractAnimationJob *next)
{
    QAnimationGroupJob::animationRemoved(anim, prev, next);

    Q_ASSERT(m_currentAnimation); // currentAnimation should always be set

    bool removingCurrent = anim == m_currentAnimation;
    if (removingCurrent) {
        if (next)
            setCurrentAnimation(next); //let's try to take the next one
        else if (prev)
            setCurrentAnimation(prev);
        else// case all animations were removed
            setCurrentAnimation(0);
    }

    // duration of the previous animations up to the current animation
    m_currentTime = 0;
    for (QAbstractAnimationJob *job = firstChild(); job; job = job->nextSibling()) {
        if (job == m_currentAnimation)
            break;
        m_currentTime += animationActualTotalDuration(job);

    }

    if (!removingCurrent) {
        //the current animation is not the one being removed
        //so we add its current time to the current time of this group
        m_currentTime += m_currentAnimation->currentTime();
    }

    //let's also update the total current time
    m_totalCurrentTime = m_currentTime + m_loopCount * duration();
}

void QSequentialAnimationGroupJob::debugAnimation(QDebug d) const
{
    d << "SequentialAnimationGroupJob(" << hex << (const void *) this << dec << ")" << "currentAnimation:" << (void *)m_currentAnimation;

    debugChildren(d);
}

QT_END_NAMESPACE
