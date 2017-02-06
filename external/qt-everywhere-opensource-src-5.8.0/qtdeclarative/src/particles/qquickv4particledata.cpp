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

#include <math.h>
#include "qquickv4particledata_p.h"
#include "qquickparticlesystem_p.h"//for QQuickParticleData
#include <QDebug>
#include <private/qv4engine_p.h>
#include <private/qv4functionobject_p.h>
#include <QtCore/private/qnumeric_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Particle
    \inqmlmodule QtQuick.Particles
    \brief Represents particles manipulated by emitters and affectors
    \ingroup qtquick-particles

    Particle elements are always managed internally by the ParticleSystem and cannot be created in QML.
    However, sometimes they are exposed via signals so as to allow arbitrary changes to the particle state
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::initialX
    The x coordinate of the particle at the beginning of its lifetime.

    The method of simulation prefers to have the initial values changed, rather
    than determining and changing the value at a given time. Change initial
    values in CustomEmitters instead of the current values.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::initialVX
    The x velocity of the particle at the beginning of its lifetime.

    The method of simulation prefers to have the initial values changed, rather
    than determining and changing the value at a given time. Change initial
    values in CustomEmitters instead of the current values.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::initialAX
    The x acceleration of the particle at the beginning of its lifetime.

    The method of simulation prefers to have the initial values changed, rather
    than determining and changing the value at a given time. Change initial
    values in CustomEmitters instead of the current values.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::initialY
    The y coordinate of the particle at the beginning of its lifetime.

    The method of simulation prefers to have the initial values changed, rather
    than determining and changing the value at a given time. Change initial
    values in CustomEmitters instead of the current values.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::initialVY
    The y velocity of the particle at the beginning of its lifetime.

    The method of simulation prefers to have the initial values changed, rather
    than determining and changing the value at a given time. Change initial
    values in CustomEmitters instead of the current values.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::initialAY
    The y acceleration of the particle at the beginning of its lifetime.

    The method of simulation prefers to have the initial values changed, rather
    than determining and changing the value at a given time. Change initial
    values in CustomEmitters instead of the current values.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::x
    The current x coordinate of the particle.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::vx
    The current x velocity of the particle.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::ax
    The current x acceleration of the particle.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::y
    The current y coordinate of the particle.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::vy
    The current y velocity of the particle.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::ay
    The current y acceleration of the particle.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::t
    The time, in seconds since the beginning of the simulation, that the particle was born.
*/


/*!
    \qmlproperty real QtQuick.Particles::Particle::startSize
    The size in pixels that the particle image is at the start
    of its life.
*/


/*!
    \qmlproperty real QtQuick.Particles::Particle::endSize
    The size in pixels that the particle image is at the end
    of its life. If this value is less than 0, then it is
    disregarded and the particle will have its startSize for the
    entire lifetime.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::lifeSpan
    The time in seconds that the particle will live for.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::rotation
    Degrees clockwise that the particle image is rotated at
    the beginning of its life.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::rotationVelocity
    Degrees clockwise per second that the particle image is rotated at while alive.
*/
/*!
    \qmlproperty bool QtQuick.Particles::Particle::autoRotate
    If autoRotate is true, then the particle's rotation will be
    set so that it faces the direction of travel, plus any
    rotation from the rotation or rotationVelocity properties.
*/

/*!
    \qmlproperty bool QtQuick.Particles::Particle::update

    Inside an Affector, the changes made to the particle will only be
    applied if update is set to true.
*/
/*!
    \qmlproperty real QtQuick.Particles::Particle::xDeformationVectorX

    The x component of the deformation vector along the X axis. ImageParticle
    can draw particles across non-square shapes. It will draw the texture rectangle
    across the parallelogram drawn with the x and y deformation vectors.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::yDeformationVectorX

    The y component of the deformation vector along the X axis. ImageParticle
    can draw particles across non-square shapes. It will draw the texture rectangle
    across the parallelogram drawn with the x and y deformation vectors.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::xDeformationVectorY

    The x component of the deformation vector along the X axis. ImageParticle
    can draw particles across non-square shapes. It will draw the texture rectangle
    across the parallelogram drawn with the x and y deformation vectors.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::yDeformationVectorY

    The y component of the deformation vector along the Y axis. ImageParticle
    can draw particles across non-square shapes. It will draw the texture rectangle
    across the parallelogram drawn with the x and y deformation vectors.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::red

    ImageParticle can draw colorized particles. When it does so, red is used
    as the red channel of the color applied to the source image.

    Values are from 0.0 to 1.0.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::green

    ImageParticle can draw colorized particles. When it does so, green is used
    as the green channel of the color applied to the source image.

    Values are from 0.0 to 1.0.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::blue

    ImageParticle can draw colorized particles. When it does so, blue is used
    as the blue channel of the color applied to the source image.

    Values are from 0.0 to 1.0.
*/

