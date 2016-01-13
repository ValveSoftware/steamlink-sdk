/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickparticlesystem_p.h"
#include <QtQuick/qsgnode.h>
#include "qquickparticleemitter_p.h"
#include "qquickparticleaffector_p.h"
#include "qquickparticlepainter_p.h"
#include <private/qquickspriteengine_p.h>
#include <private/qquicksprite_p.h>
#include "qquickv4particledata_p.h"
#include "qquickparticlegroup_p.h"

#include "qquicktrailemitter_p.h"//###For auto-follow on states, perhaps should be in emitter?
#include <private/qqmlengine_p.h>
#include <private/qqmlglobal_p.h>
#include <cmath>
#include <QDebug>

QT_BEGIN_NAMESPACE
//###Switch to define later, for now user-friendly (no compilation) debugging is worth it
DEFINE_BOOL_CONFIG_OPTION(qmlParticlesDebug, QML_PARTICLES_DEBUG)


/* \internal ParticleSystem internals documentation

   Affectors, Painters, Emitters and Groups all register themselves on construction as a callback
   from their setSystem (or componentComplete if they have a system from a parent).

   Particle data is stored by group, They have a group index (used by the particle system almost
   everywhere) and a global index (used by the Stochastic state engine powering stochastic group
   transitions). Each group has a recycling list/heap that stores the particle data.

   The recycling list/heap is a heap of particle data sorted by when they're expected to die. If
   they die prematurely then they are marked as reusable (and will probably still be alive when
   they exit the heap). If they have their life extended, then they aren't dead when expected.
   If this happens, they go back in the heap with the new estimate. If they have died on schedule,
   then the indexes are marked as reusable. If no indexes are reusable when new particles are
   requested, then the list is extended. This relatively complex datastructure is because memory
   allocation and deallocation on this scale proved to be a significant performance cost. In order
   to reuse the indexes validly (even when particles can have their life extended or cut short
   dynamically, or particle counts grow) this seemed to be the most efficient option for keeping
   track of which indices could be reused.

   When a new particle is emitted, the emitter gets a new datum from the group (through the
   system), and sets properties on it. Then it's passed back to the group briefly so that it can
   now guess when the particle will die. Then the painters get a change to initialize properties
   as well, since particle data includes shared data from painters as well as logical particle
   data.

   Every animation advance, the simulation advances by running all emitters for the elapsed
   duration, then running all affectors, then telling all particle painters to update changed
   particles. The ParticlePainter superclass stores these changes, and they are implemented
   when the painter is called to paint in the render thread.

   Particle group changes move the particle from one group to another by killing the old particle
   and then creating a new one with the same data in the new group.

   Note that currently groups only grow. Given that data is stored in vectors, it is non-trivial
   to pluck out the unused indexes when the count goes down. Given the dynamic nature of the
   system, it is difficult to tell if those unused data instances will be used again. Still,
   some form of garbage collection is on the long term plan.
*/

/*!
    \qmltype ParticleSystem
    \instantiates QQuickParticleSystem
    \inqmlmodule QtQuick.Particles
    \brief A system which includes particle painter, emitter, and affector types
    \ingroup qtquick-particles

*/

/*!
    \qmlproperty bool QtQuick.Particles::ParticleSystem::running

    If running is set to false, the particle system will stop the simulation. All particles
    will be destroyed when the system is set to running again.

    It can also be controlled with the start() and stop() methods.
*/


/*!
    \qmlproperty bool QtQuick.Particles::ParticleSystem::paused

    If paused is set to true, the particle system will not advance the simulation. When
    paused is set to false again, the simulation will resume from the same point it was
    paused.

    The simulation will automatically pause if it detects that there are no live particles
    left, and unpause when new live particles are added.

    It can also be controlled with the pause() and resume() methods.
*/

/*!
    \qmlproperty bool QtQuick.Particles::ParticleSystem::empty

    empty is set to true when there are no live particles left in the system.

    You can use this to pause the system, keeping it from spending any time updating,
    but you will need to resume it in order for additional particles to be generated
    by the system.

    To kill all the particles in the system, use an Age affector.
*/

/*!
    \qmlproperty list<Sprite> QtQuick.Particles::ParticleSystem::particleStates

    You can define a sub-set of particle groups in this property in order to provide them
    with stochastic state transitions.

    Each QtQuick::Sprite in this list is interpreted as corresponding to the particle group
    with ths same name. Any transitions defined in these sprites will take effect on the particle
    groups as well. Additionally TrailEmitters, Affectors and ParticlePainters definined
    inside one of these sprites are automatically associated with the corresponding particle group.
*/

