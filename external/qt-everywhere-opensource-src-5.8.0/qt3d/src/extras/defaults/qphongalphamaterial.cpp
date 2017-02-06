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

#include "qphongalphamaterial.h"
#include "qphongalphamaterial_p.h"
#include <Qt3DRender/qfilterkey.h>
#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qeffect.h>
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/qshaderprogram.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qrenderpass.h>
#include <Qt3DRender/qgraphicsapifilter.h>
#include <Qt3DRender/qblendequation.h>
#include <Qt3DRender/qblendequationarguments.h>
#include <Qt3DRender/qnodepthmask.h>
#include <QUrl>
#include <QVector3D>
#include <QVector4D>

QT_BEGIN_NAMESPACE

using namespace Qt3DRender;

namespace Qt3DExtras {

QPhongAlphaMaterialPrivate::QPhongAlphaMaterialPrivate()
    : QMaterialPrivate()
    , m_phongEffect(new QEffect())
    , m_ambientParameter(new QParameter(QStringLiteral("ka"), QColor::fromRgbF(0.05f, 0.05f, 0.05f, 1.0f)))
    , m_diffuseParameter(new QParameter(QStringLiteral("kd"), QColor::fromRgbF(0.7f, 0.7f, 0.7f, 1.0f)))
    , m_specularParameter(new QParameter(QStringLiteral("ks"), QColor::fromRgbF(0.01f, 0.01f, 0.01f, 1.0f)))
    , m_shininessParameter(new QParameter(QStringLiteral("shininess"), 150.0f))
    , m_alphaParameter(new QParameter(QStringLiteral("alpha"), 0.5f))
    , m_phongAlphaGL3Technique(new QTechnique())
    , m_phongAlphaGL2Technique(new QTechnique())
    , m_phongAlphaES2Technique(new QTechnique())
    , m_phongAlphaGL3RenderPass(new QRenderPass())
    , m_phongAlphaGL2RenderPass(new QRenderPass())
    , m_phongAlphaES2RenderPass(new QRenderPass())
    , m_phongAlphaGL3Shader(new QShaderProgram())
    , m_phongAlphaGL2ES2Shader(new QShaderProgram())
    , m_noDepthMask(new QNoDepthMask())
    , m_blendState(new QBlendEquationArguments())
    , m_blendEquation(new QBlendEquation())
    , m_filterKey(new QFilterKey)
{
}

// TODO: Define how lights are properties are set in the shaders. Ideally using a QShaderData
void QPhongAlphaMaterialPrivate::init()
{
    connect(m_ambientParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QPhongAlphaMaterialPrivate::handleAmbientChanged);
    connect(m_diffuseParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QPhongAlphaMaterialPrivate::handleDiffuseChanged);
    connect(m_specularParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QPhongAlphaMaterialPrivate::handleSpecularChanged);
    connect(m_shininessParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QPhongAlphaMaterialPrivate::handleShininessChanged);
    connect(m_alphaParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QPhongAlphaMaterialPrivate::handleAlphaChanged);

    m_phongAlphaGL3Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/phong.vert"))));
    m_phongAlphaGL3Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/phongalpha.frag"))));
    m_phongAlphaGL2ES2Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/phong.vert"))));
    m_phongAlphaGL2ES2Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/phongalpha.frag"))));

    m_phongAlphaGL3Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_phongAlphaGL3Technique->graphicsApiFilter()->setMajorVersion(3);
    m_phongAlphaGL3Technique->graphicsApiFilter()->setMinorVersion(1);
    m_phongAlphaGL3Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::CoreProfile);

    m_phongAlphaGL2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_phongAlphaGL2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_phongAlphaGL2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_phongAlphaGL2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    m_phongAlphaES2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGLES);
    m_phongAlphaES2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_phongAlphaES2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_phongAlphaES2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    Q_Q(QPhongAlphaMaterial);
    m_filterKey->setParent(q);
    m_filterKey->setName(QStringLiteral("renderingStyle"));
    m_filterKey->setValue(QStringLiteral("forward"));

    m_phongAlphaGL3Technique->addFilterKey(m_filterKey);
    m_phongAlphaGL2Technique->addFilterKey(m_filterKey);
    m_phongAlphaES2Technique->addFilterKey(m_filterKey);

    m_blendState->setSourceRgb(QBlendEquationArguments::SourceAlpha);
    m_blendState->setDestinationRgb(QBlendEquationArguments::OneMinusSourceAlpha);
    m_blendEquation->setBlendFunction(QBlendEquation::Add);

    m_phongAlphaGL3RenderPass->setShaderProgram(m_phongAlphaGL3Shader);
    m_phongAlphaGL2RenderPass->setShaderProgram(m_phongAlphaGL2ES2Shader);
    m_phongAlphaES2RenderPass->setShaderProgram(m_phongAlphaGL2ES2Shader);

    m_phongAlphaGL3RenderPass->addRenderState(m_noDepthMask);
    m_phongAlphaGL3RenderPass->addRenderState(m_blendState);
    m_phongAlphaGL3RenderPass->addRenderState(m_blendEquation);

    m_phongAlphaGL2RenderPass->addRenderState(m_noDepthMask);
    m_phongAlphaGL2RenderPass->addRenderState(m_blendState);
    m_phongAlphaGL2RenderPass->addRenderState(m_blendEquation);

    m_phongAlphaES2RenderPass->addRenderState(m_noDepthMask);
    m_phongAlphaES2RenderPass->addRenderState(m_blendState);
    m_phongAlphaES2RenderPass->addRenderState(m_blendEquation);

    m_phongAlphaGL3Technique->addRenderPass(m_phongAlphaGL3RenderPass);
    m_phongAlphaGL2Technique->addRenderPass(m_phongAlphaGL2RenderPass);
    m_phongAlphaES2Technique->addRenderPass(m_phongAlphaES2RenderPass);

    m_phongEffect->addTechnique(m_phongAlphaGL3Technique);
    m_phongEffect->addTechnique(m_phongAlphaGL2Technique);
    m_phongEffect->addTechnique(m_phongAlphaES2Technique);

    m_phongEffect->addParameter(m_ambientParameter);
    m_phongEffect->addParameter(m_diffuseParameter);
    m_phongEffect->addParameter(m_specularParameter);
    m_phongEffect->addParameter(m_shininessParameter);
    m_phongEffect->addParameter(m_alphaParameter);

    q->setEffect(m_phongEffect);
}

