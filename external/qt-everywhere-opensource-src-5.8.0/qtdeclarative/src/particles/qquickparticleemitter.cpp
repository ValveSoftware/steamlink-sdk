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

#include "qquickparticleemitter_p.h"
#include <private/qqmlengine_p.h>
#include <private/qqmlglobal_p.h>
QT_BEGIN_NAMESPACE


/*!
    \qmltype Emitter
    \instantiates QQuickParticleEmitter
    \inqmlmodule QtQuick.Particles
    \brief Emits logical particles
    \ingroup qtquick-particles

    This element emits logical particles into the ParticleSystem, with the
    given starting attributes.

    Note that logical particles are not
    automatically rendered, you will need to have one or more
    ParticlePainter elements visualizing them.

    Note that the given starting attributes can be modified at any point
    in the particle's lifetime by any Affector element in the same
    ParticleSystem. This includes attributes like lifespan.
*/


/*!
    \qmlproperty ParticleSystem QtQuick.Particles::Emitter::system

    This is the Particle system that the Emitter will emit into.
    This can be omitted if the Emitter is a direct child of the ParticleSystem
*/
/*!
    \qmlproperty string QtQuick.Particles::Emitter::group

    This is the logical particle group which it will emit into.

    Default value is "" (empty string).
*/
/*!
    \qmlproperty Shape QtQuick.Particles::Emitter::shape

    This shape is applied with the size of the Emitter. Particles will be emitted
    randomly from any area covered by the shape.

    The default shape is a filled in rectangle, which corresponds to the full bounding
    box of the Emitter.
*/
/*!
    \qmlproperty bool QtQuick.Particles::Emitter::enabled

    If set to false, the emitter will cease emissions until it is set to true.

    Default value is true.
*/
/*!
    \qmlproperty real QtQuick.Particles::Emitter::emitRate

    Number of particles emitted per second.

    Default value is 10 particles per second.
*/
/*!
    \qmlproperty int QtQuick.Particles::Emitter::lifeSpan

    The time in milliseconds each emitted particle should last for.

    If you do not want particles to automatically die after a time, for example if
    you wish to dispose of them manually, set lifeSpan to Emitter.InfiniteLife.

    lifeSpans greater than or equal to 600000 (10 minutes) will be treated as infinite.
    Particles with lifeSpans less than or equal to 0 will start out dead.

    Default value is 1000 (one second).
*/
/*!
    \qmlproperty int QtQuick.Particles::Emitter::lifeSpanVariation

    Particle lifespans will vary by up to this much in either direction.

    Default value is 0.
*/

/*!
    \qmlproperty int QtQuick.Particles::Emitter::maximumEmitted

    The maximum number of particles at a time that this emitter will have alive.

    This can be set as a performance optimization (when using burst and pulse) or
    to stagger emissions.

    If this is set to a number below zero, then there is no maximum limit on the number
    of particles this emitter can have alive.

    The default value is -1.
*/
/*!
    \qmlproperty int QtQuick.Particles::Emitter::startTime

    If this value is set when the emitter is loaded, then it will emit particles from the
    past, up to startTime milliseconds ago. These will simulate as if they were emitted then,
    but will not have any affectors applied to them. Affectors will take effect from the present time.
*/
/*!
    \qmlproperty real QtQuick.Particles::Emitter::size

    The size in pixels of the particles at the start of their life.

    Default value is 16.
*/
/*!
    \qmlproperty real QtQuick.Particles::Emitter::endSize

    The size in pixels of the particles at the end of their life. Size will
    be linearly interpolated during the life of the particle from this value and
    size. If endSize is -1, then the size of the particle will remain constant at
    the starting size.

    Default value is -1.
*/
/*!
    \qmlproperty real QtQuick.Particles::Emitter::sizeVariation

    The size of a particle can vary by this much up or down from size/endSize. The same
    random addition is made to both size and endSize for a single particle.

    Default value is 0.
*/
/*!
    \qmlproperty StochasticDirection QtQuick.Particles::Emitter::velocity

    The starting velocity of the particles emitted.
*/
/*!
    \qmlproperty StochasticDirection QtQuick.Particles::Emitter::acceleration

    The starting acceleraton of the particles emitted.
*/
/*!
    \qmlproperty qreal QtQuick.Particles::Emitter::velocityFromMovement

    If this value is non-zero, then any movement of the emitter will provide additional
    starting velocity to the particles based on the movement. The additional vector will be the
    same angle as the emitter's movement, with a magnitude that is the magnitude of the emitters
    movement multiplied by velocityFromMovement.

    Default value is 0.
*/