/*!
    \qmlmethod QtQuick.Particles::ParticleSystem::pause()

    Pauses the simulation if it is running.

    \sa resume, paused
*/

/*!
    \qmlmethod QtQuick.Particles::ParticleSystem::resume()

    Resumes the simulation if it is paused.

    \sa pause, paused
*/

/*!
    \qmlmethod QtQuick.Particles::ParticleSystem::start()

    Starts the simulation if it has not already running.

    \sa stop, restart, running
*/

/*!
    \qmlmethod QtQuick.Particles::ParticleSystem::stop()

    Stops the simulation if it is running.

    \sa start, restart, running
*/

/*!
    \qmlmethod QtQuick.Particles::ParticleSystem::restart()

    Stops the simulation if it is running, and then starts it.

    \sa start, stop, running
*/
/*!
    \qmlmethod QtQuick.Particles::ParticleSystem::reset()

    Discards all currently existing particles.

*/
const qreal EPSILON = 0.001;
//Utility functions for when within 1ms is close enough
bool timeEqualOrGreater(qreal a, qreal b)
{
    return (a+EPSILON >= b);
}

bool timeLess(qreal a, qreal b)
{
    return (a-EPSILON < b);
}

bool timeEqual(qreal a, qreal b)
{
    return (a+EPSILON > b) && (a-EPSILON < b);
}

int roundedTime(qreal a)
{// in ms
    return (int)qRound(a*1000.0);
}

QQuickParticleDataHeap::QQuickParticleDataHeap()
    : m_data(0)
{
    m_data.reserve(1000);
    clear();
}

void QQuickParticleDataHeap::grow() //###Consider automatic growth vs resize() calls from GroupData
{
    m_data.resize(1 << ++m_size);
}

void QQuickParticleDataHeap::insert(QQuickParticleData* data)
{
    insertTimed(data, roundedTime(data->t + data->lifeSpan));
}

void QQuickParticleDataHeap::insertTimed(QQuickParticleData* data, int time)
{
    //TODO: Optimize 0 lifespan (or already dead) case
    if (m_lookups.contains(time)) {
        m_data[m_lookups[time]].data << data;
        return;
    }
    if (m_end == (1 << m_size))
        grow();
    m_data[m_end].time = time;
    m_data[m_end].data.clear();
    m_data[m_end].data.insert(data);
    m_lookups.insert(time, m_end);
    bubbleUp(m_end++);
}

int QQuickParticleDataHeap::top()
{
    if (m_end == 0)
        return 1 << 30;
    return m_data[0].time;
}

QSet<QQuickParticleData*> QQuickParticleDataHeap::pop()
{
    if (!m_end)
        return QSet<QQuickParticleData*> ();
    QSet<QQuickParticleData*> ret = m_data[0].data;
    m_lookups.remove(m_data[0].time);
    if (m_end == 1) {
        --m_end;
    } else {
        m_data[0] = m_data[--m_end];
        bubbleDown(0);
    }
    return ret;
}

void QQuickParticleDataHeap::clear()
{
    m_size = 0;
    m_end = 0;
    //m_size is in powers of two. So to start at 0 we have one allocated
    m_data.resize(1);
    m_lookups.clear();
}

bool QQuickParticleDataHeap::contains(QQuickParticleData* d)
{
    for (int i=0; i<m_end; i++)
        if (m_data[i].data.contains(d))
            return true;
    return false;
}

void QQuickParticleDataHeap::swap(int a, int b)
{
    m_tmp = m_data[a];
    m_data[a] = m_data[b];
    m_data[b] = m_tmp;
    m_lookups[m_data[a].time] = a;
    m_lookups[m_data[b].time] = b;
}

void QQuickParticleDataHeap::bubbleUp(int idx)//tends to be called once
{
    if (!idx)
        return;
    int parent = (idx-1)/2;
    if (m_data[idx].time < m_data[parent].time) {
        swap(idx, parent);
        bubbleUp(parent);
    }
}

void QQuickParticleDataHeap::bubbleDown(int idx)//tends to be called log n times
{
    int left = idx*2 + 1;
    if (left >= m_end)
        return;
    int lesser = left;
    int right = idx*2 + 2;
    if (right < m_end) {
        if (m_data[left].time > m_data[right].time)
            lesser = right;
    }
    if (m_data[idx].time > m_data[lesser].time) {
        swap(idx, lesser);
        bubbleDown(lesser);
    }
}

