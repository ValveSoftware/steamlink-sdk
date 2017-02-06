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

#include "qphongmaterial.h"
#include "qphongmaterial_p.h"
#include <Qt3DRender/qfilterkey.h>
#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qeffect.h>
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/qshaderprogram.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qrenderpass.h>
#include <Qt3DRender/qgraphicsapifilter.h>
#include <QUrl>
#include <QVector3D>
#include <QVector4D>

QT_BEGIN_NAMESPACE

using namespace Qt3DRender;

namespace Qt3DExtras {

QPhongMaterialPrivate::QPhongMaterialPrivate()
    : QMaterialPrivate()
    , m_phongEffect(new QEffect())
    , m_ambientParameter(new QParameter(QStringLiteral("ka"), QColor::fromRgbF(0.05f, 0.05f, 0.05f, 1.0f)))
    , m_diffuseParameter(new QParameter(QStringLiteral("kd"), QColor::fromRgbF(0.7f, 0.7f, 0.7f, 1.0f)))
    , m_specularParameter(new QParameter(QStringLiteral("ks"), QColor::fromRgbF(0.01f, 0.01f, 0.01f, 1.0f)))
    , m_shininessParameter(new QParameter(QStringLiteral("shininess"), 150.0f))
    , m_phongGL3Technique(new QTechnique())
    , m_phongGL2Technique(new QTechnique())
    , m_phongES2Technique(new QTechnique())
    , m_phongGL3RenderPass(new QRenderPass())
    , m_phongGL2RenderPass(new QRenderPass())
    , m_phongES2RenderPass(new QRenderPass())
    , m_phongGL3Shader(new QShaderProgram())
    , m_phongGL2ES2Shader(new QShaderProgram())
    , m_filterKey(new QFilterKey)
{
}

void QPhongMaterialPrivate::init()
{
    connect(m_ambientParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QPhongMaterialPrivate::handleAmbientChanged);
    connect(m_diffuseParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QPhongMaterialPrivate::handleDiffuseChanged);
    connect(m_specularParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QPhongMaterialPrivate::handleSpecularChanged);
    connect(m_shininessParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QPhongMaterialPrivate::handleShininessChanged);


    m_phongGL3Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/phong.vert"))));
    m_phongGL3Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/phong.frag"))));
    m_phongGL2ES2Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/phong.vert"))));
    m_phongGL2ES2Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/phong.frag"))));

    m_phongGL3Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_phongGL3Technique->graphicsApiFilter()->setMajorVersion(3);
    m_phongGL3Technique->graphicsApiFilter()->setMinorVersion(1);
    m_phongGL3Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::CoreProfile);

    m_phongGL2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_phongGL2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_phongGL2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_phongGL2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    m_phongES2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGLES);
    m_phongES2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_phongES2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_phongES2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    m_phongGL3RenderPass->setShaderProgram(m_phongGL3Shader);
    m_phongGL2RenderPass->setShaderProgram(m_phongGL2ES2Shader);
    m_phongES2RenderPass->setShaderProgram(m_phongGL2ES2Shader);

    m_phongGL3Technique->addRenderPass(m_phongGL3RenderPass);
    m_phongGL2Technique->addRenderPass(m_phongGL2RenderPass);
    m_phongES2Technique->addRenderPass(m_phongES2RenderPass);

    Q_Q(QPhongMaterial);
    m_filterKey->setParent(q);
    m_filterKey->setName(QStringLiteral("renderingStyle"));
    m_filterKey->setValue(QStringLiteral("forward"));

    m_phongGL3Technique->addFilterKey(m_filterKey);
    m_phongGL2Technique->addFilterKey(m_filterKey);
    m_phongES2Technique->addFilterKey(m_filterKey);

    m_phongEffect->addTechnique(m_phongGL3Technique);
    m_phongEffect->addTechnique(m_phongGL2Technique);
    m_phongEffect->addTechnique(m_phongES2Technique);

    m_phongEffect->addParameter(m_ambientParameter);
    m_phongEffect->addParameter(m_diffuseParameter);
    m_phongEffect->addParameter(m_specularParameter);
    m_phongEffect->addParameter(m_shininessParameter);

    q->setEffect(m_phongEffect);
}