/*!
    \qmlsignal QtQuick.Particles::Emitter::emitParticles(Array particles)

    This signal is emitted when particles are emitted. particles is a JavaScript
    array of Particle objects. You can modify particle attributes directly within the handler.

    Note that JavaScript is slower to execute, so it is not recommended to use this in
    high-volume particle systems.

    The corresponding handler is \c onEmitParticles.
*/

/*! \qmlmethod QtQuick.Particles::Emitter::burst(int count)

    Emits count particles from this emitter immediately.
*/

/*! \qmlmethod QtQuick.Particles::Emitter::burst(int count, int x, int y)

    Emits count particles from this emitter immediately. The particles are emitted
    as if the Emitter was positioned at x,y but all other properties are the same.
*/

/*! \qmlmethod QtQuick.Particles::Emitter::pulse(int duration)

    If the emitter is not enabled, enables it for duration milliseconds and then switches
    it back off.
*/

QQuickParticleEmitter::QQuickParticleEmitter(QQuickItem *parent) :
    QQuickItem(parent)
  , m_particlesPerSecond(10)
  , m_particleDuration(1000)
  , m_particleDurationVariation(0)
  , m_enabled(true)
  , m_system(0)
  , m_extruder(0)
  , m_defaultExtruder(0)
  , m_velocity(&m_nullVector)
  , m_acceleration(&m_nullVector)
  , m_particleSize(16)
  , m_particleEndSize(-1)
  , m_particleSizeVariation(0)
  , m_startTime(0)
  , m_overwrite(true)
  , m_pulseLeft(0)
  , m_maxParticleCount(-1)
  , m_velocity_from_movement(0)
  , m_reset_last(true)
  , m_last_timestamp(-1)
  , m_last_emission(0)
  , m_groupIdNeedRecalculation(false)
  , m_groupId(QQuickParticleGroupData::DefaultGroupID)

{
    //TODO: Reset velocity/acc back to null vector? Or allow null pointer?
    connect(this, SIGNAL(particlesPerSecondChanged(qreal)),
            this, SIGNAL(particleCountChanged()));
    connect(this, SIGNAL(particleDurationChanged(int)),
            this, SIGNAL(particleCountChanged()));
}

QQuickParticleEmitter::~QQuickParticleEmitter()
{
    if (m_defaultExtruder)
        delete m_defaultExtruder;
}

bool QQuickParticleEmitter::isEmitConnected()
{
    IS_SIGNAL_CONNECTED(this, QQuickParticleEmitter, emitParticles, (QQmlV4Handle));
}

void QQuickParticleEmitter::reclaculateGroupId() const
{
    if (!m_system) {
        m_groupId = QQuickParticleGroupData::InvalidID;
        return;
    }
    m_groupId = m_system->groupIds.value(group(), QQuickParticleGroupData::InvalidID);
    m_groupIdNeedRecalculation = m_groupId == QQuickParticleGroupData::InvalidID;
}

void QQuickParticleEmitter::componentComplete()
{
    if (!m_system && qobject_cast<QQuickParticleSystem*>(parentItem()))
        setSystem(qobject_cast<QQuickParticleSystem*>(parentItem()));
    if (m_system)
        m_system->finishRegisteringParticleEmitter(this);
    QQuickItem::componentComplete();
}

void QQuickParticleEmitter::setEnabled(bool arg)
{
    if (m_enabled != arg) {
        m_enabled = arg;
        emit enabledChanged(arg);
    }
}


QQuickParticleExtruder* QQuickParticleEmitter::effectiveExtruder()
{
    if (m_extruder)
        return m_extruder;
    if (!m_defaultExtruder)
        m_defaultExtruder = new QQuickParticleExtruder;
    return m_defaultExtruder;
}