QQuickParticleGroupData::QQuickParticleGroupData(int id, QQuickParticleSystem* sys):index(id),m_size(0),m_system(sys)
{
    initList();
}

QQuickParticleGroupData::~QQuickParticleGroupData()
{
    foreach (QQuickParticleData* d, data)
        delete d;
}

int QQuickParticleGroupData::size()
{
    return m_size;
}

QString QQuickParticleGroupData::name()//### Worth caching as well?
{
    return m_system->groupIds.key(index);
}

void QQuickParticleGroupData::setSize(int newSize)
{
    if (newSize == m_size)
        return;
    Q_ASSERT(newSize > m_size);//XXX allow shrinking
    data.resize(newSize);
    for (int i=m_size; i<newSize; i++) {
        data[i] = new QQuickParticleData(m_system);
        data[i]->group = index;
        data[i]->index = i;
        reusableIndexes << i;
    }
    int delta = newSize - m_size;
    m_size = newSize;
    foreach (QQuickParticlePainter* p, painters)
        p->setCount(p->count() + delta);
}

void QQuickParticleGroupData::initList()
{
    dataHeap.clear();
}

void QQuickParticleGroupData::kill(QQuickParticleData* d)
{
    Q_ASSERT(d->group == index);
    d->lifeSpan = 0;//Kill off
    foreach (QQuickParticlePainter* p, painters)
        p->reload(d);
    reusableIndexes << d->index;
}

QQuickParticleData* QQuickParticleGroupData::newDatum(bool respectsLimits)
{
    //recycle();//Extra recycler round to be sure?

    while (!reusableIndexes.empty()) {
        int idx = *(reusableIndexes.begin());
        reusableIndexes.remove(idx);
        if (data[idx]->stillAlive()) {// ### This means resurrection of 'dead' particles. Is that allowed?
            prepareRecycler(data[idx]);
            continue;
        }
        return data[idx];
    }
    if (respectsLimits)
        return 0;

    int oldSize = m_size;
    setSize(oldSize + 10);//###+1,10%,+10? Choose something non-arbitrarily
    reusableIndexes.remove(oldSize);
    return data[oldSize];
}

bool QQuickParticleGroupData::recycle()
{
    while (dataHeap.top() <= m_system->timeInt) {
        foreach (QQuickParticleData* datum, dataHeap.pop()) {
            if (!datum->stillAlive()) {
                reusableIndexes << datum->index;
            } else {
                prepareRecycler(datum); //ttl has been altered mid-way, put it back
            }
        }
    }

    //TODO: If the data is clear, gc (consider shrinking stack size)?
    return reusableIndexes.count() == m_size;
}

void QQuickParticleGroupData::prepareRecycler(QQuickParticleData* d)
{
    if (d->lifeSpan*1000 < m_system->maxLife) {
        dataHeap.insert(d);
    } else {
        while ((roundedTime(d->t) + 2*m_system->maxLife/3) <= m_system->timeInt)
            d->extendLife(m_system->maxLife/3000.0);
        dataHeap.insertTimed(d, roundedTime(d->t) + 2*m_system->maxLife/3);
    }
}

QQuickParticleData::QQuickParticleData(QQuickParticleSystem* sys)
    : group(0)
    , e(0)
    , system(sys)
    , index(0)
    , systemIndex(-1)
    , colorOwner(0)
    , rotationOwner(0)
    , deformationOwner(0)
    , animationOwner(0)
    , v8Datum(0)
{
    x = 0;
    y = 0;
    t = -1;
    lifeSpan = 0;
    size = 0;
    endSize = 0;
    vx = 0;
    vy = 0;
    ax = 0;
    ay = 0;
    xx = 1;
    xy = 0;
    yx = 0;
    yy = 1;
    rotation = 0;
    rotationVelocity = 0;
    autoRotate = 0;
    animIdx = 0;
    frameDuration = 1;
    frameAt = -1;
    frameCount = 1;
    animT = -1;
    animX = 0;
    animY = 0;
    animWidth = 1;
    animHeight = 1;
    color.r = 255;
    color.g = 255;
    color.b = 255;
    color.a = 255;
    r = 0;
    delegate = 0;
    modelIndex = -1;
}

QQuickParticleData::~QQuickParticleData()
{
    delete v8Datum;
}

