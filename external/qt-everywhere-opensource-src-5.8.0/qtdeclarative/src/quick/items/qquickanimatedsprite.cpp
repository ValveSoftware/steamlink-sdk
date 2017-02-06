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

#include "qquickanimatedsprite_p.h"
#include "qquickanimatedsprite_p_p.h"
#include "qquicksprite_p.h"
#include "qquickspriteengine_p.h"
#include <QtQuick/private/qsgcontext_p.h>
#include <QtQuick/private/qquickitem_p.h>
#include <private/qsgadaptationlayer_p.h>
#include <private/qqmlglobal_p.h>
#include <QtQuick/qsgnode.h>
#include <QtQuick/qsgtexturematerial.h>
#include <QtQuick/qsgtexture.h>
#include <QtQuick/qquickwindow.h>
#include <QtQml/qqmlinfo.h>
#include <QFile>
#include <cmath>
#include <qmath.h>
#include <QDebug>

QT_BEGIN_NAMESPACE

/*!
    \qmltype AnimatedSprite
    \instantiates QQuickAnimatedSprite
    \inqmlmodule QtQuick
    \inherits Item
    \ingroup qtquick-visual
    \brief Draws a sprite animation

    AnimatedSprite provides rendering and control over animations which are provided
    as multiple frames in the same image file. You can play it at a fixed speed, at the
    frame rate of your display, or manually advance and control the progress.

    For details of how a sprite animation is defined see the \l{Sprite Animations} overview.
    Note that the AnimatedSprite type does not use Sprite types to define multiple animations,
    but instead encapsulates a single animation itself.
*/

/*!
    \qmlproperty bool QtQuick::AnimatedSprite::running

    Whether the sprite is animating or not.

    Default is true
*/

/*!
    \qmlproperty bool QtQuick::AnimatedSprite::interpolate

    If true, interpolation will occur between sprite frames to make the
    animation appear smoother.

    Default is true.
*/

/*!
    \qmlproperty qreal QtQuick::AnimatedSprite::frameRate

    Frames per second to show in the animation. Values equal to or below 0 are invalid.

    If frameRate is valid  then it will be used to calculate the duration of the frames.
    If not, and frameDuration is valid , then frameDuration will be used.

    Changing this parameter will restart the animation.
*/

/*!
    \qmlproperty int QtQuick::AnimatedSprite::frameDuration

    Duration of each frame of the animation. Values equal to or below 0 are invalid.

    If frameRate is valid then it will be used to calculate the duration of the frames.
    If not, and frameDuration is valid, then frameDuration will be used.

    Changing this parameter will restart the animation.
*/

/*!
    \qmlproperty int QtQuick::AnimatedSprite::frameCount

    Number of frames in this AnimatedSprite.
*/
/*!
    \qmlproperty int QtQuick::AnimatedSprite::frameHeight

    Height of a single frame in this AnimatedSprite.

    May be omitted if it is the only sprite in the file.
*/
/*!
    \qmlproperty int QtQuick::AnimatedSprite::frameWidth

    Width of a single frame in this AnimatedSprite.

    May be omitted if it is the only sprite in the file.
*/
/*!
    \qmlproperty int QtQuick::AnimatedSprite::frameX

    The X coordinate in the image file of the first frame of the AnimatedSprite.

    May be omitted if the first frame starts in the upper left corner of the file.
*/
/*!
    \qmlproperty int QtQuick::AnimatedSprite::frameY

    The Y coordinate in the image file of the first frame of the AnimatedSprite.

    May be omitted if the first frame starts in the upper left corner of the file.
*/
/*!
    \qmlproperty url QtQuick::AnimatedSprite::source

    The image source for the animation.

    If frameHeight and frameWidth are not specified, it is assumed to be a single long row of square frames.
    Otherwise, it can be multiple contiguous rows or rectangluar frames, when one row runs out the next will be used.

    If frameX and frameY are specified, the row of frames will be taken with that x/y coordinate as the upper left corner.
*/

/*!
    \qmlproperty bool QtQuick::AnimatedSprite::reverse

    If true, then the animation will be played in reverse.

    Default is false.
*/

/*!
    \qmlproperty bool QtQuick::AnimatedSprite::frameSync

    If true, then the animation will have no duration. Instead, the animation will advance
    one frame each time a frame is rendered to the screen. This synchronizes it with the painting
    rate as opposed to elapsed time.

    If frameSync is set to true, it overrides both frameRate and frameDuration.

    Default is false.

    Changing this parameter will restart the animation.
*/

