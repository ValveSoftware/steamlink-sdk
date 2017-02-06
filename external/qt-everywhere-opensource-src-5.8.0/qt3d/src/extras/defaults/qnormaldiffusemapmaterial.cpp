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

#include "qnormaldiffusemapmaterial.h"
#include "qnormaldiffusemapmaterial_p.h"
#include <Qt3DRender/qfilterkey.h>
#include <Qt3DRender/qeffect.h>
#include <Qt3DRender/qtexture.h>
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qshaderprogram.h>
#include <Qt3DRender/qrenderpass.h>
#include <Qt3DRender/qgraphicsapifilter.h>

#include <QUrl>
#include <QVector3D>
#include <QVector4D>

QT_BEGIN_NAMESPACE

using namespace Qt3DRender;

namespace Qt3DExtras {

QNormalDiffuseMapMaterialPrivate::QNormalDiffuseMapMaterialPrivate()
    : QMaterialPrivate()
    , m_normalDiffuseEffect(new QEffect())
    , m_diffuseTexture(new QTexture2D())
    , m_normalTexture(new QTexture2D())
    , m_ambientParameter(new QParameter(QStringLiteral("ka"), QColor::fromRgbF(0.1f, 0.1f, 0.1f, 1.0f)))
    , m_diffuseParameter(new QParameter(QStringLiteral("diffuseTexture"), m_diffuseTexture))
    , m_normalParameter(new QParameter(QStringLiteral("normalTexture"), m_normalTexture))
    , m_specularParameter(new QParameter(QStringLiteral("ks"), QColor::fromRgbF(0.01f, 0.01f, 0.01f, 1.0f)))
    , m_shininessParameter(new QParameter(QStringLiteral("shininess"), 150.0f))
    , m_textureScaleParameter(new QParameter(QStringLiteral("texCoordScale"), 1.0f))
    , m_normalDiffuseGL3Technique(new QTechnique())
    , m_normalDiffuseGL2Technique(new QTechnique())
    , m_normalDiffuseES2Technique(new QTechnique())
    , m_normalDiffuseGL3RenderPass(new QRenderPass())
    , m_normalDiffuseGL2RenderPass(new QRenderPass())
    , m_normalDiffuseES2RenderPass(new QRenderPass())
    , m_normalDiffuseGL3Shader(new QShaderProgram())
    , m_normalDiffuseGL2ES2Shader(new QShaderProgram())
    , m_filterKey(new QFilterKey)
{
    m_diffuseTexture->setMagnificationFilter(QAbstractTexture::Linear);
    m_diffuseTexture->setMinificationFilter(QAbstractTexture::LinearMipMapLinear);
    m_diffuseTexture->setWrapMode(QTextureWrapMode(QTextureWrapMode::Repeat));
    m_diffuseTexture->setGenerateMipMaps(true);
    m_diffuseTexture->setMaximumAnisotropy(16.0f);

    m_normalTexture->setMagnificationFilter(QAbstractTexture::Linear);
    m_normalTexture->setMinificationFilter(QAbstractTexture::LinearMipMapLinear);
    m_normalTexture->setWrapMode(QTextureWrapMode(QTextureWrapMode::Repeat));
    m_normalTexture->setGenerateMipMaps(true);
    m_normalTexture->setMaximumAnisotropy(16.0f);
}

void QNormalDiffuseMapMaterialPrivate::init()
{
    connect(m_ambientParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QNormalDiffuseMapMaterialPrivate::handleAmbientChanged);
    connect(m_diffuseParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QNormalDiffuseMapMaterialPrivate::handleDiffuseChanged);
    connect(m_normalParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QNormalDiffuseMapMaterialPrivate::handleNormalChanged);
    connect(m_specularParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QNormalDiffuseMapMaterialPrivate::handleSpecularChanged);
    connect(m_shininessParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QNormalDiffuseMapMaterialPrivate::handleShininessChanged);
    connect(m_textureScaleParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QNormalDiffuseMapMaterialPrivate::handleTextureScaleChanged);

    m_normalDiffuseGL3Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/normaldiffusemap.vert"))));
    m_normalDiffuseGL3Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/normaldiffusemap.frag"))));
    m_normalDiffuseGL2ES2Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/normaldiffusemap.vert"))));
    m_normalDiffuseGL2ES2Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/normaldiffusemap.frag"))));

    m_normalDiffuseGL3Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_normalDiffuseGL3Technique->graphicsApiFilter()->setMajorVersion(3);
    m_normalDiffuseGL3Technique->graphicsApiFilter()->setMinorVersion(1);
    m_normalDiffuseGL3Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::CoreProfile);