QQuickParticleData::QQuickParticleData(const QQuickParticleData &other)
{
    *this = other;
}

QQuickParticleData &QQuickParticleData::operator=(const QQuickParticleData &other)
{
    clone(other);

    group = other.group;
    e = other.e;
    system = other.system;
    index = other.index;
    systemIndex = other.systemIndex;
    // Lazily initialized
    v8Datum = 0;

    return *this;
}

void QQuickParticleData::clone(const QQuickParticleData& other)
{
    x = other.x;
    y = other.y;
    t = other.t;
    lifeSpan = other.lifeSpan;
    size = other.size;
    endSize = other.endSize;
    vx = other.vx;
    vy = other.vy;
    ax = other.ax;
    ay = other.ay;
    xx = other.xx;
    xy = other.xy;
    yx = other.yx;
    yy = other.yy;
    rotation = other.rotation;
    rotationVelocity = other.rotationVelocity;
    autoRotate = other.autoRotate;
    animIdx = other.animIdx;
    frameDuration = other.frameDuration;
    frameCount = other.frameCount;
    animT = other.animT;
    animX = other.animX;
    animY = other.animY;
    animWidth = other.animWidth;
    animHeight = other.animHeight;
    color.r = other.color.r;
    color.g = other.color.g;
    color.b = other.color.b;
    color.a = other.color.a;
    r = other.r;
    delegate = other.delegate;
    modelIndex = other.modelIndex;

    colorOwner = other.colorOwner;
    rotationOwner = other.rotationOwner;
    deformationOwner = other.deformationOwner;
    animationOwner = other.animationOwner;
}

QQmlV4Handle QQuickParticleData::v4Value()
{
    if (!v8Datum)
        v8Datum = new QQuickV4ParticleData(QQmlEnginePrivate::getV8Engine(qmlEngine(system)), this);
    return v8Datum->v4Value();
}
//sets the x accleration without affecting the instantaneous x velocity or position
void QQuickParticleData::setInstantaneousAX(qreal ax)
{
    qreal t = (system->timeInt / 1000.0) - this->t;
    qreal vx = (this->vx + t*this->ax) - t*ax;
    qreal ex = this->x + this->vx * t + 0.5 * this->ax * t * t;
    qreal x = ex - t*vx - 0.5 * t*t*ax;

    this->ax = ax;
    this->vx = vx;
    this->x = x;
}

//sets the x velocity without affecting the instantaneous x postion
void QQuickParticleData::setInstantaneousVX(qreal vx)
{
    qreal t = (system->timeInt / 1000.0) - this->t;
    qreal evx = vx - t*this->ax;
    qreal ex = this->x + this->vx * t + 0.5 * this->ax * t * t;
    qreal x = ex - t*evx - 0.5 * t*t*this->ax;

    this->vx = evx;
    this->x = x;
}

//sets the instantaneous x postion
void QQuickParticleData::setInstantaneousX(qreal x)
{
    qreal t = (system->timeInt / 1000.0) - this->t;
    this->x = x - t*this->vx - 0.5 * t*t*this->ax;
}

//sets the y accleration without affecting the instantaneous y velocity or position
void QQuickParticleData::setInstantaneousAY(qreal ay)
{
    qreal t = (system->timeInt / 1000.0) - this->t;
    qreal vy = (this->vy + t*this->ay) - t*ay;
    qreal ey = this->y + this->vy * t + 0.5 * this->ay * t * t;
    qreal y = ey - t*vy - 0.5 * t*t*ay;

    this->ay = ay;
    this->vy = vy;
    this->y = y;
}

//sets the y velocity without affecting the instantaneous y position
void QQuickParticleData::setInstantaneousVY(qreal vy)
{
    qreal t = (system->timeInt / 1000.0) - this->t;
    qreal evy = vy - t*this->ay;
    qreal ey = this->y + this->vy * t + 0.5 * this->ay * t * t;
    qreal y = ey - t*evy - 0.5 * t*t*this->ay;

    this->vy = evy;
    this->y = y;
}

//sets the instantaneous Y position
void QQuickParticleData::setInstantaneousY(qreal y)
{
    qreal t = (system->timeInt / 1000.0) - this->t;
    this->y = y - t*this->vy - 0.5 * t*t*this->ay;
}

qreal QQuickParticleData::curX() const
{
    qreal t = (system->timeInt / 1000.0) - this->t;
    return this->x + this->vx * t + 0.5 * this->ax * t * t;
}