/*!
    \qmlproperty int QtQuick::AnimatedSprite::loops

    After playing the animation this many times, the animation will automatically stop. Negative values are invalid.

    If this is set to AnimatedSprite.Infinite the animation will not stop playing on its own.

    Default is AnimatedSprite.Infinite
*/

/*!
    \qmlproperty bool QtQuick::AnimatedSprite::paused

    When paused, the current frame can be advanced manually.

    Default is false.
*/

/*!
    \qmlproperty int QtQuick::AnimatedSprite::currentFrame

    When paused, the current frame can be advanced manually by setting this property or calling advance().

*/

/*!
    \qmlmethod int QtQuick::AnimatedSprite::restart()

    Stops, then starts the sprite animation.
*/

//TODO: Implicitly size element to size of sprite
QQuickAnimatedSprite::QQuickAnimatedSprite(QQuickItem *parent) :
    QQuickItem(*(new QQuickAnimatedSpritePrivate), parent)
{
    Q_D(QQuickAnimatedSprite);
    d->m_sprite = new QQuickSprite(this);

    setFlag(ItemHasContents);
    connect(this, SIGNAL(widthChanged()),
            this, SLOT(reset()));
    connect(this, SIGNAL(heightChanged()),
            this, SLOT(reset()));
}

bool QQuickAnimatedSprite::running() const
{
    Q_D(const QQuickAnimatedSprite);
    return d->m_running;
}

bool QQuickAnimatedSprite::interpolate() const
{
    Q_D(const QQuickAnimatedSprite);
    return d->m_interpolate;
}

QUrl QQuickAnimatedSprite::source() const
{
    Q_D(const QQuickAnimatedSprite);
    return d->m_sprite->source();
}

bool QQuickAnimatedSprite::reverse() const
{
    Q_D(const QQuickAnimatedSprite);
    return d->m_sprite->reverse();
}

bool QQuickAnimatedSprite::frameSync() const
{
    Q_D(const QQuickAnimatedSprite);
    return d->m_sprite->frameSync();
}

int QQuickAnimatedSprite::frameCount() const
{
    Q_D(const QQuickAnimatedSprite);
    return d->m_sprite->frames();
}

int QQuickAnimatedSprite::frameHeight() const
{
    Q_D(const QQuickAnimatedSprite);
    return d->m_sprite->frameHeight();
}

int QQuickAnimatedSprite::frameWidth() const
{
    Q_D(const QQuickAnimatedSprite);
    return d->m_sprite->frameWidth();
}

int QQuickAnimatedSprite::frameX() const
{
    Q_D(const QQuickAnimatedSprite);
    return d->m_sprite->frameX();
}

int QQuickAnimatedSprite::frameY() const
{
    Q_D(const QQuickAnimatedSprite);
    return d->m_sprite->frameY();
}

qreal QQuickAnimatedSprite::frameRate() const
{
    Q_D(const QQuickAnimatedSprite);
    return d->m_sprite->frameRate();
}

int QQuickAnimatedSprite::frameDuration() const
{
    Q_D(const QQuickAnimatedSprite);
    return d->m_sprite->frameDuration();
}

int QQuickAnimatedSprite::loops() const
{
    Q_D(const QQuickAnimatedSprite);
    return d->m_loops;
}

bool QQuickAnimatedSprite::paused() const
{
    Q_D(const QQuickAnimatedSprite);
    return d->m_paused;
}

int QQuickAnimatedSprite::currentFrame() const
{
    Q_D(const QQuickAnimatedSprite);
    return d->m_curFrame;
}

bool QQuickAnimatedSprite::isCurrentFrameChangedConnected()
{
    IS_SIGNAL_CONNECTED(this, QQuickAnimatedSprite, currentFrameChanged, (int));
}

void QQuickAnimatedSprite::reloadImage()
{
    if (!isComponentComplete())
        return;
    createEngine();//### It's not as inefficient as it sounds, but it still sucks having to recreate the engine
}

void QQuickAnimatedSprite::componentComplete()
{
    Q_D(const QQuickAnimatedSprite);
    createEngine();
    QQuickItem::componentComplete();
    if (d->m_running)
        start();
}

void QQuickAnimatedSprite::start()
{
    Q_D(QQuickAnimatedSprite);
    d->m_running = true;
    if (!isComponentComplete())
        return;
    d->m_curLoop = 0;
    d->m_timestamp.start();
    if (d->m_spriteEngine) {
        d->m_spriteEngine->stop(0);
        d->m_spriteEngine->updateSprites(0);
        d->m_spriteEngine->start(0);
    }
    emit currentFrameChanged(0);
    emit runningChanged(true);
    maybeUpdate();
}