void QPhongAlphaMaterialPrivate::handleAmbientChanged(const QVariant &var)
{
    Q_Q(QPhongAlphaMaterial);
    emit q->ambientChanged(var.value<QColor>());
}

void QPhongAlphaMaterialPrivate::handleDiffuseChanged(const QVariant &var)
{
    Q_Q(QPhongAlphaMaterial);
    emit q->diffuseChanged(var.value<QColor>());
}

void QPhongAlphaMaterialPrivate::handleSpecularChanged(const QVariant &var)
{
    Q_Q(QPhongAlphaMaterial);
    emit q->specularChanged(var.value<QColor>());
}

void QPhongAlphaMaterialPrivate::handleShininessChanged(const QVariant &var)
{
    Q_Q(QPhongAlphaMaterial);
    emit q->shininessChanged(var.toFloat());
}

void QPhongAlphaMaterialPrivate::handleAlphaChanged(const QVariant &var)
{
    Q_Q(QPhongAlphaMaterial);
    emit q->alphaChanged(var.toFloat());
}

/*!
    \class Qt3DExtras::QPhongAlphaMaterial

    \brief The QPhongAlphaMaterial class provides a default implementation of
    the phong lighting effect with alpha.
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
    \li Alpha is the transparency of the surface between 0 (fully transparent) and 1 (opaque).
    \endlist

    This material uses an effect with a single render pass approach and performs per fragment
    lighting. Techniques are provided for OpenGL 2, OpenGL 3 or above as well as OpenGL ES 2.
*/

/*!
    Constructs a new QPhongAlphaMaterial instance with parent object \a parent.
*/
QPhongAlphaMaterial::QPhongAlphaMaterial(QNode *parent)
    : QMaterial(*new QPhongAlphaMaterialPrivate, parent)
{
    Q_D(QPhongAlphaMaterial);
    d->init();

    QObject::connect(d->m_blendEquation, &Qt3DRender::QBlendEquation::blendFunctionChanged,
                     this, &QPhongAlphaMaterial::blendFunctionArgChanged);
    QObject::connect(d->m_blendState, &Qt3DRender::QBlendEquationArguments::destinationAlphaChanged,
                     this, &QPhongAlphaMaterial::destinationAlphaArgChanged);
    QObject::connect(d->m_blendState, &Qt3DRender::QBlendEquationArguments::destinationRgbChanged,
                     this, &QPhongAlphaMaterial::destinationRgbArgChanged);
    QObject::connect(d->m_blendState, &Qt3DRender::QBlendEquationArguments::sourceAlphaChanged,
                     this, &QPhongAlphaMaterial::sourceAlphaArgChanged);
    QObject::connect(d->m_blendState, &Qt3DRender::QBlendEquationArguments::sourceRgbChanged,
                     this, &QPhongAlphaMaterial::sourceRgbArgChanged);
}

/*!
   Destroys the QPhongAlphaMaterial.
*/
QPhongAlphaMaterial::~QPhongAlphaMaterial()
{
}

/*!
    \property QPhongAlphaMaterial::ambient

    Holds the ambient color.
*/
QColor QPhongAlphaMaterial::ambient() const
{
    Q_D(const QPhongAlphaMaterial);
    return d->m_ambientParameter->value().value<QColor>();
}

/*!
    \property QPhongAlphaMaterial::diffuse

    Holds the diffuse color.
*/
QColor QPhongAlphaMaterial::diffuse() const
{
    Q_D(const QPhongAlphaMaterial);
    return d->m_diffuseParameter->value().value<QColor>();
}

