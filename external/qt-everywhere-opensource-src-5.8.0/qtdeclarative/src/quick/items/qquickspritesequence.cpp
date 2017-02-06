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

#include "qquickspritesequence_p.h"
#include "qquickspritesequence_p_p.h"
#include "qquicksprite_p.h"
#include "qquickspriteengine_p.h"
#include <QtQuick/private/qsgcontext_p.h>
#include <private/qsgadaptationlayer_p.h>
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
    \qmltype SpriteSequence
    \instantiates QQuickSpriteSequence
    \inqmlmodule QtQuick
    \ingroup qtquick-visual-utility
    \inherits Item
    \brief Draws a sprite animation

    SpriteSequence renders and controls a list of animations defined
    by \l Sprite types.

    For full details, see the \l{Sprite Animations} overview.
*/
/*!
    \qmlproperty bool QtQuick::SpriteSequence::running

    Whether the sprite is animating or not.

    Default is true
*/
/*!
    \qmlproperty bool QtQuick::SpriteSequence::interpolate

    If true, interpolation will occur between sprite frames to make the
    animation appear smoother.

    Default is true.
*/
/*!
    \qmlproperty string QtQuick::SpriteSequence::currentSprite

    The name of the Sprite which is currently animating.
*/
/*!
    \qmlproperty string QtQuick::SpriteSequence::goalSprite

    The name of the Sprite which the animation should move to.

    Sprite states have defined durations and transitions between them, setting goalState
    will cause it to disregard any path weightings (including 0) and head down the path
    which will reach the goalState quickest (fewest animations). It will pass through
    intermediate states on that path, and animate them for their duration.

    If it is possible to return to the goalState from the starting point of the goalState
    it will continue to do so until goalState is set to "" or an unreachable state.
*/
/*! \qmlmethod QtQuick::SpriteSequence::jumpTo(string sprite)

    This function causes the SpriteSequence to jump to the specified sprite immediately, intermediate
    sprites are not played. The \a sprite argument is the name of the sprite you wish to jump to.
*/
/*!
    \qmlproperty list<Sprite> QtQuick::SpriteSequence::sprites

    The sprite or sprites to draw. Sprites will be scaled to the size of this item.
*/

//TODO: Implicitly size element to size of first sprite?
QQuickSpriteSequence::QQuickSpriteSequence(QQuickItem *parent) :
    QQuickItem(*(new QQuickSpriteSequencePrivate), parent)
{
    setFlag(ItemHasContents);
    connect(this, SIGNAL(runningChanged(bool)),
            this, SLOT(update()));
}

void QQuickSpriteSequence::jumpTo(const QString &sprite)
{
    Q_D(QQuickSpriteSequence);
    if (!d->m_spriteEngine)
        return;
    d->m_spriteEngine->setGoal(d->m_spriteEngine->stateIndex(sprite), 0, true);
}

void QQuickSpriteSequence::setGoalSprite(const QString &sprite)
{
    Q_D(QQuickSpriteSequence);
    if (d->m_goalState != sprite){
        d->m_goalState = sprite;
        emit goalSpriteChanged(sprite);
        if (d->m_spriteEngine)
            d->m_spriteEngine->setGoal(d->m_spriteEngine->stateIndex(sprite));
    }
}

void QQuickSpriteSequence::setRunning(bool arg)
{
    Q_D(QQuickSpriteSequence);
    if (d->m_running != arg) {
        d->m_running = arg;
        Q_EMIT runningChanged(arg);
    }
}

void QQuickSpriteSequence::setInterpolate(bool arg)
{
    Q_D(QQuickSpriteSequence);
    if (d->m_interpolate != arg) {
        d->m_interpolate = arg;
        Q_EMIT interpolateChanged(arg);
    }
}

QQmlListProperty<QQuickSprite> QQuickSpriteSequence::sprites()
{
    Q_D(QQuickSpriteSequence);
    return QQmlListProperty<QQuickSprite>(this, &d->m_sprites, spriteAppend, spriteCount, spriteAt, spriteClear);
}

bool QQuickSpriteSequence::running() const
{
    Q_D(const QQuickSpriteSequence);
    return d->m_running;
}

bool QQuickSpriteSequence::interpolate() const
{
    Q_D(const QQuickSpriteSequence);
    return d->m_interpolate;
}

QString QQuickSpriteSequence::goalSprite() const
{
    Q_D(const QQuickSpriteSequence);
    return d->m_goalState;
}