void QQuickParticleEmitter::pulse(int milliseconds)
{
    if (!m_enabled)
        m_pulseLeft = milliseconds;
}

void QQuickParticleEmitter::burst(int num)
{
    m_burstQueue << qMakePair(num, QPointF(x(), y()));
}

void QQuickParticleEmitter::burst(int num, qreal x, qreal y)
{
    m_burstQueue << qMakePair(num, QPointF(x, y));
}

void QQuickParticleEmitter::setMaxParticleCount(int arg)
{
    if (m_maxParticleCount != arg) {
        if (arg < 0 && m_maxParticleCount >= 0){
            connect(this, SIGNAL(particlesPerSecondChanged(qreal)),
                    this, SIGNAL(particleCountChanged()));
            connect(this, SIGNAL(particleDurationChanged(int)),
                    this, SIGNAL(particleCountChanged()));
        }else if (arg >= 0 && m_maxParticleCount < 0){
            disconnect(this, SIGNAL(particlesPerSecondChanged(qreal)),
                    this, SIGNAL(particleCountChanged()));
            disconnect(this, SIGNAL(particleDurationChanged(int)),
                    this, SIGNAL(particleCountChanged()));
        }
        m_overwrite = arg < 0;
        m_maxParticleCount = arg;
        emit maximumEmittedChanged(arg);
        emit particleCountChanged();
    }
}

void QQuickParticleEmitter::setVelocityFromMovement(qreal t)
{
    if (t == m_velocity_from_movement)
        return;
    m_velocity_from_movement = t;
    emit velocityFromMovementChanged();
}

void QQuickParticleEmitter::reset()
{
    m_reset_last = true;
}

