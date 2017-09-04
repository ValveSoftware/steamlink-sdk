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

#include "qtexturematerial.h"
#include "qtexturematerial_p.h"
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

QT_BEGIN_NAMESPACE

using namespace Qt3DRender;

namespace Qt3DExtras {

QTextureMaterialPrivate::QTextureMaterialPrivate()
    : QMaterialPrivate()
    , m_textureEffect(new QEffect)
    , m_textureParameter(new QParameter(QStringLiteral("diffuseTexture"), new QTexture2D))
    , m_textureOffsetParameter(new QParameter(QStringLiteral("texCoordOffset"), QVector2D(0.0f, 0.0f)))
    , m_textureGL3Technique(new QTechnique)
    , m_textureGL2Technique(new QTechnique)
    , m_textureES2Technique(new QTechnique)
    , m_textureGL3RenderPass(new QRenderPass)
    , m_textureGL2RenderPass(new QRenderPass)
    , m_textureES2RenderPass(new QRenderPass)
    , m_textureGL3Shader(new QShaderProgram)
    , m_textureGL2ES2Shader(new QShaderProgram)
    , m_filterKey(new QFilterKey)
{
}

void QTextureMaterialPrivate::init()
{
    connect(m_textureParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QTextureMaterialPrivate::handleTextureChanged);
    connect(m_textureOffsetParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QTextureMaterialPrivate::handleTextureOffsetChanged);

    m_textureGL3Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/unlittexture.vert"))));
    m_textureGL3Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/unlittexture.frag"))));
    m_textureGL2ES2Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/unlittexture.vert"))));
    m_textureGL2ES2Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/unlittexture.frag"))));

    m_textureGL3Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_textureGL3Technique->graphicsApiFilter()->setMajorVersion(3);
    m_textureGL3Technique->graphicsApiFilter()->setMinorVersion(1);
    m_textureGL3Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::CoreProfile);

    m_textureGL2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_textureGL2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_textureGL2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_textureGL2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    m_textureES2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGLES);
    m_textureES2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_textureES2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_textureES2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    Q_Q(QTextureMaterial);
    m_filterKey->setParent(q);
    m_filterKey->setName(QStringLiteral("renderingStyle"));
    m_filterKey->setValue(QStringLiteral("forward"));

    m_textureGL3Technique->addFilterKey(m_filterKey);
    m_textureGL2Technique->addFilterKey(m_filterKey);
    m_textureES2Technique->addFilterKey(m_filterKey);

    m_textureGL3RenderPass->setShaderProgram(m_textureGL3Shader);
    m_textureGL2RenderPass->setShaderProgram(m_textureGL2ES2Shader);
    m_textureES2RenderPass->setShaderProgram(m_textureGL2ES2Shader);

    m_textureGL3Technique->addRenderPass(m_textureGL3RenderPass);
    m_textureGL2Technique->addRenderPass(m_textureGL2RenderPass);
    m_textureES2Technique->addRenderPass(m_textureES2RenderPass);

    m_textureEffect->addTechnique(m_textureGL3Technique);
    m_textureEffect->addTechnique(m_textureGL2Technique);
    m_textureEffect->addTechnique(m_textureES2Technique);

    m_textureEffect->addParameter(m_textureParameter);
    m_textureEffect->addParameter(m_textureOffsetParameter);

    q->setEffect(m_textureEffect);
}

void QTextureMaterialPrivate::handleTextureChanged(const QVariant &var)
{
    Q_Q(QTextureMaterial);
    emit q->textureChanged(var.value<QAbstractTexture *>());
}

void QTextureMaterialPrivate::handleTextureOffsetChanged(const QVariant &var)
{
    Q_Q(QTextureMaterial);
    emit q->textureOffsetChanged(var.value<QVector2D>());
}

/*!
    \class Qt3DExtras::QTextureMaterial
    \brief The QTextureMaterial provides a default implementation of a simple unlit
    texture material.
    \inmodule Qt3DExtras
    \since 5.9
    \inherits Qt3DRender::QMaterial

    This material uses an effect with a single render pass approach. Techniques are provided
    for OpenGL 2, OpenGL 3 or above as well as OpenGL ES 2.
*/

/*!
    Constructs a new QTextureMaterial instance with parent object \a parent.
 */
QTextureMaterial::QTextureMaterial(QNode *parent)
    : QMaterial(*new QTextureMaterialPrivate, parent)
{
    Q_D(QTextureMaterial);
    d->init();
}

/*!
    Destroys the QTextureMaterial instance.
*/
QTextureMaterial::~QTextureMaterial()
{
}

/*!
    \property QTextureMaterial::texture

    Holds the current texture used by the material.
*/
QAbstractTexture *QTextureMaterial::texture() const
{
    Q_D(const QTextureMaterial);
    return d->m_textureParameter->value().value<QAbstractTexture *>();
}

/*!
    \property QTextureMaterial::textureOffset

    Holds the current texture offset. It is applied to texture
    coordinates at render time. Defaults to (0.0, 0.0).

*/
QVector2D QTextureMaterial::textureOffset() const
{
    Q_D(const QTextureMaterial);
    return d->m_textureOffsetParameter->value().value<QVector2D>();
}

void QTextureMaterial::setTexture(QAbstractTexture *texture)
{
    Q_D(QTextureMaterial);
    d->m_textureParameter->setValue(QVariant::fromValue(texture));
}

void QTextureMaterial::setTextureOffset(QVector2D textureOffset)
{
    Q_D(QTextureMaterial);
    d->m_textureOffsetParameter->setValue(QVariant::fromValue(textureOffset));
}

} // namespace Qt3DExtras

QT_END_NAMESPACE