qreal QQuickParticleData::curVX() const
{
    qreal t = (system->timeInt / 1000.0) - this->t;
    return this->vx + t*this->ax;
}

qreal QQuickParticleData::curY() const
{
    qreal t = (system->timeInt / 1000.0) - this->t;
    return y + vy * t + 0.5 * ay * t * t;
}

qreal QQuickParticleData::curVY() const
{
    qreal t = (system->timeInt / 1000.0) - this->t;
    return vy + t*ay;
}

void QQuickParticleData::debugDump()
{
    qDebug() << "Particle" << systemIndex << group << "/" << index << stillAlive()
             << "Pos: " << x << "," << y
             << "Vel: " << vx << "," << vy
             << "Acc: " << ax << "," << ay
             << "Size: " << size << "," << endSize
             << "Time: " << t << "," <<lifeSpan << ";" << (system->timeInt / 1000.0) ;
}

bool QQuickParticleData::stillAlive()
{
    if (!system)
        return false;
    return (t + lifeSpan - EPSILON) > ((qreal)system->timeInt/1000.0);
}

bool QQuickParticleData::alive()
{
    if (!system)
        return false;
    qreal st = ((qreal)system->timeInt/1000.0);
    return (t + EPSILON) < st && (t + lifeSpan - EPSILON) > st;
}

float QQuickParticleData::curSize()
{
    if (!system || !lifeSpan)
        return 0.0f;
    return size + (endSize - size) * (1 - (lifeLeft() / lifeSpan));
}

float QQuickParticleData::lifeLeft()
{
    if (!system)
        return 0.0f;
    return (t + lifeSpan) - (system->timeInt/1000.0);
}

void QQuickParticleData::extendLife(float time)
{
    qreal newX = curX();
    qreal newY = curY();
    qreal newVX = curVX();
    qreal newVY = curVY();

    t += time;
    animT += time;

    qreal elapsed = (system->timeInt / 1000.0) - t;
    qreal evy = newVY - elapsed*ay;
    qreal ey = newY - elapsed*evy - 0.5 * elapsed*elapsed*ay;
    qreal evx = newVX - elapsed*ax;
    qreal ex = newX - elapsed*evx - 0.5 * elapsed*elapsed*ax;

    x = ex;
    vx = evx;
    y = ey;
    vy = evy;
}

QQuickParticleSystem::QQuickParticleSystem(QQuickItem *parent) :
    QQuickItem(parent),
    stateEngine(0),
    m_animation(0),
    m_running(true),
    initialized(0),
    particleCount(0),
    m_nextIndex(0),
    m_componentComplete(false),
    m_paused(false),
    m_empty(true)
{
    connect(&m_painterMapper, SIGNAL(mapped(QObject*)),
            this, SLOT(loadPainter(QObject*)));

    m_debugMode = qmlParticlesDebug();
}

QQuickParticleSystem::~QQuickParticleSystem()
{
    foreach (QQuickParticleGroupData* gd, groupData)
        delete gd;
}

void QQuickParticleSystem::initGroups()
{
    m_reusableIndexes.clear();
    m_nextIndex = 0;

    qDeleteAll(groupData);
    groupData.clear();
    groupIds.clear();

    QQuickParticleGroupData* gd = new QQuickParticleGroupData(0, this);//Default group
    groupData.insert(0,gd);
    groupIds.insert(QString(), 0);
    m_nextGroupId = 1;
}

void QQuickParticleSystem::registerParticlePainter(QQuickParticlePainter* p)
{
    if (m_debugMode)
        qDebug() << "Registering Painter" << p << "to" << this;
    //TODO: a way to Unregister emitters, painters and affectors
    m_painters << QPointer<QQuickParticlePainter>(p);//###Set or uniqueness checking?
    connect(p, SIGNAL(groupsChanged(QStringList)),
            &m_painterMapper, SLOT(map()));
    loadPainter(p);
}

void QQuickParticleSystem::registerParticleEmitter(QQuickParticleEmitter* e)
{
    if (m_debugMode)
        qDebug() << "Registering Emitter" << e << "to" << this;
    m_emitters << QPointer<QQuickParticleEmitter>(e);//###How to get them out?
    connect(e, SIGNAL(particleCountChanged()),
            this, SLOT(emittersChanged()));
    connect(e, SIGNAL(groupChanged(QString)),
            this, SLOT(emittersChanged()));
    emittersChanged();
    e->reset();//Start, so that starttime factors appropriately
}

