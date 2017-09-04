/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qmorphphongmaterial.h"
#include "qmorphphongmaterial_p.h"
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

QMorphPhongMaterialPrivate::QMorphPhongMaterialPrivate()
    : QMaterialPrivate()
    , m_phongEffect(new QEffect())
    , m_ambientParameter(new QParameter(QStringLiteral("ka"), QColor::fromRgbF(0.05f, 0.05f, 0.05f, 1.0f)))
    , m_diffuseParameter(new QParameter(QStringLiteral("kd"), QColor::fromRgbF(0.7f, 0.7f, 0.7f, 1.0f)))
    , m_specularParameter(new QParameter(QStringLiteral("ks"), QColor::fromRgbF(0.01f, 0.01f, 0.01f, 1.0f)))
    , m_shininessParameter(new QParameter(QStringLiteral("shininess"), 150.0f))
    , m_interpolatorParameter(new QParameter(QStringLiteral("interpolator"), 0.0f))
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

void QMorphPhongMaterialPrivate::init()
{
    connect(m_ambientParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QMorphPhongMaterialPrivate::handleAmbientChanged);
    connect(m_diffuseParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QMorphPhongMaterialPrivate::handleDiffuseChanged);
    connect(m_specularParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QMorphPhongMaterialPrivate::handleSpecularChanged);
    connect(m_shininessParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QMorphPhongMaterialPrivate::handleShininessChanged);
    connect(m_interpolatorParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QMorphPhongMaterialPrivate::handleInterpolatorChanged);

    m_phongGL3Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/morphphong.vert"))));
    m_phongGL3Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/phong.frag"))));
    m_phongGL2ES2Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/morphphong.vert"))));
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

    Q_Q(QMorphPhongMaterial);
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
    m_phongEffect->addParameter(m_interpolatorParameter);

    q->setEffect(m_phongEffect);
}

void QMorphPhongMaterialPrivate::handleAmbientChanged(const QVariant &var)
{
    Q_Q(QMorphPhongMaterial);
    emit q->ambientChanged(var.value<QColor>());
}

void QMorphPhongMaterialPrivate::handleDiffuseChanged(const QVariant &var)
{
    Q_Q(QMorphPhongMaterial);
    emit q->diffuseChanged(var.value<QColor>());
}

void QMorphPhongMaterialPrivate::handleSpecularChanged(const QVariant &var)
{
    Q_Q(QMorphPhongMaterial);
    emit q->specularChanged(var.value<QColor>());
}

void QMorphPhongMaterialPrivate::handleShininessChanged(const QVariant &var)
{
    Q_Q(QMorphPhongMaterial);
    emit q->shininessChanged(var.toFloat());
}

void QMorphPhongMaterialPrivate::handleInterpolatorChanged(const QVariant &var)
{
    Q_Q(QMorphPhongMaterial);
    emit q->interpolatorChanged(var.toFloat());
}

/*!
    \class Qt3DExtras::QMorphPhongMaterial
    \brief The QMorphPhongMaterial class provides a default implementation of the phong lighting effect.
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
    Constructs a new QMorphPhongMaterial instance with parent object \a parent.
*/
QMorphPhongMaterial::QMorphPhongMaterial(QNode *parent)
    : QMaterial(*new QMorphPhongMaterialPrivate, parent)
{
    Q_D(QMorphPhongMaterial);
    d->init();
}

/*!
   Destroys the QMorphPhongMaterial.
*/
QMorphPhongMaterial::~QMorphPhongMaterial()
{
}

/*!
    \property QMorphPhongMaterial::ambient

    Holds the ambient color.
*/
QColor QMorphPhongMaterial::ambient() const
{
    Q_D(const QMorphPhongMaterial);
    return d->m_ambientParameter->value().value<QColor>();
}

/*!
    \property QMorphPhongMaterial::diffuse

    Holds the diffuse color.
*/
QColor QMorphPhongMaterial::diffuse() const
{
    Q_D(const QMorphPhongMaterial);
    return d->m_diffuseParameter->value().value<QColor>();
}

/*!
    \property QMorphPhongMaterial::specular

    Holds the specular color.
*/
QColor QMorphPhongMaterial::specular() const
{
    Q_D(const QMorphPhongMaterial);
    return d->m_specularParameter->value().value<QColor>();
}

/*!
    \property QMorphPhongMaterial::shininess

    Holds the shininess exponent.
*/
float QMorphPhongMaterial::shininess() const
{
    Q_D(const QMorphPhongMaterial);
    return d->m_shininessParameter->value().toFloat();
}

float QMorphPhongMaterial::interpolator() const
{
    Q_D(const QMorphPhongMaterial);
    return d->m_interpolatorParameter->value().toFloat();
}

void QMorphPhongMaterial::setAmbient(const QColor &ambient)
{
    Q_D(QMorphPhongMaterial);
    d->m_ambientParameter->setValue(ambient);
}

void QMorphPhongMaterial::setDiffuse(const QColor &diffuse)
{
    Q_D(QMorphPhongMaterial);
    d->m_diffuseParameter->setValue(diffuse);
}

void QMorphPhongMaterial::setSpecular(const QColor &specular)
{
    Q_D(QMorphPhongMaterial);
    d->m_specularParameter->setValue(specular);
}

void QMorphPhongMaterial::setShininess(float shininess)
{
    Q_D(QMorphPhongMaterial);
    d->m_shininessParameter->setValue(shininess);
}

void QMorphPhongMaterial::setInterpolator(float interpolator)
{
    Q_D(QMorphPhongMaterial);
    d->m_interpolatorParameter->setValue(interpolator);
}

} // namespace Qt3DExtras

QT_END_NAMESPACE