/*!
    \qmlproperty real QtQuick.Particles::Particle::alpha

    ImageParticle can draw colorized particles. When it does so, alpha is used
    as the alpha channel of the color applied to the source image.

    Values are from 0.0 to 1.0.
*/
/*!
    \qmlproperty real QtQuick.Particles::Particle::lifeLeft
    The time in seconds that the particle has left to live at
    the current point in time.
*/
/*!
    \qmlproperty real QtQuick.Particles::Particle::currentSize
    The currentSize of the particle, interpolating between startSize and endSize based on the currentTime.
*/



//### Particle data handles are not locked to within certain scopes like QQuickContext2D, but there's no way to reload either...
struct QV4ParticleData : public QV4::Object
{
    struct Data : QV4::Object::Data {
        void init(QQuickParticleData *datum, QQuickParticleSystem* particleSystem)
        {
            Object::init();
            this->datum = datum;
            this->particleSystem = particleSystem;
        }
        QQuickParticleData* datum;//TODO: Guard needed?
        QQuickParticleSystem* particleSystem;
    };
    V4_OBJECT(QV4::Object)
};

DEFINE_OBJECT_VTABLE(QV4ParticleData);

class QV4ParticleDataDeletable : public QV8Engine::Deletable
{
public:
    QV4ParticleDataDeletable(QV4::ExecutionEngine *engine);
    ~QV4ParticleDataDeletable();

    QV4::PersistentValue proto;
};

static QV4::ReturnedValue particleData_discard(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QV4ParticleData> r(scope, ctx->thisObject());

    if (!r || !r->d()->datum)
        return ctx->engine()->throwError(QStringLiteral("Not a valid ParticleData object"));

    r->d()->datum->lifeSpan = 0; //Don't kill(), because it could still be in the middle of being created
    return QV4::Encode::undefined();
}

static QV4::ReturnedValue particleData_lifeLeft(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QV4ParticleData> r(scope, ctx->thisObject());

    if (!r || !r->d()->datum)
        return ctx->engine()->throwError(QStringLiteral("Not a valid ParticleData object"));

    return QV4::Encode(r->d()->datum->lifeLeft(r->d()->particleSystem));
}

static QV4::ReturnedValue particleData_curSize(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QV4ParticleData> r(scope, ctx->thisObject());

    if (!r || !r->d()->datum)
        return ctx->engine()->throwError(QStringLiteral("Not a valid ParticleData object"));

    return QV4::Encode(r->d()->datum->curSize(r->d()->particleSystem));
}
#define COLOR_GETTER_AND_SETTER(VAR, NAME) static QV4::ReturnedValue particleData_get_ ## NAME (QV4::CallContext *ctx) \
{ \
    QV4::Scope scope(ctx); \
    QV4::Scoped<QV4ParticleData> r(scope, ctx->thisObject()); \
    if (!r || !r->d()->datum) \
        ctx->engine()->throwError(QStringLiteral("Not a valid ParticleData object")); \
\
    return QV4::Encode((r->d()->datum->color. VAR )/255.0);\
}\
\
static QV4::ReturnedValue particleData_set_ ## NAME (QV4::CallContext *ctx)\
{\
    QV4::Scope scope(ctx); \
    QV4::Scoped<QV4ParticleData> r(scope, ctx->thisObject()); \
    if (!r || !r->d()->datum)\
        ctx->engine()->throwError(QStringLiteral("Not a valid ParticleData object"));\
\
    double d = ctx->argc() ? ctx->args()[0].toNumber() : 0; \
    r->d()->datum->color. VAR = qMin(255, qMax(0, (int)::floor(d * 255.0)));\
    return QV4::Encode::undefined(); \
}


#define SEMIBOOL_GETTER_AND_SETTER(VARIABLE) static QV4::ReturnedValue particleData_get_ ## VARIABLE (QV4::CallContext *ctx) \
{ \
    QV4::Scope scope(ctx); \
    QV4::Scoped<QV4ParticleData> r(scope, ctx->thisObject()); \
    if (!r || !r->d()->datum) \
        ctx->engine()->throwError(QStringLiteral("Not a valid ParticleData object")); \
\
    return QV4::Encode(r->d()->datum-> VARIABLE);\
}\
\
static QV4::ReturnedValue particleData_set_ ## VARIABLE (QV4::CallContext *ctx)\
{\
    QV4::Scope scope(ctx); \
    QV4::Scoped<QV4ParticleData> r(scope, ctx->thisObject()); \
    if (!r || !r->d()->datum)\
        ctx->engine()->throwError(QStringLiteral("Not a valid ParticleData object"));\
\
    r->d()->datum-> VARIABLE = (ctx->argc() && ctx->args()[0].toBoolean()) ? 1.0 : 0.0;\
    return QV4::Encode::undefined(); \
}