/*!
    \property QPhongAlphaMaterial::specular

    Holds the specular color.
*/
QColor QPhongAlphaMaterial::specular() const
{
    Q_D(const QPhongAlphaMaterial);
    return d->m_specularParameter->value().value<QColor>();
}

/*!
    \property QPhongAlphaMaterial::shininess

    Holds the shininess exponent.
*/
float QPhongAlphaMaterial::shininess() const
{
    Q_D(const QPhongAlphaMaterial);
    return d->m_shininessParameter->value().toFloat();
}

/*!
    \property QPhongAlphaMaterial::alpha

    Holds the alpha component of the object which varies between 0 and 1.

    The default value is 0.5f.
*/
float QPhongAlphaMaterial::alpha() const
{
    Q_D(const QPhongAlphaMaterial);
    return d->m_alphaParameter->value().toFloat();
}

/*!
    \property QPhongAlphaMaterial::sourceRgbArg

    Holds the blend equation source RGB blending argument.

    \sa Qt3DRender::QBlendEquationArguments::Blending
*/
QBlendEquationArguments::Blending QPhongAlphaMaterial::sourceRgbArg() const
{
    Q_D(const QPhongAlphaMaterial);
    return d->m_blendState->sourceRgb();
}

/*!
    \property QPhongAlphaMaterial::destinationRgbArg

    Holds the blend equation destination RGB blending argument.

    \sa Qt3DRender::QBlendEquationArguments::Blending
*/
QBlendEquationArguments::Blending QPhongAlphaMaterial::destinationRgbArg() const
{
    Q_D(const QPhongAlphaMaterial);
    return d->m_blendState->destinationRgb();
}

/*!
    \property QPhongAlphaMaterial::sourceAlphaArg

    Holds the blend equation source alpha blending argument.

    \sa Qt3DRender::QBlendEquationArguments::Blending
*/
QBlendEquationArguments::Blending QPhongAlphaMaterial::sourceAlphaArg() const
{
    Q_D(const QPhongAlphaMaterial);
    return d->m_blendState->sourceAlpha();
}

/*!
    \property QPhongAlphaMaterial::destinationAlphaArg

    Holds the blend equation destination alpha blending argument.

    \sa Qt3DRender::QBlendEquationArguments::Blending
*/
QBlendEquationArguments::Blending QPhongAlphaMaterial::destinationAlphaArg() const
{
    Q_D(const QPhongAlphaMaterial);
    return d->m_blendState->destinationAlpha();
}

/*!
    \property QPhongAlphaMaterial::blendFunctionArg

    Holds the blend equation function argument.

    \sa Qt3DRender::QBlendEquation::BlendFunction
*/
QBlendEquation::BlendFunction QPhongAlphaMaterial::blendFunctionArg() const
{
    Q_D(const QPhongAlphaMaterial);
    return d->m_blendEquation->blendFunction();
}

void QPhongAlphaMaterial::setAmbient(const QColor &ambient)
{
    Q_D(QPhongAlphaMaterial);
    d->m_ambientParameter->setValue(ambient);
}

void QPhongAlphaMaterial::setDiffuse(const QColor &diffuse)
{
    Q_D(QPhongAlphaMaterial);
    d->m_diffuseParameter->setValue(diffuse);
}

void QPhongAlphaMaterial::setSpecular(const QColor &specular)
{
    Q_D(QPhongAlphaMaterial);
    d->m_specularParameter->setValue(specular);
}

void QPhongAlphaMaterial::setShininess(float shininess)
{
    Q_D(QPhongAlphaMaterial);
    d->m_shininessParameter->setValue(shininess);
}

void QPhongAlphaMaterial::setAlpha(float alpha)
{
    Q_D(QPhongAlphaMaterial);
    d->m_alphaParameter->setValue(alpha);
}

void QPhongAlphaMaterial::setSourceRgbArg(QBlendEquationArguments::Blending sourceRgbArg)
{
    Q_D(QPhongAlphaMaterial);
    d->m_blendState->setSourceRgb(sourceRgbArg);
}

void QPhongAlphaMaterial::setDestinationRgbArg(QBlendEquationArguments::Blending destinationRgbArg)
{
    Q_D(QPhongAlphaMaterial);
    d->m_blendState->setDestinationRgb(destinationRgbArg);
}

void QPhongAlphaMaterial::setSourceAlphaArg(QBlendEquationArguments::Blending sourceAlphaArg)
{
    Q_D(QPhongAlphaMaterial);
    d->m_blendState->setSourceAlpha(sourceAlphaArg);
}

void QPhongAlphaMaterial::setDestinationAlphaArg(QBlendEquationArguments::Blending destinationAlphaArg)
{
    Q_D(QPhongAlphaMaterial);
    d->m_blendState->setDestinationAlpha(destinationAlphaArg);
}

void QPhongAlphaMaterial::setBlendFunctionArg(QBlendEquation::BlendFunction blendFunctionArg)
{
    Q_D(QPhongAlphaMaterial);
    d->m_blendEquation->setBlendFunction(blendFunctionArg);
}

} // namespace Qt3DExtras

QT_END_NAMESPACE