QString QQuickSpriteSequence::currentSprite() const
{
    Q_D(const QQuickSpriteSequence);
    return d->m_curState;
}

void QQuickSpriteSequence::createEngine()
{
    Q_D(QQuickSpriteSequence);
    //TODO: delay until component complete
    if (d->m_spriteEngine)
        delete d->m_spriteEngine;
    if (d->m_sprites.count()) {
        d->m_spriteEngine = new QQuickSpriteEngine(d->m_sprites, this);
        if (!d->m_goalState.isEmpty())
            d->m_spriteEngine->setGoal(d->m_spriteEngine->stateIndex(d->m_goalState));
    } else {
        d->m_spriteEngine = 0;
    }
    reset();
}

QSGSpriteNode *QQuickSpriteSequence::initNode()
{
    Q_D(QQuickSpriteSequence);

    if (!d->m_spriteEngine) {
        qmlInfo(this) << "No sprite engine...";
        return nullptr;
    } else if (d->m_spriteEngine->status() == QQuickPixmap::Null) {
        d->m_spriteEngine->startAssemblingImage();
        update();//Schedule another update, where we will check again
        return nullptr;
    } else if (d->m_spriteEngine->status() == QQuickPixmap::Loading) {
        update();//Schedule another update, where we will check again
        return nullptr;
    }

    QImage image = d->m_spriteEngine->assembledImage(d->sceneGraphRenderContext()->maxTextureSize());
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

    d->m_curState = d->m_spriteEngine->state(d->m_spriteEngine->curState())->name();
    emit currentSpriteChanged(d->m_curState);
    d->m_timestamp.start();
    return node;
}

void QQuickSpriteSequence::reset()
{
    Q_D(QQuickSpriteSequence);
    d->m_pleaseReset = true;
}

QSGNode *QQuickSpriteSequence::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    Q_D(QQuickSpriteSequence);

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

    if (d->m_running) {
        update();
    }

    return node;
}

void QQuickSpriteSequence::prepareNextFrame(QSGSpriteNode *node)
{
    Q_D(QQuickSpriteSequence);

    uint timeInt = d->m_timestamp.elapsed();
    qreal time =  timeInt / 1000.;

    //Advance State
    d->m_spriteEngine->updateSprites(timeInt);
    if (d->m_curStateIdx != d->m_spriteEngine->curState()) {
        d->m_curStateIdx = d->m_spriteEngine->curState();
        d->m_curState = d->m_spriteEngine->state(d->m_spriteEngine->curState())->name();
        emit currentSpriteChanged(d->m_curState);
        d->m_curFrame= -1;
    }

    //Advance Sprite
    qreal animT = d->m_spriteEngine->spriteStart()/1000.0;
    qreal frameCount = d->m_spriteEngine->spriteFrames();
    qreal frameDuration = d->m_spriteEngine->spriteDuration()/frameCount;
    double frameAt;
    qreal progress;
    if (frameDuration > 0) {
        qreal frame = (time - animT)/(frameDuration / 1000.0);
        frame = qBound(qreal(0.0), frame, frameCount - qreal(1.0));//Stop at count-1 frames until we have between anim interpolation
        progress = std::modf(frame,&frameAt);
    } else {
        d->m_curFrame++;
        if (d->m_curFrame >= frameCount){
            d->m_curFrame = 0;
            d->m_spriteEngine->advance();
        }
        frameAt = d->m_curFrame;
        progress = 0;
    }
    if (d->m_spriteEngine->sprite()->reverse())
        frameAt = (d->m_spriteEngine->spriteFrames() - 1) - frameAt;
    int y = d->m_spriteEngine->spriteY();
    int w = d->m_spriteEngine->spriteWidth();
    int h = d->m_spriteEngine->spriteHeight();
    int x1 = d->m_spriteEngine->spriteX();
    x1 += frameAt * w;
    int x2 = x1;
    if (frameAt < (frameCount-1))
        x2 += w;

    node->setSourceA(QPoint(x1, y));
    node->setSourceB(QPoint(x2, y));
    node->setSpriteSize(QSize(w, h));
    node->setTime(d->m_interpolate ? progress : 0.0);
    node->setSize(QSizeF(width(), height()));
    node->setFiltering(smooth() ? QSGTexture::Linear : QSGTexture::Nearest);
    node->update();
}

QT_END_NAMESPACE
