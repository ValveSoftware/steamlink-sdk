/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qspotlight.h"
#include "qspotlight_p.h"
#include "shaderdata_p.h"
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {


/*
  Expected Shader struct

  \code

  struct SpotLight
  {
   vec3 position;
   vec3 localDirection;
   vec4 color;
   float intensity;
   float cutOffAngle;
  };

  uniform SpotLight spotLights[10];

  \endcode
 */

QSpotLightPrivate::QSpotLightPrivate()
    : QAbstractLightPrivate(QAbstractLight::SpotLight)
{
    m_shaderData->setProperty("constantAttenuation", 1.0f);
    m_shaderData->setProperty("linearAttenuation", 0.0f);
    m_shaderData->setProperty("quadraticAttenuation", 0.0f);
    m_shaderData->setProperty("direction", QVector3D(0.0f, -1.0f, 0.0f));
    m_shaderData->setProperty("directionTransformed", Render::ShaderData::ModelToWorldDirection);
    m_shaderData->setProperty("cutOffAngle", 45.0f);
}

/*!
  \class Qt3DRender::QSpotLight
  \inmodule Qt3DRender
  \since 5.5
  \brief Encapsulate a Spot Light object in a Qt 3D scene.

 */

/*!
    \qmltype SpotLight
    \instantiates Qt3DRender::QSpotLight
    \inherits AbstractLight
    \inqmlmodule Qt3D.Render
    \since 5.5
    \brief Encapsulate a Spot Light object in a Qt 3D scene.

*/

/*!
  \fn Qt3DRender::QSpotLight::QSpotLight(Qt3DCore::QNode *parent)
  Constructs a new QSpotLight with the specified \a parent.
 */
QSpotLight::QSpotLight(QNode *parent)
    : QAbstractLight(*new QSpotLightPrivate, parent)
{
}

/*! \internal */
QSpotLight::~QSpotLight()
{
}

/*! \internal */
QSpotLight::QSpotLight(QSpotLightPrivate &dd, QNode *parent)
    : QAbstractLight(dd, parent)
{
}

/*!
  \qmlproperty float Qt3D.Render::SpotLight::constantAttenuation
    Specifies the constant attenuation of the spot light
*/

/*!
  \property Qt3DRender::QSpotLight::constantAttenuation
    Specifies the constant attenuation of the spot light
 */
float QSpotLight::constantAttenuation() const
{
    Q_D(const QSpotLight);
    return d->m_shaderData->property("constantAttenuation").toFloat();
}

void QSpotLight::setConstantAttenuation(float value)
{
    Q_D(QSpotLight);
    if (constantAttenuation() != value) {
        d->m_shaderData->setProperty("constantAttenuation", value);
        emit constantAttenuationChanged(value);
    }
}

/*!
  \qmlproperty float Qt3D.Render::SpotLight::linearAttenuation
    Specifies the linear attenuation of the spot light
*/

/*!
  \property Qt3DRender::QSpotLight::linearAttenuation
    Specifies the linear attenuation of the spot light
 */
float QSpotLight::linearAttenuation() const
{
    Q_D(const QSpotLight);
    return d->m_shaderData->property("linearAttenuation").toFloat();
}

void QSpotLight::setLinearAttenuation(float value)
{
    Q_D(QSpotLight);
    if (linearAttenuation() != value) {
        d->m_shaderData->setProperty("linearAttenuation", value);
        emit linearAttenuationChanged(value);
    }
}

/*!
  \qmlproperty float Qt3D.Render::SpotLight::quadraticAttenuation
    Specifies the quadratic attenuation of the spot light
*/

/*!
  \property Qt3DRender::QSpotLight::quadraticAttenuation
    Specifies the quadratic attenuation of the spot light
 */
float QSpotLight::quadraticAttenuation() const
{
    Q_D(const QSpotLight);
    return d->m_shaderData->property("quadraticAttenuation").toFloat();
}

void QSpotLight::setQuadraticAttenuation(float value)
{
    Q_D(QSpotLight);
    if (quadraticAttenuation() != value) {
        d->m_shaderData->setProperty("quadraticAttenuation", value);
        emit quadraticAttenuationChanged(value);
    }
}

/*!
  \qmlproperty vector3d Qt3D.Render::SpotLight::localDirection
    Specifies the local direction of the spot light
*/

/*!
  \property Qt3DRender::QSpotLight::localDirection
    Specifies the local direction of the spot light
 */
QVector3D QSpotLight::localDirection() const
{
    Q_D(const QSpotLight);
    return d->m_shaderData->property("direction").value<QVector3D>();
}

/*!
  \qmlproperty float Qt3D.Render::SpotLight::cutOffAngle
    Specifies the cut off angle of the spot light
*/

/*!
  \property Qt3DRender::QSpotLight::cutOffAngle
    Specifies the cut off angle of the spot light
 */
float QSpotLight::cutOffAngle() const
{
    Q_D(const QSpotLight);
    return d->m_shaderData->property("cutOffAngle").toFloat();
}

void QSpotLight::setLocalDirection(const QVector3D &direction)
{
    Q_D(QSpotLight);
    if (localDirection() != direction) {
        const QVector3D dir = direction.normalized();
        d->m_shaderData->setProperty("direction", dir);
        emit localDirectionChanged(dir);
    }
}

void QSpotLight::setCutOffAngle(float value)
{
    Q_D(QSpotLight);
    if (cutOffAngle() != value) {
        d->m_shaderData->setProperty("cutOffAngle", value);
        emit cutOffAngleChanged(value);
    }
}

} // namespace Qt3DRender

QT_END_NAMESPACE