    m_normalDiffuseGL2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_normalDiffuseGL2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_normalDiffuseGL2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_normalDiffuseGL2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    m_normalDiffuseES2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGLES);
    m_normalDiffuseES2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_normalDiffuseES2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_normalDiffuseES2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    Q_Q(QNormalDiffuseMapMaterial);
    m_filterKey->setParent(q);
    m_filterKey->setName(QStringLiteral("renderingStyle"));
    m_filterKey->setValue(QStringLiteral("forward"));

    m_normalDiffuseGL3Technique->addFilterKey(m_filterKey);
    m_normalDiffuseGL2Technique->addFilterKey(m_filterKey);
    m_normalDiffuseES2Technique->addFilterKey(m_filterKey);

    m_normalDiffuseGL3RenderPass->setShaderProgram(m_normalDiffuseGL3Shader);
    m_normalDiffuseGL2RenderPass->setShaderProgram(m_normalDiffuseGL2ES2Shader);
    m_normalDiffuseES2RenderPass->setShaderProgram(m_normalDiffuseGL2ES2Shader);

    m_normalDiffuseGL3Technique->addRenderPass(m_normalDiffuseGL3RenderPass);
    m_normalDiffuseGL2Technique->addRenderPass(m_normalDiffuseGL2RenderPass);
    m_normalDiffuseES2Technique->addRenderPass(m_normalDiffuseES2RenderPass);

    m_normalDiffuseEffect->addTechnique(m_normalDiffuseGL3Technique);
    m_normalDiffuseEffect->addTechnique(m_normalDiffuseGL2Technique);
    m_normalDiffuseEffect->addTechnique(m_normalDiffuseES2Technique);

    m_normalDiffuseEffect->addParameter(m_ambientParameter);
    m_normalDiffuseEffect->addParameter(m_diffuseParameter);
    m_normalDiffuseEffect->addParameter(m_normalParameter);
    m_normalDiffuseEffect->addParameter(m_specularParameter);
    m_normalDiffuseEffect->addParameter(m_shininessParameter);
    m_normalDiffuseEffect->addParameter(m_textureScaleParameter);

    q->setEffect(m_normalDiffuseEffect);
}

void QNormalDiffuseMapMaterialPrivate::handleAmbientChanged(const QVariant &var)
{
    Q_Q(QNormalDiffuseMapMaterial);
    emit q->ambientChanged(var.value<QColor>());
}

void QNormalDiffuseMapMaterialPrivate::handleDiffuseChanged(const QVariant &var)
{
    Q_Q(QNormalDiffuseMapMaterial);
    emit q->diffuseChanged(var.value<QAbstractTexture *>());
}

void QNormalDiffuseMapMaterialPrivate::handleNormalChanged(const QVariant &var)
{
    Q_Q(QNormalDiffuseMapMaterial);
    emit q->normalChanged(var.value<QAbstractTexture *>());
}

void QNormalDiffuseMapMaterialPrivate::handleSpecularChanged(const QVariant &var)
{
    Q_Q(QNormalDiffuseMapMaterial);
    emit q->specularChanged(var.value<QColor>());
}

void QNormalDiffuseMapMaterialPrivate::handleShininessChanged(const QVariant &var)
{
    Q_Q(QNormalDiffuseMapMaterial);
    emit q->shininessChanged(var.toFloat());
}

void QNormalDiffuseMapMaterialPrivate::handleTextureScaleChanged(const QVariant &var)
{
    Q_Q(QNormalDiffuseMapMaterial);
    emit q->textureScaleChanged(var.toFloat());
}

/*!
    \class Qt3DExtras::QNormalDiffuseMapMaterial
    \brief The QNormalDiffuseMapMaterial provides a default implementation of the phong lighting
    and bump effect where the diffuse light component is read from a texture map and the normals of
    the mesh being rendered from a normal texture map.
    \inmodule Qt3DExtras
    \since 5.7
    \inherits Qt3DRender::QMaterial

    The specular lighting effect is based on the combination of 3 lighting components ambient,
    diffuse and specular. The relative strengths of these components are controlled by means of
    their reflectivity coefficients which are modelled as RGB triplets:

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
    Constructs a new QNormalDiffuseMapMaterial instance with parent object \a parent.
*/
QNormalDiffuseMapMaterial::QNormalDiffuseMapMaterial(QNode *parent)
    : QMaterial(*new QNormalDiffuseMapMaterialPrivate, parent)
{
    Q_D(QNormalDiffuseMapMaterial);
    d->init();
}