#define FLOAT_GETTER_AND_SETTER(VARIABLE) static QV4::ReturnedValue particleData_get_ ## VARIABLE (QV4::CallContext *ctx) \
{ \
    QV4::Scope scope(ctx); \
    QV4::Scoped<QV4ParticleData> r(scope, ctx->thisObject()); \
    if (!r || !r->d()->datum) \
        ctx->engine()->throwError(QStringLiteral("Not a valid ParticleData object")); \
\
    return QV4::Encode(r->d()->datum-> VARIABLE);\
}\
\
static QV4::ReturnedValue particleData_set_ ## VARIABLE (QV4::CallContext *ctx)\
{\
    QV4::Scope scope(ctx); \
    QV4::Scoped<QV4ParticleData> r(scope, ctx->thisObject()); \
    if (!r || !r->d()->datum)\
        ctx->engine()->throwError(QStringLiteral("Not a valid ParticleData object"));\
\
    r->d()->datum-> VARIABLE = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();\
    return QV4::Encode::undefined(); \
}

#define FAKE_FLOAT_GETTER_AND_SETTER(VARIABLE, GETTER, SETTER) static QV4::ReturnedValue particleData_get_ ## VARIABLE (QV4::CallContext *ctx) \
{ \
    QV4::Scope scope(ctx); \
    QV4::Scoped<QV4ParticleData> r(scope, ctx->thisObject()); \
    if (!r || !r->d()->datum) \
        ctx->engine()->throwError(QStringLiteral("Not a valid ParticleData object")); \
\
    return QV4::Encode(r->d()->datum-> GETTER (r->d()->particleSystem));\
}\
\
static QV4::ReturnedValue particleData_set_ ## VARIABLE (QV4::CallContext *ctx)\
{\
    QV4::Scope scope(ctx); \
    QV4::Scoped<QV4ParticleData> r(scope, ctx->thisObject()); \
    if (!r || !r->d()->datum)\
        ctx->engine()->throwError(QStringLiteral("Not a valid ParticleData object"));\
\
    r->d()->datum-> SETTER (ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan(), r->d()->particleSystem);\
    return QV4::Encode::undefined(); \
}