void QQuickAnimatedSprite::stop()
{
    Q_D(QQuickAnimatedSprite);
    d->m_running = false;
    if (!isComponentComplete())
        return;
    d->m_pauseOffset = 0;
    emit runningChanged(false);
    maybeUpdate();
}

/*!
    \qmlmethod int QtQuick::AnimatedSprite::advance()

    Advances the sprite animation by one frame.
*/
void QQuickAnimatedSprite::advance(int frames)
{
    Q_D(QQuickAnimatedSprite);
    if (!frames)
        return;
    //TODO-C: May not work when running - only when paused
    d->m_curFrame += frames;
    while (d->m_curFrame < 0)
        d->m_curFrame += d->m_spriteEngine->maxFrames();
    d->m_curFrame = d->m_curFrame % d->m_spriteEngine->maxFrames();
    emit currentFrameChanged(d->m_curFrame);
    maybeUpdate();
}

void QQuickAnimatedSprite::maybeUpdate()
{
    QQuickItemPrivate *priv = QQuickItemPrivate::get(this);
    const QLazilyAllocated<QQuickItemPrivate::ExtraData> &extraData = priv->extra;
    if ((extraData.isAllocated() && extraData->effectRefCount > 0) || priv->effectiveVisible)
        update();
}

/*!
    \qmlmethod int QtQuick::AnimatedSprite::pause()

    Pauses the sprite animation. This does nothing if
    \l paused is true.

    \sa resume()
*/
void QQuickAnimatedSprite::pause()
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_paused)
        return;
    d->m_pauseOffset = d->m_timestamp.elapsed();
    d->m_paused = true;
    emit pausedChanged(true);
    maybeUpdate();
}

/*!
    \qmlmethod int QtQuick::AnimatedSprite::resume()

    Resumes the sprite animation if \l paused is true;
    otherwise, this does nothing.

    \sa pause()
*/
void QQuickAnimatedSprite::resume()
{
    Q_D(QQuickAnimatedSprite);

    if (!d->m_paused)
        return;
    d->m_pauseOffset = d->m_pauseOffset - d->m_timestamp.elapsed();
    d->m_paused = false;
    emit pausedChanged(false);
    maybeUpdate();
}

void QQuickAnimatedSprite::setRunning(bool arg)
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_running != arg) {
        if (d->m_running)
            stop();
        else
            start();
    }
}

void QQuickAnimatedSprite::setPaused(bool arg)
{
    Q_D(const QQuickAnimatedSprite);

    if (d->m_paused != arg) {
        if (d->m_paused)
            resume();
        else
            pause();
    }
}

void QQuickAnimatedSprite::setInterpolate(bool arg)
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_interpolate != arg) {
        d->m_interpolate = arg;
        Q_EMIT interpolateChanged(arg);
    }
}

void QQuickAnimatedSprite::setSource(QUrl arg)
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_sprite->m_source != arg) {
        d->m_sprite->setSource(arg);
        Q_EMIT sourceChanged(arg);
        reloadImage();
    }
}

void QQuickAnimatedSprite::setReverse(bool arg)
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_sprite->m_reverse != arg) {
        d->m_sprite->setReverse(arg);
        Q_EMIT reverseChanged(arg);
    }
}

void QQuickAnimatedSprite::setFrameSync(bool arg)
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_sprite->m_frameSync != arg) {
        d->m_sprite->setFrameSync(arg);
        Q_EMIT frameSyncChanged(arg);
        if (d->m_running)
            restart();
    }
}

void QQuickAnimatedSprite::setFrameCount(int arg)
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_sprite->m_frames != arg) {
        d->m_sprite->setFrameCount(arg);
        Q_EMIT frameCountChanged(arg);
        reloadImage();
    }
}

void QQuickAnimatedSprite::setFrameHeight(int arg)
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_sprite->m_frameHeight != arg) {
        d->m_sprite->setFrameHeight(arg);
        Q_EMIT frameHeightChanged(arg);
        reloadImage();
    }
}

void QQuickAnimatedSprite::setFrameWidth(int arg)
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_sprite->m_frameWidth != arg) {
        d->m_sprite->setFrameWidth(arg);
        Q_EMIT frameWidthChanged(arg);
        reloadImage();
    }
}

void QQuickAnimatedSprite::setFrameX(int arg)
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_sprite->m_frameX != arg) {
        d->m_sprite->setFrameX(arg);
        Q_EMIT frameXChanged(arg);
        reloadImage();
    }
}

