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

#include "qquickspriteengine_p.h"
#include "qquicksprite_p.h"
#include <qqmlinfo.h>
#include <qqml.h>
#include <QDebug>
#include <QPainter>
#include <QSet>
#include <QtGui/qopengl.h>
#include <QOpenGLFunctions>

QT_BEGIN_NAMESPACE

/*
    \internal Stochastic/Sprite engine implementation docs

    Nomenclature: 'thing' refers to an instance of a running sprite or state. It could be renamed.
    States and Transitions are referred to in the state machine sense here, NOT in the QML sense.

    The Stochastic State engine takes states with stochastic state transitions defined and transitions them.
    When a state is started, it's added to a list of pending updates sorted by their time they want to update.
    An external driver calls the update function with an elapsed time, which becomes the new time offset.
    The pending update stack is popped until all entries are past the current time, which simulates all intervening time.

    The Sprite Engine subclass has two major differences. Firstly all states are sprites (and there's a new vector with them
    cast to sprite). Secondly, it chops up images and states to fit a texture friendly format.
    Before the Sprite Engine starts running, its user requests a texture assembled from all the sprite images. This
    texture is made by pasting the sprites into one image, with one sprite animation per row (in the future it is planned to have
    arbitrary X/Y start ends, but they will still be assembled and recorded here and still have to be contiguous lines).
    This cut-up allows the users to calcuate frame positions with a texture percentage width and elapsed time.
    It also means that large sprites cover multiple lines to fit inside the texture memory limit (which is a square).

    Large sprites covering multiple lines breaks this simple interface for the users, so each line is treated as a pseudostate
    and it's mostly hidden from the spriteengine users (except that they'll get advanced signals where the state is the same
    but the visual parameters changed). These are not real states because that would get very complex with bindings. Instead,
    when sprite attributes are requested from a sprite that has multiple pseudostates, it returns the values for the psuedostate
    it is in. State advancement is intercepted and hollow for pseudostates, except the last one. The last one transitions as the
    state normally does.
*/

static const int NINF = -1000000;//magic number for random start time - should be more negative than a single realistic animation duration
//#define SPRITE_IMAGE_DEBUG
#ifdef SPRITE_IMAGE_DEBUG
#include <QFile>
#include <QDir>
#endif
/* TODO:
   make sharable?
   solve the state data initialization/transfer issue so as to not need to make friends
*/

QQuickStochasticEngine::QQuickStochasticEngine(QObject *parent) :
    QObject(parent), m_timeOffset(0), m_addAdvance(false)
{
    //Default size 1
    setCount(1);
}

QQuickStochasticEngine::QQuickStochasticEngine(const QList<QQuickStochasticState *> &states, QObject *parent) :
    QObject(parent), m_states(states), m_timeOffset(0), m_addAdvance(false)
{
    //Default size 1
    setCount(1);
}

QQuickStochasticEngine::~QQuickStochasticEngine()
{
}

QQuickSpriteEngine::QQuickSpriteEngine(QObject *parent)
    : QQuickStochasticEngine(parent), m_startedImageAssembly(false), m_loaded(false)
{
}

QQuickSpriteEngine::QQuickSpriteEngine(const QList<QQuickSprite *> &sprites, QObject *parent)
    : QQuickSpriteEngine(parent)
{
    for (QQuickSprite* sprite : sprites)
        m_states << (QQuickStochasticState*)sprite;
}

QQuickSpriteEngine::~QQuickSpriteEngine()
{
}


int QQuickSpriteEngine::maxFrames() const
{
    return m_maxFrames;
}

