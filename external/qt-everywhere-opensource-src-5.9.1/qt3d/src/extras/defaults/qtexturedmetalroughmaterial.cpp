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

#include "qtexturedmetalroughmaterial.h"
#include "qtexturedmetalroughmaterial_p.h"
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

QTexturedMetalRoughMaterialPrivate::QTexturedMetalRoughMaterialPrivate()
    : QMaterialPrivate()
    , m_baseColorTexture(new QTexture2D())
    , m_metalnessTexture(new QTexture2D())
    , m_roughnessTexture(new QTexture2D())
    , m_ambientOcclusionTexture(new QTexture2D())
    , m_normalTexture(new QTexture2D())
    , m_environmentIrradianceTexture(new QTexture2D())
    , m_environmentSpecularTexture(new QTexture2D())
    , m_baseColorParameter(new QParameter(QStringLiteral("baseColorMap"), m_baseColorTexture))
    , m_metalnessParameter(new QParameter(QStringLiteral("metalnessMap"), m_metalnessTexture))
    , m_roughnessParameter(new QParameter(QStringLiteral("roughnessMap"), m_roughnessTexture))
    , m_ambientOcclusionParameter(new QParameter(QStringLiteral("ambientOcclusionMap"), m_ambientOcclusionTexture))
    , m_normalParameter(new QParameter(QStringLiteral("normalMap"), m_normalTexture))
    , m_environmentIrradianceParameter(new QParameter(QStringLiteral("envLight.irradiance"), m_environmentIrradianceTexture))
    , m_environmentSpecularParameter(new QParameter(QStringLiteral("envLight.specular"), m_environmentSpecularTexture))
    , m_metalRoughEffect(new QEffect())
    , m_metalRoughGL3Technique(new QTechnique())
    , m_metalRoughGL3RenderPass(new QRenderPass())
    , m_metalRoughGL3Shader(new QShaderProgram())
    , m_filterKey(new QFilterKey)
{
    m_baseColorTexture->setMagnificationFilter(QAbstractTexture::Linear);
    m_baseColorTexture->setMinificationFilter(QAbstractTexture::LinearMipMapLinear);
    m_baseColorTexture->setWrapMode(QTextureWrapMode(QTextureWrapMode::Repeat));
    m_baseColorTexture->setGenerateMipMaps(true);
    m_baseColorTexture->setMaximumAnisotropy(16.0f);

    m_metalnessTexture->setMagnificationFilter(QAbstractTexture::Linear);
    m_metalnessTexture->setMinificationFilter(QAbstractTexture::LinearMipMapLinear);
    m_metalnessTexture->setWrapMode(QTextureWrapMode(QTextureWrapMode::Repeat));
    m_metalnessTexture->setGenerateMipMaps(true);
    m_metalnessTexture->setMaximumAnisotropy(16.0f);

    m_roughnessTexture->setMagnificationFilter(QAbstractTexture::Linear);
    m_roughnessTexture->setMinificationFilter(QAbstractTexture::LinearMipMapLinear);
    m_roughnessTexture->setWrapMode(QTextureWrapMode(QTextureWrapMode::Repeat));
    m_roughnessTexture->setGenerateMipMaps(true);
    m_roughnessTexture->setMaximumAnisotropy(16.0f);

    m_ambientOcclusionTexture->setMagnificationFilter(QAbstractTexture::Linear);
    m_ambientOcclusionTexture->setMinificationFilter(QAbstractTexture::LinearMipMapLinear);
    m_ambientOcclusionTexture->setWrapMode(QTextureWrapMode(QTextureWrapMode::Repeat));
    m_ambientOcclusionTexture->setGenerateMipMaps(true);
    m_ambientOcclusionTexture->setMaximumAnisotropy(16.0f);

    m_normalTexture->setMagnificationFilter(QAbstractTexture::Linear);
    m_normalTexture->setMinificationFilter(QAbstractTexture::LinearMipMapLinear);
    m_normalTexture->setWrapMode(QTextureWrapMode(QTextureWrapMode::Repeat));
    m_normalTexture->setGenerateMipMaps(true);
    m_normalTexture->setMaximumAnisotropy(16.0f);

    m_environmentIrradianceTexture->setMagnificationFilter(QAbstractTexture::Linear);
    m_environmentIrradianceTexture->setMinificationFilter(QAbstractTexture::LinearMipMapLinear);
    m_environmentIrradianceTexture->setWrapMode(QTextureWrapMode(QTextureWrapMode::Repeat));
    m_environmentIrradianceTexture->setGenerateMipMaps(true);
    m_environmentIrradianceTexture->setMaximumAnisotropy(16.0f);

    m_environmentSpecularTexture->setMagnificationFilter(QAbstractTexture::Linear);
    m_environmentSpecularTexture->setMinificationFilter(QAbstractTexture::LinearMipMapLinear);
    m_environmentSpecularTexture->setWrapMode(QTextureWrapMode(QTextureWrapMode::Repeat));
    m_environmentSpecularTexture->setGenerateMipMaps(true);
    m_environmentSpecularTexture->setMaximumAnisotropy(16.0f);
}