void QQuickAnimatedSprite::setFrameY(int arg)
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_sprite->m_frameY != arg) {
        d->m_sprite->setFrameY(arg);
        Q_EMIT frameYChanged(arg);
        reloadImage();
    }
}

void QQuickAnimatedSprite::setFrameRate(qreal arg)
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_sprite->m_frameRate != arg) {
        d->m_sprite->setFrameRate(arg);
        Q_EMIT frameRateChanged(arg);
        if (d->m_running)
            restart();
    }
}

void QQuickAnimatedSprite::setFrameDuration(int arg)
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_sprite->m_frameDuration != arg) {
        d->m_sprite->setFrameDuration(arg);
        Q_EMIT frameDurationChanged(arg);
        if (d->m_running)
            restart();
    }
}

void QQuickAnimatedSprite::resetFrameRate()
{
    setFrameRate(-1.0);
}

void QQuickAnimatedSprite::resetFrameDuration()
{
    setFrameDuration(-1);
}

void QQuickAnimatedSprite::setLoops(int arg)
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_loops != arg) {
        d->m_loops = arg;
        Q_EMIT loopsChanged(arg);
    }
}

void QQuickAnimatedSprite::setCurrentFrame(int arg) //TODO-C: Probably only works when paused
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_curFrame != arg) {
        d->m_curFrame = arg;
        Q_EMIT currentFrameChanged(arg); //TODO-C Only emitted on manual advance!
        update();
    }
}

void QQuickAnimatedSprite::createEngine()
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_spriteEngine)
        delete d->m_spriteEngine;
    QList<QQuickSprite*> spriteList;
    spriteList << d->m_sprite;
    d->m_spriteEngine = new QQuickSpriteEngine(QList<QQuickSprite*>(spriteList), this);
    d->m_spriteEngine->startAssemblingImage();
    reset();
}

QSGSpriteNode* QQuickAnimatedSprite::initNode()
{
    Q_D(QQuickAnimatedSprite);

    if (!d->m_spriteEngine) {
        qmlInfo(this) << "No sprite engine...";
        return nullptr;
    } else if (d->m_spriteEngine->status() == QQuickPixmap::Null) {
        d->m_spriteEngine->startAssemblingImage();
        maybeUpdate();//Schedule another update, where we will check again
        return nullptr;
    } else if (d->m_spriteEngine->status() == QQuickPixmap::Loading) {
        maybeUpdate();//Schedule another update, where we will check again
        return nullptr;
    }

    QImage image = d->m_spriteEngine->assembledImage(d->sceneGraphRenderContext()->maxTextureSize()); //Engine prints errors if there are any
    if (image.isNull())
        return nullptr;

    QSGSpriteNode *node = d->sceneGraphContext()->createSpriteNode();

    d->m_sheetSize = QSize(image.size());
    node->setTexture(window()->createTextureFromImage(image));
    d->m_spriteEngine->start(0);
    node->setTime(0.0f);
    node->setSourceA(QPoint(d->m_spriteEngine->spriteX(), d->m_spriteEngine->spriteY()));
    node->setSourceB(QPoint(d->m_spriteEngine->spriteX(), d->m_spriteEngine->spriteY()));
    node->setSpriteSize(QSize(d->m_spriteEngine->spriteWidth(), d->m_spriteEngine->spriteHeight()));
    node->setSheetSize(d->m_sheetSize);
    node->setSize(QSizeF(width(), height()));
    return node;
}

void QQuickAnimatedSprite::reset()
{
    Q_D(QQuickAnimatedSprite);
    d->m_pleaseReset = true;
    maybeUpdate();
}

QSGNode *QQuickAnimatedSprite::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    Q_D(QQuickAnimatedSprite);

    if (d->m_pleaseReset) {
        delete oldNode;

        oldNode = nullptr;
        d->m_pleaseReset = false;
    }

    QSGSpriteNode *node = static_cast<QSGSpriteNode *>(oldNode);
    if (!node)
        node = initNode();

    if (node)
        prepareNextFrame(node);

    if (d->m_running && !d->m_paused)
        maybeUpdate();

    return node;
}