void QQuickParticleSystem::registerParticleAffector(QQuickParticleAffector* a)
{
    if (m_debugMode)
        qDebug() << "Registering Affector" << a << "to" << this;
    m_affectors << QPointer<QQuickParticleAffector>(a);
}

void QQuickParticleSystem::registerParticleGroup(QQuickParticleGroup* g)
{
    if (m_debugMode)
        qDebug() << "Registering Group" << g << "to" << this;
    m_groups << QPointer<QQuickParticleGroup>(g);
    createEngine();
}

void QQuickParticleSystem::setRunning(bool arg)
{
    if (m_running != arg) {
        m_running = arg;
        emit runningChanged(arg);
        setPaused(false);
        if (m_animation)//Not created until componentCompleted
            m_running ? m_animation->start() : m_animation->stop();
        reset();
    }
}

void QQuickParticleSystem::setPaused(bool arg) {
    if (m_paused != arg) {
        m_paused = arg;
        if (m_animation && m_animation->state() != QAbstractAnimation::Stopped)
            m_paused ? m_animation->pause() : m_animation->resume();
        if (!m_paused) {
            foreach (QQuickParticlePainter *p, m_painters)
                p->update();
        }
        emit pausedChanged(arg);
    }
}

void QQuickParticleSystem::statePropertyRedirect(QQmlListProperty<QObject> *prop, QObject *value)
{
    //Hooks up automatic state-associated stuff
    QQuickParticleSystem* sys = qobject_cast<QQuickParticleSystem*>(prop->object->parent());
    QQuickParticleGroup* group = qobject_cast<QQuickParticleGroup*>(prop->object);
    if (!group || !sys || !value)
        return;
    stateRedirect(group, sys, value);
}

void QQuickParticleSystem::stateRedirect(QQuickParticleGroup* group, QQuickParticleSystem* sys, QObject *value)
{
    QStringList list;
    list << group->name();
    QQuickParticleAffector* a = qobject_cast<QQuickParticleAffector*>(value);
    if (a) {
        a->setParentItem(sys);
        a->setGroups(list);
        a->setSystem(sys);
        return;
    }
    QQuickTrailEmitter* fe = qobject_cast<QQuickTrailEmitter*>(value);
    if (fe) {
        fe->setParentItem(sys);
        fe->setFollow(group->name());
        fe->setSystem(sys);
        return;
    }
    QQuickParticleEmitter* e = qobject_cast<QQuickParticleEmitter*>(value);
    if (e) {
        e->setParentItem(sys);
        e->setGroup(group->name());
        e->setSystem(sys);
        return;
    }
    QQuickParticlePainter* p = qobject_cast<QQuickParticlePainter*>(value);
    if (p) {
        p->setParentItem(sys);
        p->setGroups(list);
        p->setSystem(sys);
        return;
    }
    qWarning() << value << " was placed inside a particle system state but cannot be taken into the particle system. It will be lost.";
}

void QQuickParticleSystem::componentComplete()

{
    QQuickItem::componentComplete();
    m_componentComplete = true;
    m_animation = new QQuickParticleSystemAnimation(this);
    reset();//restarts animation as well
}

void QQuickParticleSystem::reset()
{
    if (!m_componentComplete)
        return;

    timeInt = 0;
    //Clear guarded pointers which have been deleted
    int cleared = 0;
    cleared += m_emitters.removeAll(0);
    cleared += m_painters.removeAll(0);
    cleared += m_affectors.removeAll(0);

    bySysIdx.resize(0);
    initGroups();//Also clears all logical particles

    if (!m_running)
        return;

    foreach (QQuickParticleEmitter* e, m_emitters)
        e->reset();

    emittersChanged();

    foreach (QQuickParticlePainter *p, m_painters) {
        loadPainter(p);
        p->reset();
    }

    //### Do affectors need reset too?
    if (m_animation) {//Animation is explicitly disabled in benchmarks
        //reset restarts animation (if running)
        if ((m_animation->state() == QAbstractAnimation::Running))
            m_animation->stop();
        m_animation->start();
        if (m_paused)
            m_animation->pause();
    }

    initialized = true;
}