/* States too large to fit in one row are split into multiple rows
   This is more efficient for the implementation, but should remain an implementation detail (invisible from QML)
   Therefore the below functions abstract sprite from the viewpoint of classes that pass the details onto shaders
   But States maintain their listed index for internal structures
TODO: All these calculations should be pre-calculated and cached during initialization for a significant performance boost
TODO: Above idea needs to have the varying duration offset added to it
*/
//TODO: Should these be adding advanceTime as well? But only if advanceTime was added to your startTime...
/*
    To get these working with duration=-1, m_startTimes will be messed with should duration=-1
    m_startTimes will be set in advance/restart to 0->(m_framesPerRow-1) and can be used directly as extra.
    This makes it 'frame' instead, but is more memory efficient than two arrays and less hideous than a vector of unions.
*/
int QQuickSpriteEngine::pseudospriteProgress(int sprite, int state, int* rowDuration) const
{
    int myRowDuration = m_duration[sprite] * m_sprites[state]->m_framesPerRow / m_sprites[state]->m_frames;
    if (rowDuration)
        *rowDuration = myRowDuration;

    if (m_sprites[state]->reverse()) //shift start-time back by the amount of time the first frame is smaller than rowDuration
        return (m_timeOffset - (m_startTimes[sprite] - (myRowDuration - (m_duration[sprite] % myRowDuration))) )
                    / myRowDuration;
    else
        return (m_timeOffset - m_startTimes[sprite]) / myRowDuration;
}

int QQuickSpriteEngine::spriteState(int sprite) const
{
    if (!m_loaded)
        return 0;
    int state = m_things[sprite];
    if (!m_sprites[state]->m_generatedCount)
        return state;

    int extra;
    if (m_sprites[state]->frameSync())
        extra = m_startTimes[sprite];
    else if (!m_duration[sprite])
        return state;
    else
        extra = pseudospriteProgress(sprite, state);
    if (m_sprites[state]->reverse())
        extra = (m_sprites[state]->m_generatedCount - 1) - extra;

    return state + extra;
}

int QQuickSpriteEngine::spriteStart(int sprite) const
{
    if (!m_duration[sprite] || !m_loaded)
        return m_timeOffset;
    int state = m_things[sprite];
    if (!m_sprites[state]->m_generatedCount)
        return m_startTimes[sprite];
    int rowDuration;
    int extra = pseudospriteProgress(sprite, state, &rowDuration);
    if (m_sprites[state]->reverse())
        return m_startTimes[sprite] + (extra ? (extra - 1)*rowDuration + (m_duration[sprite] % rowDuration) : 0);
    return m_startTimes[sprite] + extra*rowDuration;
}

int QQuickSpriteEngine::spriteFrames(int sprite) const
{
    if (!m_loaded)
        return 1;
    int state = m_things[sprite];
    if (!m_sprites[state]->m_generatedCount)
        return m_sprites[state]->frames();

    int extra;
    if (m_sprites[state]->frameSync())
        extra = m_startTimes[sprite];
    else if (!m_duration[sprite])
        return m_sprites[state]->frames();
    else
        extra = pseudospriteProgress(sprite, state);
    if (m_sprites[state]->reverse())
        extra = (m_sprites[state]->m_generatedCount - 1) - extra;


    if (extra == m_sprites[state]->m_generatedCount - 1) {//last state
        const int framesRemaining = m_sprites[state]->frames() % m_sprites[state]->m_framesPerRow;
        if (framesRemaining > 0)
            return framesRemaining;
    }
    return m_sprites[state]->m_framesPerRow;
}

int QQuickSpriteEngine::spriteDuration(int sprite) const //Full duration, not per frame
{
    if (!m_duration[sprite] || !m_loaded)
        return m_duration[sprite];
    int state = m_things[sprite];
    if (!m_sprites[state]->m_generatedCount)
        return m_duration[sprite];
    int rowDuration;
    int extra = pseudospriteProgress(sprite, state, &rowDuration);
    if (m_sprites[state]->reverse())
        extra = (m_sprites[state]->m_generatedCount - 1) - extra;

    if (extra == m_sprites[state]->m_generatedCount - 1) {//last state
        const int durationRemaining = m_duration[sprite] % rowDuration;
        if (durationRemaining > 0)
            return durationRemaining;
    }
    return rowDuration;
}

