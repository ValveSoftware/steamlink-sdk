/****************************************************************************
**
** Copyright (C) 2015 Lorenz Esch (TU Ilmenau).
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

#include "qpervertexcolormaterial.h"
#include "qpervertexcolormaterial_p.h"
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

QPerVertexColorMaterialPrivate::QPerVertexColorMaterialPrivate()
    : QMaterialPrivate()
    , m_vertexEffect(new QEffect())
    , m_vertexGL3Technique(new QTechnique())
    , m_vertexGL2Technique(new QTechnique())
    , m_vertexES2Technique(new QTechnique())
    , m_vertexGL3RenderPass(new QRenderPass())
    , m_vertexGL2RenderPass(new QRenderPass())
    , m_vertexES2RenderPass(new QRenderPass())
    , m_vertexGL3Shader(new QShaderProgram())
    , m_vertexGL2ES2Shader(new QShaderProgram())
    , m_filterKey(new QFilterKey)
{
}

/*!
    \class Qt3DExtras::QPerVertexColorMaterial
    \brief The QPerVertexColorMaterial class provides a default implementation for rendering the
    color properties set for each vertex.
    \inmodule Qt3DExtras
    \since 5.7
    \inherits Qt3DRender::QMaterial

    This lighting effect is based on the combination of 2 lighting components ambient and diffuse.
    Ambient is set by the vertex color.
    Diffuse takes in account the normal distribution of each vertex.

    \list
    \li Ambient is the color that is emitted by an object without any other light source.
    \li Diffuse is the color that is emitted for rough surface reflections with the lights
    \endlist

    This material uses an effect with a single render pass approach and forms  fragment lighting.
    Techniques are provided for OpenGL 2, OpenGL 3 or above as well as OpenGL ES 2.
*/

/*!
    Constructs a new QPerVertexColorMaterial instance with parent object \a parent.
*/
QPerVertexColorMaterial::QPerVertexColorMaterial(QNode *parent)
    : QMaterial(*new QPerVertexColorMaterialPrivate, parent)
{
    Q_D(QPerVertexColorMaterial);
    d->init();
}

/*!
   Destroys the QPerVertexColorMaterial
*/
QPerVertexColorMaterial::~QPerVertexColorMaterial()
{
}

// TODO: Define how lights are proties are set in the shaders. Ideally using a QShaderData
void QPerVertexColorMaterialPrivate::init()
{
    m_vertexGL3Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/pervertexcolor.vert"))));
    m_vertexGL3Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/pervertexcolor.frag"))));
    m_vertexGL2ES2Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/pervertexcolor.vert"))));
    m_vertexGL2ES2Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/pervertexcolor.frag"))));

    m_vertexGL3Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_vertexGL3Technique->graphicsApiFilter()->setMajorVersion(3);
    m_vertexGL3Technique->graphicsApiFilter()->setMinorVersion(1);
    m_vertexGL3Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::CoreProfile);

    m_vertexGL2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_vertexGL2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_vertexGL2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_vertexGL2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    m_vertexES2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGLES);
    m_vertexES2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_vertexES2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_vertexES2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    Q_Q(QPerVertexColorMaterial);
    m_filterKey->setParent(q);
    m_filterKey->setName(QStringLiteral("renderingStyle"));
    m_filterKey->setValue(QStringLiteral("forward"));

    m_vertexGL3Technique->addFilterKey(m_filterKey);
    m_vertexGL2Technique->addFilterKey(m_filterKey);
    m_vertexES2Technique->addFilterKey(m_filterKey);

    m_vertexGL3RenderPass->setShaderProgram(m_vertexGL3Shader);
    m_vertexGL2RenderPass->setShaderProgram(m_vertexGL2ES2Shader);
    m_vertexES2RenderPass->setShaderProgram(m_vertexGL2ES2Shader);

    m_vertexGL3Technique->addRenderPass(m_vertexGL3RenderPass);
    m_vertexGL2Technique->addRenderPass(m_vertexGL2RenderPass);
    m_vertexES2Technique->addRenderPass(m_vertexES2RenderPass);

    m_vertexEffect->addTechnique(m_vertexGL3Technique);
    m_vertexEffect->addTechnique(m_vertexGL2Technique);
    m_vertexEffect->addTechnique(m_vertexES2Technique);

    q->setEffect(m_vertexEffect);
}

} // namespace Qt3DExtras

QT_END_NAMESPACE