void QQuickParticleSystem::loadPainter(QObject *p)
{
    if (!m_componentComplete || !p)
        return;

    QQuickParticlePainter* painter = qobject_cast<QQuickParticlePainter*>(p);
    Q_ASSERT(painter);//XXX
    foreach (QQuickParticleGroupData* sg, groupData)
        sg->painters.remove(painter);
    int particleCount = 0;
    if (painter->groups().isEmpty()) {//Uses default particle
        QStringList def;
        def << QString();
        painter->setGroups(def);
        particleCount += groupData[0]->size();
        groupData[0]->painters << painter;
    } else {
        foreach (const QString &group, painter->groups()) {
            if (group != QLatin1String("") && !groupIds[group]) {//new group
                int id = m_nextGroupId++;
                QQuickParticleGroupData* gd = new QQuickParticleGroupData(id, this);
                groupIds.insert(group, id);
                groupData.insert(id, gd);
            }
            particleCount += groupData[groupIds[group]]->size();
            groupData[groupIds[group]]->painters << painter;
        }
    }
    painter->setCount(particleCount);
    painter->update();//Initial update here
    return;
}

void QQuickParticleSystem::emittersChanged()
{
    if (!m_componentComplete)
        return;

    m_emitters.removeAll(0);


    QList<int> previousSizes;
    QList<int> newSizes;
    for (int i=0; i<m_nextGroupId; i++) {
        previousSizes << groupData[i]->size();
        newSizes << 0;
    }

    foreach (QQuickParticleEmitter* e, m_emitters) {//Populate groups and set sizes.
        if (!groupIds.contains(e->group())
                || (!e->group().isEmpty() && !groupIds[e->group()])) {//or it was accidentally inserted by a failed lookup earlier
            int id = m_nextGroupId++;
            QQuickParticleGroupData* gd = new QQuickParticleGroupData(id, this);
            groupIds.insert(e->group(), id);
            groupData.insert(id, gd);
            previousSizes << 0;
            newSizes << 0;
        }
        newSizes[groupIds[e->group()]] += e->particleCount();
        //###: Cull emptied groups?
    }

    //TODO: Garbage collection?
    particleCount = 0;
    for (int i=0; i<m_nextGroupId; i++) {
        groupData[i]->setSize(qMax(newSizes[i], previousSizes[i]));
        particleCount += groupData[i]->size();
    }

    if (m_debugMode)
        qDebug() << "Particle system emitters changed. New particle count: " << particleCount;

    if (particleCount > bySysIdx.size())//New datum requests haven't updated it
        bySysIdx.resize(particleCount);

    foreach (QQuickParticleAffector *a, m_affectors)//Groups may have changed
        a->m_updateIntSet = true;

    foreach (QQuickParticlePainter *p, m_painters)
        loadPainter(p);

    if (!m_groups.isEmpty())
        createEngine();

}

void QQuickParticleSystem::createEngine()
{
    if (!m_componentComplete)
        return;
    if (stateEngine && m_debugMode)
        qDebug() << "Resetting Existing Sprite Engine...";
    //### Solve the losses if size/states go down
    foreach (QQuickParticleGroup* group, m_groups) {
        bool exists = false;
        foreach (const QString &name, groupIds.keys())
            if (group->name() == name)
                exists = true;
        if (!exists) {
            int id = m_nextGroupId++;
            QQuickParticleGroupData* gd = new QQuickParticleGroupData(id, this);
            groupIds.insert(group->name(), id);
            groupData.insert(id, gd);
        }
    }

    if (m_groups.count()) {
        //Reorder groups List so as to have the same order as groupData
        QList<QQuickParticleGroup*> newList;
        for (int i=0; i<m_nextGroupId; i++) {
            bool exists = false;
            QString name = groupData[i]->name();
            foreach (QQuickParticleGroup* existing, m_groups) {
                if (existing->name() == name) {
                    newList << existing;
                    exists = true;
                }
            }
            if (!exists) {
                newList << new QQuickParticleGroup(this);
                newList.back()->setName(name);
            }
        }
        m_groups = newList;
        QList<QQuickStochasticState*> states;
        foreach (QQuickParticleGroup* g, m_groups)
            states << (QQuickStochasticState*)g;

        if (!stateEngine)
            stateEngine = new QQuickStochasticEngine(this);
        stateEngine->setCount(particleCount);
        stateEngine->m_states = states;

        connect(stateEngine, SIGNAL(stateChanged(int)),
                this, SLOT(particleStateChange(int)));

    } else {
        if (stateEngine)
            delete stateEngine;
        stateEngine = 0;
    }

}