int QQuickSpriteEngine::spriteY(int sprite) const
{
    if (!m_loaded)
        return 0;
    int state = m_things[sprite];
    if (!m_sprites[state]->m_generatedCount)
        return m_sprites[state]->m_rowY;

    int extra;
    if (m_sprites[state]->frameSync())
        extra = m_startTimes[sprite];
    else if (!m_duration[sprite])
        return m_sprites[state]->m_rowY;
    else
        extra = pseudospriteProgress(sprite, state);
    if (m_sprites[state]->reverse())
        extra = (m_sprites[state]->m_generatedCount - 1) - extra;


    return m_sprites[state]->m_rowY + m_sprites[state]->m_frameHeight * extra;
}

int QQuickSpriteEngine::spriteX(int sprite) const
{
    if (!m_loaded)
        return 0;
    int state = m_things[sprite];
    if (!m_sprites[state]->m_generatedCount)
        return m_sprites[state]->m_rowStartX;

    int extra;
    if (m_sprites[state]->frameSync())
        extra = m_startTimes[sprite];
    else if (!m_duration[sprite])
        return m_sprites[state]->m_rowStartX;
    else
        extra = pseudospriteProgress(sprite, state);
    if (m_sprites[state]->reverse())
        extra = (m_sprites[state]->m_generatedCount - 1) - extra;

    if (extra)
        return 0;
    return m_sprites[state]->m_rowStartX;
}

QQuickSprite* QQuickSpriteEngine::sprite(int sprite) const
{
    return m_sprites[m_things[sprite]];
}

int QQuickSpriteEngine::spriteWidth(int sprite) const
{
    int state = m_things[sprite];
    return m_sprites[state]->m_frameWidth;
}

int QQuickSpriteEngine::spriteHeight(int sprite) const
{
    int state = m_things[sprite];
    return m_sprites[state]->m_frameHeight;
}

int QQuickSpriteEngine::spriteCount() const //TODO: Actually image state count, need to rename these things to make sense together
{
    return m_imageStateCount;
}

void QQuickStochasticEngine::setGoal(int state, int sprite, bool jump)
{
    if (sprite >= m_things.count() || state >= m_states.count()
            || sprite < 0 || state < 0)
        return;
    if (!jump){
        m_goals[sprite] = state;
        return;
    }

    if (m_things.at(sprite) == state)
        return;//Already there
    m_things[sprite] = state;
    m_duration[sprite] = m_states.at(state)->variedDuration();
    m_goals[sprite] = -1;
    restart(sprite);
    emit stateChanged(sprite);
    emit m_states.at(state)->entered();
    return;
}

QQuickPixmap::Status QQuickSpriteEngine::status()//Composed status of all Sprites
{
    if (!m_startedImageAssembly)
        return QQuickPixmap::Null;
    int null, loading, ready;
    null = loading = ready = 0;
    for (QQuickSprite* s : qAsConst(m_sprites)) {
        switch (s->m_pix.status()) {
            // ### Maybe add an error message here, because this null shouldn't be reached but when it does, the image fails without an error message.
            case QQuickPixmap::Null : null++; break;
            case QQuickPixmap::Loading : loading++; break;
            case QQuickPixmap::Error : return QQuickPixmap::Error;
            case QQuickPixmap::Ready : ready++; break;
        }
    }
    if (null)
        return QQuickPixmap::Null;
    if (loading)
        return QQuickPixmap::Loading;
    if (ready)
        return QQuickPixmap::Ready;
    return QQuickPixmap::Null;
}

