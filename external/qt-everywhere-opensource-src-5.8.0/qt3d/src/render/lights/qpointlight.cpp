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

#include "qpointlight.h"
#include "qpointlight_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*
  Expected Shader struct

  \code

  struct PointLight
  {
   vec3 position;
   vec4 color;
   float intensity;
  };

  uniform PointLight pointLights[10];

  \endcode
 */

QPointLightPrivate::QPointLightPrivate()
    : QAbstractLightPrivate(QAbstractLight::PointLight)
{
    m_shaderData->setProperty("constantAttenuation", 1.0f);
    m_shaderData->setProperty("linearAttenuation", 0.0f);
    m_shaderData->setProperty("quadraticAttenuation", 0.0f);
}

/*!
  \class Qt3DRender::QPointLight
  \inmodule Qt3DRender
  \since 5.5
    \brief Encapsulate a Point Light object in a Qt 3D scene.

 */

/*!
    \qmltype PointLight
    \instantiates Qt3DRender::QPointLight
    \inherits AbstractLight
    \inqmlmodule Qt3D.Render
    \since 5.5
    \brief Encapsulate a Point Light object in a Qt 3D scene.
*/

/*!
  \fn Qt3DRender::QPointLight::QPointLight(Qt3DCore::QNode *parent)
  Constructs a new QPointLight with the specified \a parent.
 */
QPointLight::QPointLight(QNode *parent)
    : QAbstractLight(*new QPointLightPrivate, parent)
{
}

/*! \internal */
QPointLight::~QPointLight()
{
}

/*! \internal */
QPointLight::QPointLight(QPointLightPrivate &dd, QNode *parent)
    : QAbstractLight(dd, parent)
{
}

/*!
  \qmlproperty float Qt3D.Render::PointLight::constantAttenuation
    Specifies the constant attenuation of the point light
*/

/*!
  \property Qt3DRender::QPointLight::constantAttenuation
    Specifies the constant attenuation of the point light
 */
float QPointLight::constantAttenuation() const
{
    Q_D(const QPointLight);
    return d->m_shaderData->property("constantAttenuation").toFloat();
}

void QPointLight::setConstantAttenuation(float value)
{
    Q_D(QPointLight);
    if (constantAttenuation() != value) {
        d->m_shaderData->setProperty("constantAttenuation", value);
        emit constantAttenuationChanged(value);
    }
}

/*!
  \qmlproperty float Qt3D.Render::PointLight::linearAttenuation
    Specifies the linear attenuation of the point light
*/

/*!
  \property Qt3DRender::QPointLight::linearAttenuation
    Specifies the linear attenuation of the point light
 */
float QPointLight::linearAttenuation() const
{
    Q_D(const QPointLight);
    return d->m_shaderData->property("linearAttenuation").toFloat();
}

void QPointLight::setLinearAttenuation(float value)
{
    Q_D(QPointLight);
    if (linearAttenuation() != value) {
        d->m_shaderData->setProperty("linearAttenuation", value);
        emit linearAttenuationChanged(value);
    }
}

/*!
  \qmlproperty float Qt3D.Render::PointLight::quadraticAttenuation
    Specifies the quadratic attenuation of the point light
*/

/*!
  \property Qt3DRender::QPointLight::quadraticAttenuation
    Specifies the quadratic attenuation of the point light
 */
float QPointLight::quadraticAttenuation() const
{
    Q_D(const QPointLight);
    return d->m_shaderData->property("quadraticAttenuation").toFloat();
}

void QPointLight::setQuadraticAttenuation(float value)
{
    Q_D(QPointLight);
    if (quadraticAttenuation() != value) {
        d->m_shaderData->setProperty("quadraticAttenuation", value);
        emit quadraticAttenuationChanged(value);
    }
}

} // namespace Qt3DRender

QT_END_NAMESPACE