void QQuickParticleSystem::particleStateChange(int idx)
{
    moveGroups(bySysIdx[idx], stateEngine->curState(idx));
}

void QQuickParticleSystem::moveGroups(QQuickParticleData *d, int newGIdx)
{
    if (!d || newGIdx == d->group)
        return;

    QQuickParticleData* pd = newDatum(newGIdx, false, d->systemIndex);
    if (!pd)
        return;

    pd->clone(*d);
    finishNewDatum(pd);

    d->systemIndex = -1;
    groupData[d->group]->kill(d);
}

int QQuickParticleSystem::nextSystemIndex()
{
    if (!m_reusableIndexes.isEmpty()) {
        int ret = *(m_reusableIndexes.begin());
        m_reusableIndexes.remove(ret);
        return ret;
    }
    if (m_nextIndex >= bySysIdx.size()) {
        bySysIdx.resize(bySysIdx.size() < 10 ? 10 : bySysIdx.size()*1.1);//###+1,10%,+10? Choose something non-arbitrarily
        if (stateEngine)
            stateEngine->setCount(bySysIdx.size());

    }
    return m_nextIndex++;
}

QQuickParticleData* QQuickParticleSystem::newDatum(int groupId, bool respectLimits, int sysIndex)
{
    Q_ASSERT(groupId < groupData.count());//XXX shouldn't really be an assert

    QQuickParticleData* ret = groupData[groupId]->newDatum(respectLimits);
    if (!ret) {
        return 0;
    }
    if (sysIndex == -1) {
        if (ret->systemIndex == -1)
            ret->systemIndex = nextSystemIndex();
    } else {
        if (ret->systemIndex != -1) {
            if (stateEngine)
                stateEngine->stop(ret->systemIndex);
            m_reusableIndexes << ret->systemIndex;
            bySysIdx[ret->systemIndex] = 0;
        }
        ret->systemIndex = sysIndex;
    }
    bySysIdx[ret->systemIndex] = ret;

    if (stateEngine)
        stateEngine->start(ret->systemIndex, ret->group);

    m_empty = false;
    return ret;
}

void QQuickParticleSystem::emitParticle(QQuickParticleData* pd)
{// called from prepareNextFrame()->emitWindow - enforce?
    //Account for relative emitter position
    bool okay = false;
    QTransform t = pd->e->itemTransform(this, &okay);
    if (okay) {
        qreal tx,ty;
        t.map(pd->x, pd->y, &tx, &ty);
        pd->x = tx;
        pd->y = ty;
    }

    finishNewDatum(pd);
}

void QQuickParticleSystem::finishNewDatum(QQuickParticleData *pd)
{
    Q_ASSERT(pd);
    groupData[pd->group]->prepareRecycler(pd);

    foreach (QQuickParticleAffector *a, m_affectors)
        if (a && a->m_needsReset)
            a->reset(pd);
    foreach (QQuickParticlePainter* p, groupData[pd->group]->painters)
        if (p)
            p->load(pd);
}

void QQuickParticleSystem::updateCurrentTime( int currentTime )
{
    if (!initialized)
        return;//error in initialization

    //### Elapsed time never shrinks - may cause problems if left emitting for weeks at a time.
    qreal dt = timeInt / 1000.;
    timeInt = currentTime;
    qreal time =  timeInt / 1000.;
    dt = time - dt;
    needsReset.clear();

    m_emitters.removeAll(0);
    m_painters.removeAll(0);
    m_affectors.removeAll(0);

    bool oldClear = m_empty;
    m_empty = true;
    foreach (QQuickParticleGroupData* gd, groupData)//Recycle all groups and see if they're out of live particles
        m_empty = gd->recycle() && m_empty;

    if (stateEngine)
        stateEngine->updateSprites(timeInt);

    foreach (QQuickParticleEmitter* emitter, m_emitters)
        emitter->emitWindow(timeInt);
    foreach (QQuickParticleAffector* a, m_affectors)
        a->affectSystem(dt);
    foreach (QQuickParticleData* d, needsReset)
        foreach (QQuickParticlePainter* p, groupData[d->group]->painters)
            p->reload(d);

    if (oldClear != m_empty)
        emptyChanged(m_empty);
}

int QQuickParticleSystem::systemSync(QQuickParticlePainter* p)
{
    if (!m_running)
        return 0;
    if (!initialized)
        return 0;//error in initialization
    p->performPendingCommits();
    return timeInt;
}


QT_END_NAMESPACE
