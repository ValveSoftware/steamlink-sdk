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

#include "qnormaldiffusemapalphamaterial.h"
#include "qnormaldiffusemapalphamaterial_p.h"

#include <Qt3DRender/qeffect.h>
#include <Qt3DRender/qtexture.h>
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qshaderprogram.h>
#include <Qt3DRender/qrenderpass.h>
#include <Qt3DRender/qgraphicsapifilter.h>
#include <Qt3DRender/qalphacoverage.h>
#include <Qt3DRender/qdepthtest.h>

#include <QUrl>
#include <QVector3D>
#include <QVector4D>

QT_BEGIN_NAMESPACE

using namespace Qt3DRender;

namespace Qt3DExtras {


QNormalDiffuseMapAlphaMaterialPrivate::QNormalDiffuseMapAlphaMaterialPrivate()
    : QNormalDiffuseMapMaterialPrivate()
    , m_alphaCoverage(new QAlphaCoverage())
    , m_depthTest(new QDepthTest())
{
}

void QNormalDiffuseMapAlphaMaterialPrivate::init()
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
    m_normalDiffuseGL3Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/normaldiffusemapalpha.frag"))));
    m_normalDiffuseGL2ES2Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/normaldiffusemap.vert"))));
    m_normalDiffuseGL2ES2Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/normaldiffusemapalpha.frag"))));

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

    m_depthTest->setDepthFunction(QDepthTest::Less);

    m_normalDiffuseGL3RenderPass->setShaderProgram(m_normalDiffuseGL3Shader);
    m_normalDiffuseGL3RenderPass->addRenderState(m_alphaCoverage);
    m_normalDiffuseGL3RenderPass->addRenderState(m_depthTest);

    m_normalDiffuseGL2RenderPass->setShaderProgram(m_normalDiffuseGL2ES2Shader);
    m_normalDiffuseGL2RenderPass->addRenderState(m_alphaCoverage);
    m_normalDiffuseGL2RenderPass->addRenderState(m_depthTest);

    m_normalDiffuseES2RenderPass->setShaderProgram(m_normalDiffuseGL2ES2Shader);
    m_normalDiffuseES2RenderPass->addRenderState(m_alphaCoverage);
    m_normalDiffuseES2RenderPass->addRenderState(m_depthTest);

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


/*!
    \class Qt3DExtras::QNormalDiffuseMapAlphaMaterial
    \brief The QNormalDiffuseMapAlphaMaterial provides a specialization of QNormalDiffuseMapMaterial
    with alpha coverage and a depth test performed in the rendering pass.
    \inmodule Qt3DExtras
    \since 5.7
    \inherits Qt3DExtras::QNormalDiffuseMapMaterial

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
    Constructs a new QNormalDiffuseMapAlphaMaterial instance with parent object \a parent.
*/
QNormalDiffuseMapAlphaMaterial::QNormalDiffuseMapAlphaMaterial(QNode *parent)
    : QNormalDiffuseMapMaterial(*new QNormalDiffuseMapAlphaMaterialPrivate, parent)
{
}

/*!
    Destroys the QNormalDiffuseMapAlphaMaterial instance.
*/
QNormalDiffuseMapAlphaMaterial::~QNormalDiffuseMapAlphaMaterial()
{
}

} // namespace Qt3DRender

QT_END_NAMESPACE
