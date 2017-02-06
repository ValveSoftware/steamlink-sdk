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

#include "qabstractlight.h"
#include "qabstractlight_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender
{

/*!
 * \qmltype Light
 * \inqmlmodule Qt3D.Render
 * \instantiates Qt3DRender::QAbstractLight
 * \brief Encapsulate a QAbstractLight object in a Qt 3D scene.
 * \since 5.6
 */

/*!
    \enum QAbstractLight::Type

    This enum type identifies the particular type of light.
    \value PointLight
    \value DirectionalLight
    \value SpotLight
*/

QAbstractLightPrivate::QAbstractLightPrivate(QAbstractLight::Type type)
    : m_type(type)
    , m_shaderData(new QShaderData)
{
    m_shaderData->setProperty("type", type);
    m_shaderData->setProperty("color", QColor(Qt::white));
    m_shaderData->setProperty("intensity", 0.5f);
}

QAbstractLightPrivate::~QAbstractLightPrivate()
{
}

Qt3DCore::QNodeCreatedChangeBasePtr QAbstractLight::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QAbstractLightData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QAbstractLight);
    data.shaderDataId = qIdForNode(d->m_shaderData);
    return creationChange;
}

/*!
    \class Qt3DRender::QAbstractLight
    \inmodule Qt3DRender
    \brief Encapsulate a QAbstractLight object in a Qt 3D scene.
    \since 5.6
*/

/*! \internal */
QAbstractLight::QAbstractLight(QAbstractLightPrivate &dd, QNode *parent)
    : QComponent(dd, parent)
{
    Q_D(QAbstractLight);
    d->m_shaderData->setParent(this);
}

QAbstractLight::~QAbstractLight()
{
}

/*!
    \property Qt3DRender::QAbstractLight::type

    Holds the current QAbstractLight type.
*/
QAbstractLight::Type QAbstractLight::type() const
{
    Q_D(const QAbstractLight);
    return d->m_type;
}

/*!
 *  \property Qt3DRender::QAbstractLight::color
 *
 * Holds the current QAbstractLight color.
 */
QColor QAbstractLight::color() const
{
    Q_D(const QAbstractLight);
    return d->m_shaderData->property("color").value<QColor>();
}

void QAbstractLight::setColor(const QColor &c)
{
    Q_D(QAbstractLight);
    if (color() != c) {
        d->m_shaderData->setProperty("color", c);
        emit colorChanged(c);
    }
}

/*!
    \property Qt3DRender::QAbstractLight::intensity

    Holds the current QAbstractLight intensity.
*/
float QAbstractLight::intensity() const
{
    Q_D(const QAbstractLight);
    return d->m_shaderData->property("intensity").toFloat();
}

void QAbstractLight::setIntensity(float value)
{
    Q_D(QAbstractLight);
    if (intensity() != value) {
        d->m_shaderData->setProperty("intensity", value);
        emit intensityChanged(value);
    }
}

} // namespace Qt3DRender

QT_END_NAMESPACE