void QQuickSpriteEngine::startAssemblingImage()
{
    if (m_startedImageAssembly)
        return;
    m_loaded = false;
    m_errorsPrinted = false;

    //This could also trigger the start of the image loading in Sprites, however that currently happens in Sprite::setSource

    QList<QQuickStochasticState*> removals;

    for (QQuickStochasticState* s : qAsConst(m_states)) {
        QQuickSprite* sprite = qobject_cast<QQuickSprite*>(s);
        if (sprite) {
            m_sprites << sprite;
        } else {
            removals << s;
            qDebug() << "Error: Non-sprite in QQuickSpriteEngine";
        }
    }
    for (QQuickStochasticState* s : qAsConst(removals))
        m_states.removeAll(s);
    m_startedImageAssembly = true;
}

QImage QQuickSpriteEngine::assembledImage(int maxSize)
{
    QQuickPixmap::Status stat = status();
    if (!m_errorsPrinted && stat == QQuickPixmap::Error) {
        for (QQuickSprite* s : qAsConst(m_sprites))
            if (s->m_pix.isError())
                qmlInfo(s) << s->m_pix.error();
        m_errorsPrinted = true;
    }

    if (stat != QQuickPixmap::Ready)
        return QImage();

    int h = 0;
    int w = 0;
    m_maxFrames = 0;
    m_imageStateCount = 0;

    for (QQuickSprite* state : qAsConst(m_sprites)) {
        if (state->frames() > m_maxFrames)
            m_maxFrames = state->frames();

        QImage img = state->m_pix.image();

        {
            const QSize frameSize(state->m_frameWidth, state->m_frameHeight);
            if (!(img.size() - frameSize).isValid()) {
                qmlInfo(state).nospace() << "SpriteEngine: Invalid frame size " << frameSize << "."
                                            " It's bigger than image size " << img.size() << ".";
                return QImage();
            }
        }

        //Check that the frame sizes are the same within one sprite
        if (!state->m_frameWidth)
            state->m_frameWidth = img.width() / state->frames();

        if (!state->m_frameHeight)
            state->m_frameHeight = img.height();

        if (state->frames() * state->frameWidth() > maxSize){
            struct helper{
                static int divRoundUp(int a, int b){return (a+b-1)/b;}
            };
            int rowsNeeded = helper::divRoundUp(state->frames(), (maxSize / state->frameWidth()));
            if (h + rowsNeeded * state->frameHeight() > maxSize){
                if (rowsNeeded * state->frameHeight() > maxSize)
                    qmlInfo(state) << "SpriteEngine: Animation too large to fit in one texture:" << state->source().toLocalFile();
                else
                    qmlInfo(state) << "SpriteEngine: Animations too large to fit in one texture, pushed over the edge by:" << state->source().toLocalFile();
                qmlInfo(state) << "SpriteEngine: Your texture max size today is " << maxSize;
                return QImage();
            }
            state->m_generatedCount = rowsNeeded;
            h += state->frameHeight() * rowsNeeded;
            w = qMax(w, ((int)(maxSize / state->frameWidth())) * state->frameWidth());
            m_imageStateCount += rowsNeeded;
        }else{
            h += state->frameHeight();
            w = qMax(w, state->frameWidth() * state->frames());
            m_imageStateCount++;
        }
    }

    //maxFrames is max number in a line of the texture
    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    image.fill(0);
    QPainter p(&image);
    int y = 0;
    for (QQuickSprite* state : qAsConst(m_sprites)) {
        QImage img(state->m_pix.image());
        int frameWidth = state->m_frameWidth;
        int frameHeight = state->m_frameHeight;
        if (img.height() == frameHeight && img.width() <  maxSize){//Simple case
            p.drawImage(0,y,img.copy(state->m_frameX,0,state->m_frames * frameWidth, frameHeight));
            state->m_rowStartX = 0;
            state->m_rowY = y;
            y += frameHeight;
        }else{//Chopping up image case
            state->m_framesPerRow = image.width()/frameWidth;
            state->m_rowY = y;
            int x = 0;
            int curX = state->m_frameX;
            int curY = state->m_frameY;
            int framesLeft = state->frames();
            while (framesLeft > 0){
                if (image.width() - x + curX <= img.width()){//finish a row in image (dest)
                    int copied = image.width() - x;
                    framesLeft -= copied/frameWidth;
                    p.drawImage(x,y,img.copy(curX,curY,copied,frameHeight));
                    y += frameHeight;
                    curX += copied;
                    x = 0;
                    if (curX == img.width()){
                        curX = 0;
                        curY += frameHeight;
                    }
                }else{//finish a row in img (src)
                    int copied = img.width() - curX;
                    framesLeft -= copied/frameWidth;
                    p.drawImage(x,y,img.copy(curX,curY,copied,frameHeight));
                    curY += frameHeight;
                    x += copied;
                    curX = 0;
                }
            }
            if (x)
                y += frameHeight;
        }
    }

    if (image.height() > maxSize){
        qWarning() << "SpriteEngine: Too many animations to fit in one texture...";
        qWarning() << "SpriteEngine: Your texture max size today is " << maxSize;
        return QImage();
    }

#ifdef SPRITE_IMAGE_DEBUG
    QString fPath = QDir::tempPath() + "/SpriteImage.%1.png";
    int acc = 0;
    while (QFile::exists(fPath.arg(acc))) acc++;
    image.save(fPath.arg(acc), "PNG");
    qDebug() << "Assembled image output to: " << fPath.arg(acc);
#endif

    m_loaded = true;
    m_startedImageAssembly = false;
    return image;
}

