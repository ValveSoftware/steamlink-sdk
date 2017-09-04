/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qenvironmentlight.h"
#include "qenvironmentlight_p.h"
#include "qabstracttexture.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender
{

/*!
 * \qmltype EnvironmentLight
 * \inqmlmodule Qt3D.Render
 * \instantiates Qt3DRender::QEnvironmentLight
 * \brief Encapsulate an environment light object in a Qt 3D scene.
 * \since 5.9
 */

QEnvironmentLightPrivate::QEnvironmentLightPrivate()
    : m_shaderData(new QShaderData)
    , m_irradiance(nullptr)
    , m_specular(nullptr)
{
}

QEnvironmentLightPrivate::~QEnvironmentLightPrivate()
{
}

Qt3DCore::QNodeCreatedChangeBasePtr QEnvironmentLight::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QEnvironmentLightData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QEnvironmentLight);
    data.shaderDataId = qIdForNode(d->m_shaderData);
    return creationChange;
}

/*!
    \class Qt3DRender::QEnvironmentLight
    \inmodule Qt3DRender
    \brief Encapsulate an environment light object in a Qt 3D scene.
    \since 5.9
*/

QEnvironmentLight::QEnvironmentLight(Qt3DCore::QNode *parent)
    : QComponent(*new QEnvironmentLightPrivate, parent)
{
    Q_D(QEnvironmentLight);
    d->m_shaderData->setParent(this);
}

/*! \internal */
QEnvironmentLight::QEnvironmentLight(QEnvironmentLightPrivate &dd, QNode *parent)
    : QComponent(dd, parent)
{
    Q_D(QEnvironmentLight);
    d->m_shaderData->setParent(this);
}

QEnvironmentLight::~QEnvironmentLight()
{
}

/*!
    \qmlproperty Texture EnvironmentLight::irradiance

    Holds the current environment irradiance map texture.

    By default, the environment irradiance texture is null.
*/

/*!
    \property QEnvironmentLight::irradiance

    Holds the current environment irradiance map texture.

    By default, the environment irradiance texture is null.
*/
QAbstractTexture *QEnvironmentLight::irradiance() const
{
    Q_D(const QEnvironmentLight);
    return d->m_irradiance;
}

/*!
    \qmlproperty Texture EnvironmentLight::specular

    Holds the current environment specular map texture.

    By default, the environment specular texture is null.
*/

/*!
    \property QEnvironmentLight::specular

    Holds the current environment specular map texture.

    By default, the environment specular texture is null.
*/
QAbstractTexture *QEnvironmentLight::specular() const
{
    Q_D(const QEnvironmentLight);
    return d->m_specular;
}

void QEnvironmentLight::setIrradiance(QAbstractTexture *i)
{
    Q_D(QEnvironmentLight);
    if (irradiance() == i)
        return;

    if (irradiance())
        d->unregisterDestructionHelper(irradiance());

    if (i && !i->parent())
        i->setParent(this);

    d->m_irradiance = i;
    d->m_shaderData->setProperty("irradiance", QVariant::fromValue(i));

    if (i)
        d->registerDestructionHelper(i, &QEnvironmentLight::setIrradiance, i);

    emit irradianceChanged(i);
}

void QEnvironmentLight::setSpecular(QAbstractTexture *s)
{
    Q_D(QEnvironmentLight);
    if (specular() == s)
        return;

    if (irradiance())
        d->unregisterDestructionHelper(specular());

    if (s && !s->parent())
        s->setParent(this);

    d->m_specular = s;
    d->m_shaderData->setProperty("specular", QVariant::fromValue(s));

    if (s)
        d->registerDestructionHelper(s, &QEnvironmentLight::setSpecular, s);

    emit specularChanged(s);
}

} // namespace Qt3DRender

QT_END_NAMESPACE