void QQuickParticleEmitter::emitWindow(int timeStamp)
{
    if (m_system == 0)
        return;
    if ((!m_enabled || m_particlesPerSecond <= 0)&& !m_pulseLeft && m_burstQueue.isEmpty()){
        m_reset_last = true;
        return;
    }

    if (m_reset_last) {
        m_last_emitter = m_last_last_emitter = QPointF(x(), y());
        if (m_last_timestamp == -1)
            m_last_timestamp = (timeStamp - m_startTime)/1000.;
        else
            m_last_timestamp = timeStamp/1000.;
        m_last_emission = m_last_timestamp;
        m_reset_last = false;
        m_emitCap = -1;
    }

    if (m_pulseLeft){
        m_pulseLeft -= timeStamp - m_last_timestamp * 1000.;
        if (m_pulseLeft < 0){
            if (!m_enabled)
                timeStamp += m_pulseLeft;
            m_pulseLeft = 0;
        }
    }
    qreal time = timeStamp / 1000.;
    qreal particleRatio = 1. / m_particlesPerSecond;
    qreal pt = m_last_emission;
    qreal maxLife = (m_particleDuration + m_particleDurationVariation)/1000.0;
    if (pt + maxLife < time)//We missed so much, that we should skip emiting particles that are dead by now
        pt = time - maxLife;

    qreal opt = pt; // original particle time
    qreal dt = time - m_last_timestamp; // timestamp delta...
    if (!dt)
        dt = 0.000001;

    // emitter difference since last...
    qreal dex = (x() - m_last_emitter.x());
    qreal dey = (y() - m_last_emitter.y());

    qreal ax = (m_last_last_emitter.x() + m_last_emitter.x()) / 2;
    qreal bx = m_last_emitter.x();
    qreal cx = (x() + m_last_emitter.x()) / 2;
    qreal ay = (m_last_last_emitter.y() + m_last_emitter.y()) / 2;
    qreal by = m_last_emitter.y();
    qreal cy = (y() + m_last_emitter.y()) / 2;

    qreal sizeAtEnd = m_particleEndSize >= 0 ? m_particleEndSize : m_particleSize;
    qreal emitter_x_offset = m_last_emitter.x() - x();
    qreal emitter_y_offset = m_last_emitter.y() - y();
    if (!m_burstQueue.isEmpty() && !m_pulseLeft && !m_enabled)//'outside time' emissions only
        pt = time;

    QList<QQuickParticleData*> toEmit;

    while ((pt < time && m_emitCap) || !m_burstQueue.isEmpty()) {
        //int pos = m_last_particle % m_particle_count;
        QQuickParticleData* datum = m_system->newDatum(m_system->groupIds[m_group], !m_overwrite);
        if (datum){//actually emit(otherwise we've been asked to skip this one)
            qreal t = 1 - (pt - opt) / dt;
            qreal vx =
              - 2 * ax * (1 - t)
              + 2 * bx * (1 - 2 * t)
              + 2 * cx * t;
            qreal vy =
              - 2 * ay * (1 - t)
              + 2 * by * (1 - 2 * t)
              + 2 * cy * t;


            // Particle timestamp
            datum->t = pt;
            datum->lifeSpan =
                    (m_particleDuration
                     + ((rand() % ((m_particleDurationVariation*2) + 1)) - m_particleDurationVariation))
                    / 1000.0;

            if (datum->lifeSpan >= m_system->maxLife){
                datum->lifeSpan = m_system->maxLife;
                if (m_emitCap == -1)
                    m_emitCap = particleCount();
                m_emitCap--;//emitCap keeps us from reemitting 'infinite' particles after their life. Unless you reset the emitter.
            }

            // Particle position
            QRectF boundsRect;
            if (!m_burstQueue.isEmpty()){
                boundsRect = QRectF(m_burstQueue.first().second.x() - x(), m_burstQueue.first().second.y() - y(),
                        width(), height());
            } else {
                boundsRect = QRectF(emitter_x_offset + dex * (pt - opt) / dt, emitter_y_offset + dey * (pt - opt) / dt
                              , width(), height());
            }
            QPointF newPos = effectiveExtruder()->extrude(boundsRect);
            datum->x = newPos.x();
            datum->y = newPos.y();

            // Particle velocity
            const QPointF &velocity = m_velocity->sample(newPos);
            datum->vx = velocity.x()
                    + m_velocity_from_movement * vx;
            datum->vy = velocity.y()
                    + m_velocity_from_movement * vy;

            // Particle acceleration
            const QPointF &accel = m_acceleration->sample(newPos);
            datum->ax = accel.x();
            datum->ay = accel.y();

            // Particle size
            float sizeVariation = -m_particleSizeVariation
                    + rand() / float(RAND_MAX) * m_particleSizeVariation * 2;

            float size = qMax((qreal)0.0 , m_particleSize + sizeVariation);
            float endSize = qMax((qreal)0.0 , sizeAtEnd + sizeVariation);

            datum->size = size;// * float(m_emitting);
            datum->endSize = endSize;// * float(m_emitting);

            toEmit << datum;
        }
        if (m_burstQueue.isEmpty()){
            pt += particleRatio;
        }else{
            m_burstQueue.first().first--;
            if (m_burstQueue.first().first <= 0)
                m_burstQueue.pop_front();
        }
    }

    foreach (QQuickParticleData* d, toEmit)
            m_system->emitParticle(d, this);

    if (isEmitConnected()) {
        QQmlEngine *qmlEngine = ::qmlEngine(this);
        QV4::ExecutionEngine *v4 = QV8Engine::getV4(qmlEngine->handle());
        QV4::Scope scope(v4);

        //Done after emitParticle so that the Painter::load is done first, this allows you to customize its static variables
        //We then don't need to request another reload, because the first reload isn't scheduled until we get back to the render thread
        QV4::ScopedArrayObject array(scope, v4->newArrayObject(toEmit.size()));
        QV4::ScopedValue v(scope);
        for (int i=0; i<toEmit.size(); i++)
            array->putIndexed(i, (v = toEmit[i]->v4Value(m_system)));

        emitParticles(QQmlV4Handle(array));//A chance for arbitrary JS changes
    }

    m_last_emission = pt;

    m_last_last_last_emitter = m_last_last_emitter;
    m_last_last_emitter = m_last_emitter;
    m_last_emitter = QPointF(x(), y());
    m_last_timestamp = time;
}


QT_END_NAMESPACE