void QTexturedMetalRoughMaterialPrivate::init()
{
    connect(m_baseColorParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QTexturedMetalRoughMaterialPrivate::handleBaseColorChanged);
    connect(m_metalnessParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QTexturedMetalRoughMaterialPrivate::handleMetallicChanged);
    connect(m_roughnessParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QTexturedMetalRoughMaterialPrivate::handleRoughnessChanged);
    connect(m_ambientOcclusionParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QTexturedMetalRoughMaterialPrivate::handleAmbientOcclusionChanged);
    connect(m_normalParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QTexturedMetalRoughMaterialPrivate::handleNormalChanged);

    m_metalRoughGL3Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/metalrough.vert"))));
    m_metalRoughGL3Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/metalrough.frag"))));

    m_metalRoughGL3Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_metalRoughGL3Technique->graphicsApiFilter()->setMajorVersion(3);
    m_metalRoughGL3Technique->graphicsApiFilter()->setMinorVersion(1);
    m_metalRoughGL3Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::CoreProfile);

    Q_Q(QTexturedMetalRoughMaterial);
    m_filterKey->setParent(q);
    m_filterKey->setName(QStringLiteral("renderingStyle"));
    m_filterKey->setValue(QStringLiteral("forward"));

    m_metalRoughGL3Technique->addFilterKey(m_filterKey);
    m_metalRoughGL3RenderPass->setShaderProgram(m_metalRoughGL3Shader);
    m_metalRoughGL3Technique->addRenderPass(m_metalRoughGL3RenderPass);
    m_metalRoughEffect->addTechnique(m_metalRoughGL3Technique);

    m_metalRoughEffect->addParameter(m_baseColorParameter);
    m_metalRoughEffect->addParameter(m_metalnessParameter);
    m_metalRoughEffect->addParameter(m_roughnessParameter);
    m_metalRoughEffect->addParameter(m_ambientOcclusionParameter);
    m_metalRoughEffect->addParameter(m_normalParameter);

    // Note that even though those parameters are not exposed in the API,
    // they need to be kept around for now due to a bug in some drivers/GPUs
    // (at least Intel) which cause issues with unbound textures even if you
    // don't try to sample from them.
    // Can probably go away once we generate the shaders and deal in this
    // case in a better way.
    m_metalRoughEffect->addParameter(m_environmentIrradianceParameter);
    m_metalRoughEffect->addParameter(m_environmentSpecularParameter);

    q->setEffect(m_metalRoughEffect);
}

void QTexturedMetalRoughMaterialPrivate::handleBaseColorChanged(const QVariant &var)
{
    Q_Q(QTexturedMetalRoughMaterial);
    emit q->baseColorChanged(var.value<QAbstractTexture *>());
}

void QTexturedMetalRoughMaterialPrivate::handleMetallicChanged(const QVariant &var)
{
    Q_Q(QTexturedMetalRoughMaterial);
    emit q->metalnessChanged(var.value<QAbstractTexture *>());
}
void QTexturedMetalRoughMaterialPrivate::handleRoughnessChanged(const QVariant &var)
{
    Q_Q(QTexturedMetalRoughMaterial);
    emit q->roughnessChanged(var.value<QAbstractTexture *>());
}
void QTexturedMetalRoughMaterialPrivate::handleAmbientOcclusionChanged(const QVariant &var)
{
    Q_Q(QTexturedMetalRoughMaterial);
    emit q->ambientOcclusionChanged(var.value<QAbstractTexture *>());
}

void QTexturedMetalRoughMaterialPrivate::handleNormalChanged(const QVariant &var)
{
    Q_Q(QTexturedMetalRoughMaterial);
    emit q->normalChanged(var.value<QAbstractTexture *>());
}

/*!
    \class Qt3DExtras::QTexturedMetalRoughMaterial
    \brief The QTexturedMetalRoughMaterial provides a default implementation of PBR
    lighting, environment maps and bump effect where the components are read from texture
    maps (including normal maps).
    \inmodule Qt3DExtras
    \since 5.9
    \inherits Qt3DRender::QMaterial

    This material uses an effect with a single render pass approach and performs per fragment
    lighting. Techniques are provided for OpenGL 3 only.
*/

/*!
    Constructs a new QTexturedMetalRoughMaterial instance with parent object \a parent.
*/
QTexturedMetalRoughMaterial::QTexturedMetalRoughMaterial(QNode *parent)
    : QMaterial(*new QTexturedMetalRoughMaterialPrivate, parent)
{
    Q_D(QTexturedMetalRoughMaterial);
    d->init();
}

