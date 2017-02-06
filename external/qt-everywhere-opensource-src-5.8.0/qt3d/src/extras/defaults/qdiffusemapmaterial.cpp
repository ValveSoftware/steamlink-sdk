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

#include "qdiffusemapmaterial.h"
#include "qdiffusemapmaterial_p.h"
#include <Qt3DRender/qfilterkey.h>
#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qeffect.h>
#include <Qt3DRender/qtexture.h>
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

QDiffuseMapMaterialPrivate::QDiffuseMapMaterialPrivate()
    : QMaterialPrivate()
    , m_diffuseMapEffect(new QEffect())
    , m_diffuseTexture(new QTexture2D())
    , m_ambientParameter(new QParameter(QStringLiteral("ka"), QColor::fromRgbF(0.05f, 0.05f, 0.05f, 1.0f)))
    , m_diffuseParameter(new QParameter(QStringLiteral("diffuseTexture"), m_diffuseTexture))
    , m_specularParameter(new QParameter(QStringLiteral("ks"), QColor::fromRgbF(0.01f, 0.01f, 0.01f, 1.0f)))
    , m_shininessParameter(new QParameter(QStringLiteral("shininess"), 150.0f))
    , m_textureScaleParameter(new QParameter(QStringLiteral("texCoordScale"), 1.0f))
    , m_diffuseMapGL3Technique(new QTechnique())
    , m_diffuseMapGL2Technique(new QTechnique())
    , m_diffuseMapES2Technique(new QTechnique())
    , m_diffuseMapGL3RenderPass(new QRenderPass())
    , m_diffuseMapGL2RenderPass(new QRenderPass())
    , m_diffuseMapES2RenderPass(new QRenderPass())
    , m_diffuseMapGL3Shader(new QShaderProgram())
    , m_diffuseMapGL2ES2Shader(new QShaderProgram())
    , m_filterKey(new QFilterKey)
{
    m_diffuseTexture->setMagnificationFilter(QAbstractTexture::Linear);
    m_diffuseTexture->setMinificationFilter(QAbstractTexture::LinearMipMapLinear);
    m_diffuseTexture->setWrapMode(QTextureWrapMode(QTextureWrapMode::Repeat));
    m_diffuseTexture->setGenerateMipMaps(true);
    m_diffuseTexture->setMaximumAnisotropy(16.0f);
}

void QDiffuseMapMaterialPrivate::init()
{
    connect(m_ambientParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QDiffuseMapMaterialPrivate::handleAmbientChanged);
    connect(m_diffuseParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QDiffuseMapMaterialPrivate::handleDiffuseChanged);
    connect(m_specularParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QDiffuseMapMaterialPrivate::handleSpecularChanged);
    connect(m_shininessParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QDiffuseMapMaterialPrivate::handleShininessChanged);
    connect(m_textureScaleParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QDiffuseMapMaterialPrivate::handleTextureScaleChanged);

    m_diffuseMapGL3Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/diffusemap.vert"))));
    m_diffuseMapGL3Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/diffusemap.frag"))));
    m_diffuseMapGL2ES2Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/diffusemap.vert"))));
    m_diffuseMapGL2ES2Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/diffusemap.frag"))));

    m_diffuseMapGL3Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_diffuseMapGL3Technique->graphicsApiFilter()->setMajorVersion(3);
    m_diffuseMapGL3Technique->graphicsApiFilter()->setMinorVersion(1);
    m_diffuseMapGL3Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::CoreProfile);

    m_diffuseMapGL2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_diffuseMapGL2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_diffuseMapGL2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_diffuseMapGL2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    m_diffuseMapES2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGLES);
    m_diffuseMapES2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_diffuseMapES2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_diffuseMapES2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    Q_Q(QDiffuseMapMaterial);
    m_filterKey->setParent(q);
    m_filterKey->setName(QStringLiteral("renderingStyle"));
    m_filterKey->setValue(QStringLiteral("forward"));

    m_diffuseMapGL3Technique->addFilterKey(m_filterKey);
    m_diffuseMapGL2Technique->addFilterKey(m_filterKey);
    m_diffuseMapES2Technique->addFilterKey(m_filterKey);

    m_diffuseMapGL3RenderPass->setShaderProgram(m_diffuseMapGL3Shader);
    m_diffuseMapGL2RenderPass->setShaderProgram(m_diffuseMapGL2ES2Shader);
    m_diffuseMapES2RenderPass->setShaderProgram(m_diffuseMapGL2ES2Shader);

    m_diffuseMapGL3Technique->addRenderPass(m_diffuseMapGL3RenderPass);
    m_diffuseMapGL2Technique->addRenderPass(m_diffuseMapGL2RenderPass);
    m_diffuseMapES2Technique->addRenderPass(m_diffuseMapES2RenderPass);

    m_diffuseMapEffect->addTechnique(m_diffuseMapGL3Technique);
    m_diffuseMapEffect->addTechnique(m_diffuseMapGL2Technique);
    m_diffuseMapEffect->addTechnique(m_diffuseMapES2Technique);

    m_diffuseMapEffect->addParameter(m_ambientParameter);
    m_diffuseMapEffect->addParameter(m_diffuseParameter);
    m_diffuseMapEffect->addParameter(m_specularParameter);
    m_diffuseMapEffect->addParameter(m_shininessParameter);
    m_diffuseMapEffect->addParameter(m_textureScaleParameter);

    q->setEffect(m_diffuseMapEffect);
}