void QQuickAnimatedSprite::prepareNextFrame(QSGSpriteNode *node)
{
    Q_D(QQuickAnimatedSprite);

    int timeInt = d->m_timestamp.elapsed() + d->m_pauseOffset;
    qreal time =  timeInt / 1000.;

    int frameAt;
    qreal progress = 0.0;
    int lastFrame = d->m_curFrame;
    if (d->m_running && !d->m_paused) {
        const int nColumns = d->m_sheetSize.width() / d->m_spriteEngine->spriteWidth();
        //Advance State (keeps time for psuedostates)
        d->m_spriteEngine->updateSprites(timeInt);

        //Advance AnimatedSprite
        qreal animT = d->m_spriteEngine->spriteStart()/1000.0;
        const int frameCountInRow = d->m_spriteEngine->spriteFrames();
        const qreal frameDuration = d->m_spriteEngine->spriteDuration() / frameCountInRow;
        if (frameDuration > 0) {
            qreal frame = (time - animT)/(frameDuration / 1000.0);
            bool lastLoop = d->m_loops > 0 && d->m_curLoop == d->m_loops-1;
            //don't visually interpolate for the last frame of the last loop
            const int max = lastLoop ? frameCountInRow - 1 : frameCountInRow;
            frame = qBound(qreal(0.0), frame, qreal(max));
            double intpart;
            progress = std::modf(frame,&intpart);
            frameAt = (int)intpart;
            const int rowIndex = d->m_spriteEngine->spriteY()/frameHeight();
            const int newFrame = rowIndex * nColumns + frameAt;
            if (d->m_curFrame > newFrame) //went around
                d->m_curLoop++;
            d->m_curFrame = newFrame;
        } else {
            d->m_curFrame++;
            if (d->m_curFrame >= d->m_spriteEngine->maxFrames()) {    // maxFrames: total number of frames including all rows
                d->m_curFrame = 0;
                d->m_curLoop++;
            }
            frameAt = d->m_curFrame % nColumns;
            if (frameAt == 0)
                d->m_spriteEngine->advance();
            progress = 0;
        }
        if (d->m_loops > 0 && d->m_curLoop >= d->m_loops) {
            frameAt = 0;
            d->m_running = false;
            emit runningChanged(false);
            maybeUpdate();
        }
    } else {
        frameAt = d->m_curFrame;
    }
    if (d->m_curFrame != lastFrame) {
        if (isCurrentFrameChangedConnected())
            emit currentFrameChanged(d->m_curFrame);
        maybeUpdate();
    }

    qreal frameCount = d->m_spriteEngine->spriteFrames();
    bool reverse = d->m_spriteEngine->sprite()->reverse();
    if (reverse)
        frameAt = (frameCount - 1) - frameAt;

    int w = d->m_spriteEngine->spriteWidth();
    int h = d->m_spriteEngine->spriteHeight();
    int x1;
    int y1;
    if (d->m_paused) {
        int spriteY = d->m_spriteEngine->spriteY();
        if (reverse) {
            int rows = d->m_spriteEngine->maxFrames() * d->m_spriteEngine->spriteWidth() / d->m_sheetSize.width();
            spriteY -= rows * d->m_spriteEngine->spriteHeight();
            frameAt = (frameCount - 1) - frameAt;
        }

        int position = frameAt * d->m_spriteEngine->spriteWidth() + d->m_spriteEngine->spriteX();
        int row = position / d->m_sheetSize.width();

        x1 = (position - (row * d->m_sheetSize.width()));
        y1 = (row * d->m_spriteEngine->spriteHeight() + spriteY);
    } else {
        x1 = d->m_spriteEngine->spriteX() + frameAt * w;
        y1 = d->m_spriteEngine->spriteY();
    }

    //### hard-coded 0/1 work because we are the only
    // images in the sprite sheet (without this we cannot assume
    // where in the sheet we begin/end).
    int x2;
    int y2;
    if (reverse) {
        if (frameAt > 0) {
            x2 = x1 - w;
            y2 = y1;
        } else {
            x2 = 1.0 - w;
            y2 = y1 - h;
            if (y2 < 0.0) {
                //the last row may not fill the entire width
                int maxRowFrames = d->m_sheetSize.width() / d->m_spriteEngine->spriteWidth();
                if (d->m_spriteEngine->maxFrames() % maxRowFrames)
                    x2 = ((d->m_spriteEngine->maxFrames() % maxRowFrames) - 1) * w;

                y2 = 1.0 - h;
            }
        }
    } else {
        if (frameAt < (frameCount-1)) {
            x2 = x1 + w;
            y2 = y1;
        } else {
            x2 = 0.0;
            y2 = y1 + h;
            if (y2 >= 1.0)
                y2 = 0.0;
        }
    }

    node->setSourceA(QPoint(x1, y1));
    node->setSourceB(QPoint(x2, y2));
    node->setSpriteSize(QSize(w, h));
    node->setTime(d->m_interpolate ? progress : 0.0);
    node->setSize(QSizeF(width(), height()));
    node->setFiltering(smooth() ? QSGTexture::Linear : QSGTexture::Nearest);
    node->update();
}

QT_END_NAMESPACE
