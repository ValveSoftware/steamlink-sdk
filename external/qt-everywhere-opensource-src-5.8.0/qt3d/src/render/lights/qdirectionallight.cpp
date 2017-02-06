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

#include "qdirectionallight.h"
#include "qdirectionallight_p.h"
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*
 *
 * Expected Shader struct
 *
 * \code
 *
 * struct DirectionalLight
 * {
 *  vec3 position;
 *  vec3 worldDirection;
 *  vec4 color;
 *  float intensity;
 * };
 *
 * uniform DirectionalLight directionalLights[10];
 *
 * \endcode
 */

QDirectionalLightPrivate::QDirectionalLightPrivate()
    : QAbstractLightPrivate(QAbstractLight::DirectionalLight)
{
    m_shaderData->setProperty("direction", QVector3D(0.0f, -1.0f, 0.0f));
}

/*!
  \class Qt3DRender::QDirectionalLight
  \inmodule Qt3DRender
  \since 5.7
  \brief Encapsulate a Directional Light object in a Qt 3D scene.

 */

/*!
    \qmltype DirectionalLight
    \instantiates Qt3DRender::QDirectionalLight
    \inherits AbstractLight
    \inqmlmodule Qt3D.Render
    \since 5.7
    \brief Encapsulate a Directional Light object in a Qt 3D scene.

*/

/*!
  \fn Qt3DRender::QDirectionalLight::QDirectionalLight(Qt3DCore::QNode *parent)
  Constructs a new QDirectionalLight with the specified \a parent.
 */
QDirectionalLight::QDirectionalLight(QNode *parent)
    : QAbstractLight(*new QDirectionalLightPrivate, parent)
{
}

/*! \internal */
QDirectionalLight::~QDirectionalLight()
{
}

/*! \internal */
QDirectionalLight::QDirectionalLight(QDirectionalLightPrivate &dd, QNode *parent)
    : QAbstractLight(dd, parent)
{
}

/*!
  \qmlproperty vector3d Qt3D.Render::DirectionalLight::worldDirection
    Specifies the world direction of the directional light
*/

/*!
  \property Qt3DRender::QDirectionalLight::worldDirection
    Specifies the world direction of the directional light
 */
void QDirectionalLight::setWorldDirection(const QVector3D &direction)
{
    Q_D(QDirectionalLight);
    if (worldDirection() != direction) {
        d->m_shaderData->setProperty("direction", direction);
        emit worldDirectionChanged(direction);
    }
}

QVector3D QDirectionalLight::worldDirection() const
{
    Q_D(const QDirectionalLight);
    return d->m_shaderData->property("direction").value<QVector3D>();
}

} // namespace Qt3DRender

QT_END_NAMESPACE