//TODO: Add a reset() function, for completeness in the case of a SpriteEngine being restarted from 0
void QQuickStochasticEngine::setCount(int c)
{
    m_things.resize(c);
    m_goals.resize(c);
    m_duration.resize(c);
    m_startTimes.resize(c);
}

void QQuickStochasticEngine::start(int index, int state)
{
    if (index >= m_things.count())
        return;
    m_things[index] = state;
    m_duration[index] = m_states.at(state)->variedDuration();
    if (m_states.at(state)->randomStart())
        m_startTimes[index] = NINF;
    else
        m_startTimes[index] = 0;
    m_goals[index] = -1;
    m_addAdvance = false;
    restart(index);
    m_addAdvance = true;
}

void QQuickStochasticEngine::stop(int index)
{
    if (index >= m_things.count())
        return;
    //Will never change until start is called again with a new state (or manually advanced) - this is not a 'pause'
    for (int i=0; i<m_stateUpdates.count(); i++)
        m_stateUpdates[i].second.removeAll(index);
}

void QQuickStochasticEngine::restart(int index)
{
    bool randomStart = (m_startTimes.at(index) == NINF);
    m_startTimes[index] = m_timeOffset;
    if (m_addAdvance)
        m_startTimes[index] += m_advanceTime.elapsed();
    if (randomStart)
        m_startTimes[index] -= qrand() % m_duration.at(index);
    int time = m_duration.at(index) + m_startTimes.at(index);
    for (int i=0; i<m_stateUpdates.count(); i++)
        m_stateUpdates[i].second.removeAll(index);
    if (m_duration.at(index) >= 0)
        addToUpdateList(time, index);
}

void QQuickSpriteEngine::restart(int index) //Reimplemented to recognize and handle pseudostates
{
    bool randomStart = (m_startTimes.at(index) == NINF);
    if (m_loaded && m_sprites.at(m_things.at(index))->frameSync()) {//Manually advanced
        m_startTimes[index] = 0;
        if (randomStart && m_sprites.at(m_things.at(index))->m_generatedCount)
            m_startTimes[index] += qrand() % m_sprites.at(m_things.at(index))->m_generatedCount;
    } else {
        m_startTimes[index] = m_timeOffset;
        if (m_addAdvance)
            m_startTimes[index] += m_advanceTime.elapsed();
        if (randomStart)
            m_startTimes[index] -= qrand() % m_duration.at(index);
        int time = spriteDuration(index) + m_startTimes.at(index);
        if (randomStart) {
            int curTime = m_timeOffset + (m_addAdvance ? m_advanceTime.elapsed() : 0);
            while (time < curTime) //Fast forward through psuedostates as needed
                time += spriteDuration(index);
        }

        for (int i=0; i<m_stateUpdates.count(); i++)
            m_stateUpdates[i].second.removeAll(index);
        addToUpdateList(time, index);
    }
}

