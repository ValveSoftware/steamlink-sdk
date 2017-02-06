/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquicksprite_p.h"
#include <qqml.h>
#include <QDebug>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Sprite
    \instantiates QQuickSprite
    \inqmlmodule QtQuick
    \ingroup qtquick-visual-utility
    \brief Specifies sprite animations

    QQuickSprite renders sprites of one or more frames and animates them. The sprites
    can be in the middle of an image file, or split along multiple rows, as long as they form
    a contiguous line wrapping to the next row of the file from the left edge of the file.

    For full details, see the \l{Sprite Animations} overview.
*/
/*!
    \qmlproperty int QtQuick::Sprite::duration

    Duration of the animation. Values below 0 are invalid.

    If frameRate is valid then it will be used to calculate the duration of the frames.
    If not, and frameDuration is valid, then frameDuration will be used. Otherwise duration is used.
*/
/*!
    \qmlproperty int QtQuick::Sprite::durationVariation

    The duration of the animation can vary by up to this amount. Variation will never decrease the
    length of the animation to less than 0.

    durationVariation will only take effect if duration is
    used to calculate the duration of frames.

    Default is 0.
*/

/*!
    \qmlproperty qreal QtQuick::Sprite::frameRate

    Frames per second to show in the animation. Values below 0 are invalid.

    If frameRate is valid  then it will be used to calculate the duration of the frames.
    If not, and frameDuration is valid , then frameDuration will be used. Otherwise duration is used.
*/
/*!
    \qmlproperty qreal QtQuick::Sprite::frameRateVariation

    The frame rate between animations can vary by up to this amount. Variation will never decrease the
    length of the animation to less than 0.

    frameRateVariation will only take effect if frameRate is
    used to calculate the duration of frames.

    Default is 0.
*/

/*!
    \qmlproperty int QtQuick::Sprite::frameDuration

    Duration of each frame of the animation. Values below 0 are invalid.

    If frameRate is valid then it will be used to calculate the duration of the frames.
    If not, and frameDuration is valid, then frameDuration will be used. Otherwise duration is used.
*/
/*!
    \qmlproperty int QtQuick::Sprite::frameDurationVariation

    The duration of a frame in the animation can vary by up to this amount. Variation will never decrease the
    length of the animation to less than 0.

    frameDurationVariation will only take effect if frameDuration is
    used to calculate the duration of frames.

    Default is 0.
*/

/*!
    \qmlproperty string QtQuick::Sprite::name

    The name of this sprite, for use in the to property of other sprites.
*/
/*!
    \qmlproperty QVariantMap QtQuick::Sprite::to

    A list of other sprites and weighted transitions to them,
    for example {"a":1, "b":2, "c":0} would specify that one-third should
    transition to sprite "a" when this sprite is done, and two-thirds should
    transition to sprite "b" when this sprite is done. As the transitions are
    chosen randomly, these proportions will not be exact. With "c":0 in the list,
    no sprites will randomly transition to "c", but it wll be a valid path if a sprite
    goal is set.

    If no list is specified, or the sum of weights in the list is zero, then the sprite
    will repeat itself after completing.
*/
/*!
    \qmlproperty int QtQuick::Sprite::frameCount

    Number of frames in this sprite.
*/
/*!
    \qmlproperty int QtQuick::Sprite::frameHeight

    Height of a single frame in this sprite.
*/
/*!
    \qmlproperty int QtQuick::Sprite::frameWidth

    Width of a single frame in this sprite.
*/
/*!
    \qmlproperty int QtQuick::Sprite::frameX

    The X coordinate in the image file of the first frame of the sprite.
*/
/*!
    \qmlproperty int QtQuick::Sprite::frameY

    The Y coordinate in the image file of the first frame of the sprite.
*/
/*!
    \qmlproperty url QtQuick::Sprite::source

    The image source for the animation.

    If frameHeight and frameWidth are not specified, it is assumed to be a single long row of square frames.
    Otherwise, it can be multiple contiguous rows or rectangluar frames, when one row runs out the next will be used.

    If frameX and frameY are specified, the row of frames will be taken with that x/y coordinate as the upper left corner.
*/

/*!
    \qmlproperty bool QtQuick::Sprite::reverse

    If true, then the animation will be played in reverse.

    Default is false.
*/

/*!
    \qmlproperty bool QtQuick::Sprite::randomStart

    If true, then the animation will start its first animation with a random amount of its duration skipped.
    This allows them to not look like they all just started when the animation begins.

    This only affects the very first animation played. Transitioning to another animation, or the same
    animation again, will not trigger this.

    Default is false.
*/

/*!
    \qmlproperty bool QtQuick::Sprite::frameSync

    If true, then the animation will have no duration. Instead, the animation will advance
    one frame each time a frame is rendered to the screen. This synchronizes it with the painting
    rate as opposed to elapsed time.

    If frameSync is set to true, it overrides all of duration, frameRate and frameDuration.

    Default is false.
*/

static const int unsetDuration = -2;//-1 means perframe for duration
QQuickSprite::QQuickSprite(QObject *parent)
    : QQuickStochasticState(parent)
    , m_generatedCount(0)
    , m_framesPerRow(0)
    , m_rowY(0)
    , m_rowStartX(0)
    , m_reverse(false)
    , m_frameHeight(0)
    , m_frameWidth(0)
    , m_frames(1)
    , m_frameX(0)
    , m_frameY(0)
    , m_frameRate(unsetDuration)
    , m_frameRateVariation(0)
    , m_frameDuration(unsetDuration)
    , m_frameDurationVariation(0)
    , m_frameSync(false)
{
}

/*! \internal */
QQuickSprite::~QQuickSprite()
{
}

int QQuickSprite::variedDuration() const //Deals with precedence when multiple durations are set
{
    if (m_frameSync)
        return 0;

    if (m_frameRate != unsetDuration) {
        qreal fpms = (m_frameRate
                + (m_frameRateVariation * ((qreal)qrand()/RAND_MAX) * 2)
                - m_frameRateVariation) / 1000.0;
        return qMax(qreal(0.0) , m_frames / fpms);
    } else if (m_frameDuration != unsetDuration) {
        int mspf = m_frameDuration
                + (m_frameDurationVariation * ((qreal)qrand()/RAND_MAX) * 2)
                - m_frameDurationVariation;
        return qMax(0, m_frames * mspf);
    } else if (duration() >= 0) {
        qWarning() << "Sprite::duration is changing meaning to the full animation duration.";
        qWarning() << "Use Sprite::frameDuration for the old meaning, of per frame duration.";
        qWarning() << "As an interim measure, duration/durationVariation means the same as frameDuration/frameDurationVariation, and you'll get this warning spewed out everywhere to motivate you.";
    //Note that the spammyness is due to this being the best location to detect, but also called once each animation loop
        return QQuickStochasticState::variedDuration() * m_frames;
    }
    return 1000; //When nothing set
}

void QQuickSprite::startImageLoading()
{
    m_pix.clear(this);
    if (!m_source.isEmpty()) {
        QQmlEngine *e = qmlEngine(this);
        if (!e) { //If not created in QML, you must set the QObject parent to the QML element so this can work
            e = qmlEngine(parent());
            if (!e)
                qWarning() << "QQuickSprite: Cannot find QQmlEngine - this class is only for use in QML and may not work";
        }
        m_pix.load(e, m_source);
    }
}

QT_END_NAMESPACE