void QDiffuseMapMaterialPrivate::handleAmbientChanged(const QVariant &var)
{
    Q_Q(QDiffuseMapMaterial);
    emit q->ambientChanged(var.value<QColor>());
}

void QDiffuseMapMaterialPrivate::handleDiffuseChanged(const QVariant &var)
{
    Q_Q(QDiffuseMapMaterial);
    emit q->diffuseChanged(var.value<QAbstractTexture *>());
}

void QDiffuseMapMaterialPrivate::handleSpecularChanged(const QVariant &var)
{
    Q_Q(QDiffuseMapMaterial);
    emit q->specularChanged(var.value<QColor>());
}

void QDiffuseMapMaterialPrivate::handleShininessChanged(const QVariant &var)
{
    Q_Q(QDiffuseMapMaterial);
    emit q->shininessChanged(var.toFloat());
}

void QDiffuseMapMaterialPrivate::handleTextureScaleChanged(const QVariant &var)
{
    Q_Q(QDiffuseMapMaterial);
    emit q->textureScaleChanged(var.toFloat());
}

/*!
    \class Qt3DExtras::QDiffuseMapMaterial
    \brief The QDiffuseMapMaterial provides a default implementation of the phong lighting effect
    where the diffuse light component is read from a texture map.
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
    Constructs a new QDiffuseMapMaterial instance with parent object \a parent.
 */
QDiffuseMapMaterial::QDiffuseMapMaterial(QNode *parent)
    : QMaterial(*new QDiffuseMapMaterialPrivate, parent)
{
    Q_D(QDiffuseMapMaterial);
    d->init();
}

/*!
    Destroys the QDiffuseMapMaterial instance.
*/
QDiffuseMapMaterial::~QDiffuseMapMaterial()
{
}

/*!
    \property QDiffuseMapMaterial::ambient

    Holds the current ambient color.
*/

QColor QDiffuseMapMaterial::ambient() const
{
    Q_D(const QDiffuseMapMaterial);
    return d->m_ambientParameter->value().value<QColor>();
}

/*!
    \property QDiffuseMapMaterial::specular

    Holds the current specular color.
*/
QColor QDiffuseMapMaterial::specular() const
{
    Q_D(const QDiffuseMapMaterial);
    return d->m_specularParameter->value().value<QColor>();
}

/*!
    \property QDiffuseMapMaterial::shininess

    Holds the current shininess as a float value.
*/
float QDiffuseMapMaterial::shininess() const
{
    Q_D(const QDiffuseMapMaterial);
    return d->m_shininessParameter->value().toFloat();
}

/*!
    \property QDiffuseMapMaterial::diffuse

    Holds the current texture used as the diffuse map.

    By default, the diffuse texture has the following properties:

    \list
        \li Linear minification and magnification filters
        \li Linear mipmap with mipmapping enabled
        \li Repeat wrap mode
        \li Maximum anisotropy of 16.0
    \endlist
*/
QAbstractTexture *QDiffuseMapMaterial::diffuse() const
{
    Q_D(const QDiffuseMapMaterial);
    return d->m_diffuseParameter->value().value<QAbstractTexture *>();
}

/*!
    \property QDiffuseMapMaterial::textureScale

    Holds the current texture scale. It is applied as a multiplier to texture
    coordinates at render time. Defaults to 1.0.

*/
float QDiffuseMapMaterial::textureScale() const
{
    Q_D(const QDiffuseMapMaterial);
    return d->m_textureScaleParameter->value().toFloat();
}

void QDiffuseMapMaterial::setAmbient(const QColor &ambient)
{
    Q_D(const QDiffuseMapMaterial);
    d->m_ambientParameter->setValue(ambient);
}

void QDiffuseMapMaterial::setSpecular(const QColor &specular)
{
    Q_D(QDiffuseMapMaterial);
    d->m_specularParameter->setValue(specular);
}

void QDiffuseMapMaterial::setShininess(float shininess)
{
    Q_D(QDiffuseMapMaterial);
    d->m_shininessParameter->setValue(shininess);
}

void QDiffuseMapMaterial::setDiffuse(QAbstractTexture *diffuseMap)
{
    Q_D(QDiffuseMapMaterial);
    d->m_diffuseParameter->setValue(QVariant::fromValue(diffuseMap));
}

void QDiffuseMapMaterial::setTextureScale(float textureScale)
{
    Q_D(QDiffuseMapMaterial);
    d->m_textureScaleParameter->setValue(textureScale);
}

} // namespace Qt3DExtras

QT_END_NAMESPACE