void QQuickStochasticEngine::advance(int idx)
{
    if (idx >= m_things.count())
        return;//TODO: Proper fix(because this has happened and I just ignored it)
    int nextIdx = nextState(m_things.at(idx), idx);
    m_things[idx] = nextIdx;
    m_duration[idx] = m_states.at(nextIdx)->variedDuration();
    restart(idx);
    emit m_states.at(nextIdx)->entered();
    emit stateChanged(idx);
}

void QQuickSpriteEngine::advance(int idx) //Reimplemented to recognize and handle pseudostates
{
    if (!m_loaded) {
        qWarning() << QLatin1String("QQuickSpriteEngine: Trying to advance sprites before sprites finish loading. Ignoring directive");
        return;
    }

    if (idx >= m_things.count())
        return;//TODO: Proper fix(because this has happened and I just ignored it)
    if (m_duration.at(idx) == 0) {
        if (m_sprites.at(m_things.at(idx))->frameSync()) {
            //Manually called, advance inner substate count
            m_startTimes[idx]++;
            if (m_startTimes.at(idx) < m_sprites.at(m_things.at(idx))->m_generatedCount) {
                //only a pseudostate ended
                emit stateChanged(idx);
                return;
            }
        }
        //just go past the pseudostate logic
    } else if (m_startTimes.at(idx) + m_duration.at(idx)
            > int(m_timeOffset + (m_addAdvance ? m_advanceTime.elapsed() : 0))) {
        //only a pseduostate ended
        emit stateChanged(idx);
        addToUpdateList(spriteStart(idx) + spriteDuration(idx) + (m_addAdvance ? m_advanceTime.elapsed() : 0), idx);
        return;
    }
    int nextIdx = nextState(m_things.at(idx), idx);
    m_things[idx] = nextIdx;
    m_duration[idx] = m_states.at(nextIdx)->variedDuration();
    restart(idx);
    emit m_states.at(nextIdx)->entered();
    emit stateChanged(idx);
}

int QQuickStochasticEngine::nextState(int curState, int curThing)
{
    int nextIdx = -1;
    int goalPath = goalSeek(curState, curThing);
    if (goalPath == -1){//Random
        qreal r =(qreal) qrand() / (qreal) RAND_MAX;
        qreal total = 0.0;
        for (QVariantMap::const_iterator iter=m_states.at(curState)->m_to.constBegin();
            iter!=m_states.at(curState)->m_to.constEnd(); ++iter)
            total += (*iter).toReal();
        r*=total;
        for (QVariantMap::const_iterator iter= m_states.at(curState)->m_to.constBegin();
                iter!=m_states.at(curState)->m_to.constEnd(); ++iter){
            if (r < (*iter).toReal()){
                bool superBreak = false;
                for (int i=0; i<m_states.count(); i++){
                    if (m_states.at(i)->name() == iter.key()){
                        nextIdx = i;
                        superBreak = true;
                        break;
                    }
                }
                if (superBreak)
                    break;
            }
            r -= (*iter).toReal();
        }
    }else{//Random out of shortest paths to goal
        nextIdx = goalPath;
    }
    if (nextIdx == -1)//No 'to' states means stay here
        nextIdx = curState;
    return nextIdx;
}