void QPhongMaterialPrivate::handleAmbientChanged(const QVariant &var)
{
    Q_Q(QPhongMaterial);
    emit q->ambientChanged(var.value<QColor>());
}

void QPhongMaterialPrivate::handleDiffuseChanged(const QVariant &var)
{
    Q_Q(QPhongMaterial);
    emit q->diffuseChanged(var.value<QColor>());
}

void QPhongMaterialPrivate::handleSpecularChanged(const QVariant &var)
{
    Q_Q(QPhongMaterial);
    emit q->specularChanged(var.value<QColor>());
}

void QPhongMaterialPrivate::handleShininessChanged(const QVariant &var)
{
    Q_Q(QPhongMaterial);
    emit q->shininessChanged(var.toFloat());
}

/*!
    \class Qt3DExtras::QPhongMaterial
    \brief The QPhongMaterial class provides a default implementation of the phong lighting effect.
    \inmodule Qt3DExtras
    \since 5.7
    \inherits Qt3DRender::QMaterial

    The phong lighting effect is based on the combination of 3 lighting components ambient, diffuse
    and specular. The relative strengths of these components are controlled by means of their
    reflectivity coefficients which are modelled as RGB triplets:

    \list
    \li Ambient is the color that is emitted by an object without any other light source.
    \li Diffuse is the color that is emitted for rought surface reflections with the lights.
    \li Specular is the color emitted for shiny surface reflections with the lights.
    \li The shininess of a surface is controlled by a float property.
    \endlist

    This material uses an effect with a single render pass approach and performs per fragment
    lighting. Techniques are provided for OpenGL 2, OpenGL 3 or above as well as OpenGL ES 2.
*/

/*!
    Constructs a new QPhongMaterial instance with parent object \a parent.
*/
QPhongMaterial::QPhongMaterial(QNode *parent)
    : QMaterial(*new QPhongMaterialPrivate, parent)
{
    Q_D(QPhongMaterial);
    d->init();
}

/*!
   Destroys the QPhongMaterial.
*/
QPhongMaterial::~QPhongMaterial()
{
}

/*!
    \property QPhongMaterial::ambient

    Holds the ambient color.
*/
QColor QPhongMaterial::ambient() const
{
    Q_D(const QPhongMaterial);
    return d->m_ambientParameter->value().value<QColor>();
}

/*!
    \property QPhongMaterial::diffuse

    Holds the diffuse color.
*/
QColor QPhongMaterial::diffuse() const
{
    Q_D(const QPhongMaterial);
    return d->m_diffuseParameter->value().value<QColor>();
}

/*!
    \property QPhongMaterial::specular

    Holds the specular color.
*/
QColor QPhongMaterial::specular() const
{
    Q_D(const QPhongMaterial);
    return d->m_specularParameter->value().value<QColor>();
}

/*!
    \property QPhongMaterial::shininess

    Holds the shininess exponent.
*/
float QPhongMaterial::shininess() const
{
    Q_D(const QPhongMaterial);
    return d->m_shininessParameter->value().toFloat();
}

void QPhongMaterial::setAmbient(const QColor &ambient)
{
    Q_D(QPhongMaterial);
    d->m_ambientParameter->setValue(ambient);
}

void QPhongMaterial::setDiffuse(const QColor &diffuse)
{
    Q_D(QPhongMaterial);
    d->m_diffuseParameter->setValue(diffuse);
}

void QPhongMaterial::setSpecular(const QColor &specular)
{
    Q_D(QPhongMaterial);
    d->m_specularParameter->setValue(specular);
}

void QPhongMaterial::setShininess(float shininess)
{
    Q_D(QPhongMaterial);
    d->m_shininessParameter->setValue(shininess);
}

} // namespace Qt3DExtras

QT_END_NAMESPACE