/*! \internal */
QNormalDiffuseMapMaterial::QNormalDiffuseMapMaterial(QNormalDiffuseMapMaterialPrivate &dd, QNode *parent)
    : QMaterial(dd, parent)
{
    Q_D(QNormalDiffuseMapMaterial);
    d->init();
}

/*!
    Destroys the QNormalDiffuseMapMaterial instance.
*/
QNormalDiffuseMapMaterial::~QNormalDiffuseMapMaterial()
{
}

/*!
    \property QNormalDiffuseMapMaterial::ambient

    Holds the current ambient color.
*/
QColor QNormalDiffuseMapMaterial::ambient() const
{
    Q_D(const QNormalDiffuseMapMaterial);
    return d->m_ambientParameter->value().value<QColor>();
}

/*!
    \property QNormalDiffuseMapMaterial::specular

    Holds the current specular color.
*/
QColor QNormalDiffuseMapMaterial::specular() const
{
    Q_D(const QNormalDiffuseMapMaterial);
    return d->m_specularParameter->value().value<QColor>();
}

/*!
    \property QNormalDiffuseMapMaterial::diffuse

    Holds the current diffuse map texture.

    By default, the diffuse texture has these properties:

    \list
        \li Linear minification and magnification filters
        \li Linear mipmap with mipmapping enabled
        \li Repeat wrap modeM
        \li Maximum anisotropy of 16.0
    \endlist
*/
QAbstractTexture *QNormalDiffuseMapMaterial::diffuse() const
{
    Q_D(const QNormalDiffuseMapMaterial);
    return d->m_diffuseParameter->value().value<QAbstractTexture *>();
}

/*!
    \property QNormalDiffuseMapMaterial::normal

    Holds the current normal map texture.

    By default, the normal texture has the following properties:

    \list
        \li Linear minification and magnification filters
        \li Repeat wrap mode
        \li Maximum anisotropy of 16.0
    \endlist
*/
QAbstractTexture *QNormalDiffuseMapMaterial::normal() const
{
    Q_D(const QNormalDiffuseMapMaterial);
    return d->m_normalParameter->value().value<QAbstractTexture *>();
}

/*!
    \property QNormalDiffuseMapMaterial::shininess

    Holds the current shininess as a float value.
*/
float QNormalDiffuseMapMaterial::shininess() const
{
    Q_D(const QNormalDiffuseMapMaterial);
    return d->m_shininessParameter->value().toFloat();
}

/*!
    \property QNormalDiffuseMapMaterial::textureScale

    Holds the current texture scale. It is applied as a multiplier to texture
    coordinates at render time. Defaults to 1.0.
*/
float QNormalDiffuseMapMaterial::textureScale() const
{
    Q_D(const QNormalDiffuseMapMaterial);
    return d->m_textureScaleParameter->value().toFloat();
}

void QNormalDiffuseMapMaterial::setAmbient(const QColor &ambient)
{
    Q_D(QNormalDiffuseMapMaterial);
    d->m_ambientParameter->setValue(ambient);
}

void QNormalDiffuseMapMaterial::setSpecular(const QColor &specular)
{
    Q_D(QNormalDiffuseMapMaterial);
    d->m_specularParameter->setValue(specular);
}

void QNormalDiffuseMapMaterial::setDiffuse(QAbstractTexture *diffuse)
{
    Q_D(QNormalDiffuseMapMaterial);
    d->m_diffuseParameter->setValue(QVariant::fromValue(diffuse));
}

void QNormalDiffuseMapMaterial::setNormal(QAbstractTexture *normal)
{
    Q_D(QNormalDiffuseMapMaterial);
    d->m_normalParameter->setValue(QVariant::fromValue(normal));
}

void QNormalDiffuseMapMaterial::setShininess(float shininess)
{
    Q_D(QNormalDiffuseMapMaterial);
    d->m_shininessParameter->setValue(shininess);
}

void QNormalDiffuseMapMaterial::setTextureScale(float textureScale)
{
    Q_D(QNormalDiffuseMapMaterial);
    d->m_textureScaleParameter->setValue(textureScale);
}

} // namespace Qt3DRender

QT_END_NAMESPACE