uint QQuickStochasticEngine::updateSprites(uint time)//### would returning a list of changed idxs be faster than signals?
{
    //Sprite State Update;
    m_timeOffset = time;
    m_addAdvance = false;
    while (!m_stateUpdates.isEmpty() && time >= m_stateUpdates.constFirst().first){
        const auto copy = m_stateUpdates.constFirst().second;
        for (int idx : copy)
            advance(idx);
        m_stateUpdates.pop_front();
    }

    m_advanceTime.start();
    m_addAdvance = true;
    if (m_stateUpdates.isEmpty())
        return uint(-1);
    return m_stateUpdates.constFirst().first;
}

int QQuickStochasticEngine::goalSeek(int curIdx, int spriteIdx, int dist)
{
    QString goalName;
    if (m_goals.at(spriteIdx) != -1)
        goalName = m_states.at(m_goals.at(spriteIdx))->name();
    else
        goalName = m_globalGoal;
    if (goalName.isEmpty())
        return -1;
    //TODO: caching instead of excessively redoing iterative deepening (which was chosen arbitrarily anyways)
    // Paraphrased - implement in an *efficient* manner
    for (int i=0; i<m_states.count(); i++)
        if (m_states.at(curIdx)->name() == goalName)
            return curIdx;
    if (dist < 0)
        dist = m_states.count();
    QQuickStochasticState* curState = m_states.at(curIdx);
    for (QVariantMap::const_iterator iter = curState->m_to.constBegin();
        iter!=curState->m_to.constEnd(); ++iter){
        if (iter.key() == goalName)
            for (int i=0; i<m_states.count(); i++)
                if (m_states.at(i)->name() == goalName)
                    return i;
    }
    QSet<int> options;
    for (int i=1; i<dist; i++){
        for (QVariantMap::const_iterator iter = curState->m_to.constBegin();
            iter!=curState->m_to.constEnd(); ++iter){
            int option = -1;
            for (int j=0; j<m_states.count(); j++)//One place that could be a lot more efficient...
                if (m_states.at(j)->name() == iter.key())
                    if (goalSeek(j, spriteIdx, i) != -1)
                        option = j;
            if (option != -1)
                options << option;
        }
        if (!options.isEmpty()){
            if (options.count()==1)
                return *(options.begin());
            int option = -1;
            qreal r =(qreal) qrand() / (qreal) RAND_MAX;
            qreal total = 0;
            for (QSet<int>::const_iterator iter=options.constBegin();
                iter!=options.constEnd(); ++iter)
                total += curState->m_to.value(m_states.at((*iter))->name()).toReal();
            r *= total;
            for (QVariantMap::const_iterator iter = curState->m_to.constBegin();
                iter!=curState->m_to.constEnd(); ++iter){
                bool superContinue = true;
                for (int j=0; j<m_states.count(); j++)
                    if (m_states.at(j)->name() == iter.key())
                        if (options.contains(j))
                            superContinue = false;
                if (superContinue)
                    continue;
                if (r < (*iter).toReal()){
                    bool superBreak = false;
                    for (int j=0; j<m_states.count(); j++){
                        if (m_states.at(j)->name() == iter.key()){
                            option = j;
                            superBreak = true;
                            break;
                        }
                    }
                    if (superBreak)
                        break;
                }
                r-=(*iter).toReal();
            }
            return option;
        }
    }
    return -1;
}

void QQuickStochasticEngine::addToUpdateList(uint t, int idx)
{
    for (int i=0; i<m_stateUpdates.count(); i++){
        if (m_stateUpdates.at(i).first == t){
            m_stateUpdates[i].second << idx;
            return;
        } else if (m_stateUpdates.at(i).first > t) {
            QList<int> tmpList;
            tmpList << idx;
            m_stateUpdates.insert(i, qMakePair(t, tmpList));
            return;
        }
    }
    QList<int> tmpList;
    tmpList << idx;
    m_stateUpdates << qMakePair(t, tmpList);
}

QT_END_NAMESPACE