#define REGISTER_ACCESSOR(PROTO, ENGINE, VARIABLE, NAME) \
    PROTO ->defineAccessorProperty( QStringLiteral( #NAME ), particleData_get_ ## VARIABLE , particleData_set_ ## VARIABLE )

COLOR_GETTER_AND_SETTER(r, red)
COLOR_GETTER_AND_SETTER(g, green)
COLOR_GETTER_AND_SETTER(b, blue)
COLOR_GETTER_AND_SETTER(a, alpha)
SEMIBOOL_GETTER_AND_SETTER(autoRotate)
SEMIBOOL_GETTER_AND_SETTER(update)
FLOAT_GETTER_AND_SETTER(x)
FLOAT_GETTER_AND_SETTER(y)
FLOAT_GETTER_AND_SETTER(t)
FLOAT_GETTER_AND_SETTER(lifeSpan)
FLOAT_GETTER_AND_SETTER(size)
FLOAT_GETTER_AND_SETTER(endSize)
FLOAT_GETTER_AND_SETTER(vx)
FLOAT_GETTER_AND_SETTER(vy)
FLOAT_GETTER_AND_SETTER(ax)
FLOAT_GETTER_AND_SETTER(ay)
FLOAT_GETTER_AND_SETTER(xx)
FLOAT_GETTER_AND_SETTER(xy)
FLOAT_GETTER_AND_SETTER(yx)
FLOAT_GETTER_AND_SETTER(yy)
FLOAT_GETTER_AND_SETTER(rotation)
FLOAT_GETTER_AND_SETTER(rotationVelocity)
FLOAT_GETTER_AND_SETTER(animIdx)
FLOAT_GETTER_AND_SETTER(frameDuration)
FLOAT_GETTER_AND_SETTER(frameAt)
FLOAT_GETTER_AND_SETTER(frameCount)
FLOAT_GETTER_AND_SETTER(animT)
FLOAT_GETTER_AND_SETTER(r)
FAKE_FLOAT_GETTER_AND_SETTER(curX, curX, setInstantaneousX)
FAKE_FLOAT_GETTER_AND_SETTER(curVX, curVX, setInstantaneousVX)
FAKE_FLOAT_GETTER_AND_SETTER(curAX, curAX, setInstantaneousAX)
FAKE_FLOAT_GETTER_AND_SETTER(curY, curY, setInstantaneousY)
FAKE_FLOAT_GETTER_AND_SETTER(curVY, curVY, setInstantaneousVY)
FAKE_FLOAT_GETTER_AND_SETTER(curAY, curAY, setInstantaneousAY)

QV4ParticleDataDeletable::QV4ParticleDataDeletable(QV4::ExecutionEngine *v4)
{
    QV4::Scope scope(v4);
    QV4::ScopedObject p(scope, v4->newObject());

    p->defineDefaultProperty(QStringLiteral("discard"), particleData_discard);
    p->defineDefaultProperty(QStringLiteral("lifeLeft"), particleData_lifeLeft);
    p->defineDefaultProperty(QStringLiteral("currentSize"), particleData_curSize);

    REGISTER_ACCESSOR(p, v4, x, initialX);
    REGISTER_ACCESSOR(p, v4, y, initialY);
    REGISTER_ACCESSOR(p, v4, t, t);
    REGISTER_ACCESSOR(p, v4, lifeSpan, lifeSpan);
    REGISTER_ACCESSOR(p, v4, size, startSize);
    REGISTER_ACCESSOR(p, v4, endSize, endSize);
    REGISTER_ACCESSOR(p, v4, vx, initialVX);
    REGISTER_ACCESSOR(p, v4, vy, initialVY);
    REGISTER_ACCESSOR(p, v4, ax, initialAX);
    REGISTER_ACCESSOR(p, v4, ay, initialAY);
    REGISTER_ACCESSOR(p, v4, xx, xDeformationVectorX);
    REGISTER_ACCESSOR(p, v4, xy, xDeformationVectorY);
    REGISTER_ACCESSOR(p, v4, yx, yDeformationVectorX);
    REGISTER_ACCESSOR(p, v4, yy, yDeformationVectorY);
    REGISTER_ACCESSOR(p, v4, rotation, rotation);
    REGISTER_ACCESSOR(p, v4, rotationVelocity, rotationVelocity);
    REGISTER_ACCESSOR(p, v4, autoRotate, autoRotate);
    REGISTER_ACCESSOR(p, v4, animIdx, animationIndex);
    REGISTER_ACCESSOR(p, v4, frameDuration, frameDuration);
    REGISTER_ACCESSOR(p, v4, frameAt, frameAt);
    REGISTER_ACCESSOR(p, v4, frameCount, frameCount);
    REGISTER_ACCESSOR(p, v4, animT, animationT);
    REGISTER_ACCESSOR(p, v4, r, r);
    REGISTER_ACCESSOR(p, v4, update, update);
    REGISTER_ACCESSOR(p, v4, curX, x);
    REGISTER_ACCESSOR(p, v4, curVX, vx);
    REGISTER_ACCESSOR(p, v4, curAX, ax);
    REGISTER_ACCESSOR(p, v4, curY, y);
    REGISTER_ACCESSOR(p, v4, curVY, vy);
    REGISTER_ACCESSOR(p, v4, curAY, ay);
    REGISTER_ACCESSOR(p, v4, red, red);
    REGISTER_ACCESSOR(p, v4, green, green);
    REGISTER_ACCESSOR(p, v4, blue, blue);
    REGISTER_ACCESSOR(p, v4, alpha, alpha);

    proto = p;
}

QV4ParticleDataDeletable::~QV4ParticleDataDeletable()
{
}

V4_DEFINE_EXTENSION(QV4ParticleDataDeletable, particleV4Data);


QQuickV4ParticleData::QQuickV4ParticleData(QV8Engine* engine, QQuickParticleData* datum, QQuickParticleSystem *system)
{
    if (!engine || !datum)
        return;

    QV4::ExecutionEngine *v4 = QV8Engine::getV4(engine);
    QV4::Scope scope(v4);
    QV4ParticleDataDeletable *d = particleV4Data(scope.engine);
    QV4::ScopedObject o(scope, v4->memoryManager->allocObject<QV4ParticleData>(datum, system));
    QV4::ScopedObject p(scope, d->proto.value());
    o->setPrototype(p);
    m_v4Value = o;
}

QQuickV4ParticleData::~QQuickV4ParticleData()
{
}

QQmlV4Handle QQuickV4ParticleData::v4Value()
{
    return QQmlV4Handle(m_v4Value.value());
}

QT_END_NAMESPACE