/*! \internal */
QTexturedMetalRoughMaterial::QTexturedMetalRoughMaterial(QTexturedMetalRoughMaterialPrivate &dd, QNode *parent)
    : QMaterial(dd, parent)
{
    Q_D(QTexturedMetalRoughMaterial);
    d->init();
}

/*!
    Destroys the QTexturedMetalRoughMaterial instance.
*/
QTexturedMetalRoughMaterial::~QTexturedMetalRoughMaterial()
{
}

/*!
    \property QTexturedMetalRoughMaterial::baseColor

    Holds the current base color map texture.

    By default, the base color texture has the following properties:

    \list
        \li Linear minification and magnification filters
        \li Linear mipmap with mipmapping enabled
        \li Repeat wrap mode
        \li Maximum anisotropy of 16.0
    \endlist
*/
QAbstractTexture *QTexturedMetalRoughMaterial::baseColor() const
{
    Q_D(const QTexturedMetalRoughMaterial);
    return d->m_baseColorParameter->value().value<QAbstractTexture *>();
}

/*!
    \property QTexturedMetalRoughMaterial::metalness

    Holds the current metalness map texture.

    By default, the metalness texture has the following properties:

    \list
        \li Linear minification and magnification filters
        \li Linear mipmap with mipmapping enabled
        \li Repeat wrap mode
        \li Maximum anisotropy of 16.0
    \endlist
*/
QAbstractTexture *QTexturedMetalRoughMaterial::metalness() const
{
    Q_D(const QTexturedMetalRoughMaterial);
    return d->m_metalnessParameter->value().value<QAbstractTexture *>();
}

/*!
    \property QTexturedMetalRoughMaterial::roughness

    Holds the current roughness map texture.

    By default, the roughness texture has the following properties:

    \list
        \li Linear minification and magnification filters
        \li Linear mipmap with mipmapping enabled
        \li Repeat wrap mode
        \li Maximum anisotropy of 16.0
    \endlist
*/
QAbstractTexture *QTexturedMetalRoughMaterial::roughness() const
{
    Q_D(const QTexturedMetalRoughMaterial);
    return d->m_roughnessParameter->value().value<QAbstractTexture *>();
}

/*!
    \property QTexturedMetalRoughMaterial::ambientOcclusion

    Holds the current ambient occlusion map texture.

    By default, the ambient occlusion texture has the following properties:

    \list
        \li Linear minification and magnification filters
        \li Linear mipmap with mipmapping enabled
        \li Repeat wrap mode
        \li Maximum anisotropy of 16.0
    \endlist
*/
QAbstractTexture *QTexturedMetalRoughMaterial::ambientOcclusion() const
{
    Q_D(const QTexturedMetalRoughMaterial);
    return d->m_ambientOcclusionParameter->value().value<QAbstractTexture *>();
}

/*!
    \property QTexturedMetalRoughMaterial::normal

    Holds the current normal map texture.

    By default, the normal texture has the following properties:

    \list
        \li Linear minification and magnification filters
        \li Repeat wrap mode
        \li Maximum anisotropy of 16.0
    \endlist
*/
QAbstractTexture *QTexturedMetalRoughMaterial::normal() const
{
    Q_D(const QTexturedMetalRoughMaterial);
    return d->m_normalParameter->value().value<QAbstractTexture *>();
}


void QTexturedMetalRoughMaterial::setBaseColor(QAbstractTexture *baseColor)
{
    Q_D(QTexturedMetalRoughMaterial);
    d->m_baseColorParameter->setValue(QVariant::fromValue(baseColor));
}

void QTexturedMetalRoughMaterial::setMetalness(QAbstractTexture *metalness)
{
    Q_D(QTexturedMetalRoughMaterial);
    d->m_metalnessParameter->setValue(QVariant::fromValue(metalness));
}

void QTexturedMetalRoughMaterial::setRoughness(QAbstractTexture *roughness)
{
    Q_D(QTexturedMetalRoughMaterial);
    d->m_roughnessParameter->setValue(QVariant::fromValue(roughness));
}

void QTexturedMetalRoughMaterial::setAmbientOcclusion(QAbstractTexture *ambientOcclusion)
{
    Q_D(QTexturedMetalRoughMaterial);
    d->m_ambientOcclusionParameter->setValue(QVariant::fromValue(ambientOcclusion));
}

void QTexturedMetalRoughMaterial::setNormal(QAbstractTexture *normal)
{
    Q_D(QTexturedMetalRoughMaterial);
    d->m_normalParameter->setValue(QVariant::fromValue(normal));
}

} // namespace Qt3DExtras

QT_END_NAMESPACE
