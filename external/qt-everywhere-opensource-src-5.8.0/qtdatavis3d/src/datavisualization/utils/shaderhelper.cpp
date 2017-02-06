/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "shaderhelper_p.h"

#include <QtGui/QOpenGLShader>

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

void discardDebugMsgs(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(type)
    Q_UNUSED(context)
    Q_UNUSED(msg)
    // Used to discard warnings generated during shader test compilation
}

ShaderHelper::ShaderHelper(QObject *parent,
                           const QString &vertexShader,
                           const QString &fragmentShader,
                           const QString &texture,
                           const QString &depthTexture)
    : m_caller(parent),
      m_program(0),
      m_vertexShaderFile(vertexShader),
      m_fragmentShaderFile(fragmentShader),
      m_textureFile(texture),
      m_depthTextureFile(depthTexture),
      m_positionAttr(0),
      m_uvAttr(0),
      m_normalAttr(0),
      m_colorUniform(0),
      m_viewMatrixUniform(0),
      m_modelMatrixUniform(0),
      m_invTransModelMatrixUniform(0),
      m_depthMatrixUniform(0),
      m_mvpMatrixUniform(0),
      m_lightPositionUniform(0),
      m_lightStrengthUniform(0),
      m_ambientStrengthUniform(0),
      m_shadowQualityUniform(0),
      m_textureUniform(0),
      m_shadowUniform(0),
      m_gradientMinUniform(0),
      m_gradientHeightUniform(0),
      m_lightColorUniform(0),
      m_volumeSliceIndicesUniform(0),
      m_colorIndexUniform(0),
      m_cameraPositionRelativeToModelUniform(0),
      m_color8BitUniform(0),
      m_textureDimensionsUniform(0),
      m_sampleCountUniform(0),
      m_alphaMultiplierUniform(0),
      m_preserveOpacityUniform(0),
      m_minBoundsUniform(0),
      m_maxBoundsUniform(0),
      m_sliceFrameWidthUniform(0),
      m_initialized(false)
{
}

ShaderHelper::~ShaderHelper()
{
    delete m_program;
}

void ShaderHelper::setShaders(const QString &vertexShader,
                              const QString &fragmentShader)
{
    m_vertexShaderFile = vertexShader;
    m_fragmentShaderFile = fragmentShader;
}

void ShaderHelper::setTextures(const QString &texture,
                               const QString &depthTexture)
{
    m_textureFile = texture;
    m_depthTextureFile = depthTexture;
}

void ShaderHelper::initialize()
{
    if (m_program)
        delete m_program;
    m_program = new QOpenGLShaderProgram(m_caller);
    if (!m_program->addShaderFromSourceFile(QOpenGLShader::Vertex, m_vertexShaderFile))
        qFatal("Compiling Vertex shader failed");
    if (!m_program->addShaderFromSourceFile(QOpenGLShader::Fragment, m_fragmentShaderFile))
        qFatal("Compiling Fragment shader failed");
    m_program->link();

    m_positionAttr = m_program->attributeLocation("vertexPosition_mdl");
    m_normalAttr = m_program->attributeLocation("vertexNormal_mdl");
    m_uvAttr = m_program->attributeLocation("vertexUV");

    m_mvpMatrixUniform = m_program->uniformLocation("MVP");
    m_viewMatrixUniform = m_program->uniformLocation("V");
    m_modelMatrixUniform = m_program->uniformLocation("M");
    m_invTransModelMatrixUniform = m_program->uniformLocation("itM");
    m_depthMatrixUniform = m_program->uniformLocation("depthMVP");
    m_lightPositionUniform = m_program->uniformLocation("lightPosition_wrld");
    m_lightStrengthUniform = m_program->uniformLocation("lightStrength");
    m_ambientStrengthUniform = m_program->uniformLocation("ambientStrength");
    m_shadowQualityUniform = m_program->uniformLocation("shadowQuality");
    m_colorUniform = m_program->uniformLocation("color_mdl");
    m_textureUniform = m_program->uniformLocation("textureSampler");
    m_shadowUniform = m_program->uniformLocation("shadowMap");
    m_gradientMinUniform = m_program->uniformLocation("gradMin");
    m_gradientHeightUniform = m_program->uniformLocation("gradHeight");
    m_lightColorUniform = m_program->uniformLocation("lightColor");
    m_volumeSliceIndicesUniform = m_program->uniformLocation("volumeSliceIndices");
    m_colorIndexUniform = m_program->uniformLocation("colorIndex");
    m_cameraPositionRelativeToModelUniform = m_program->uniformLocation("cameraPositionRelativeToModel");
    m_color8BitUniform = m_program->uniformLocation("color8Bit");
    m_textureDimensionsUniform = m_program->uniformLocation("textureDimensions");
    m_sampleCountUniform = m_program->uniformLocation("sampleCount");
    m_alphaMultiplierUniform = m_program->uniformLocation("alphaMultiplier");
    m_preserveOpacityUniform = m_program->uniformLocation("preserveOpacity");
    m_minBoundsUniform = m_program->uniformLocation("minBounds");
    m_maxBoundsUniform = m_program->uniformLocation("maxBounds");
    m_sliceFrameWidthUniform = m_program->uniformLocation("sliceFrameWidth");
    m_initialized = true;
}

bool ShaderHelper::testCompile()
{
    bool result = true;

    // Discard warnings, we only need the result
    QtMessageHandler handler = qInstallMessageHandler(discardDebugMsgs);
    if (m_program)
        delete m_program;
    m_program = new QOpenGLShaderProgram();
    if (!m_program->addShaderFromSourceFile(QOpenGLShader::Vertex, m_vertexShaderFile))
        result = false;
    if (!m_program->addShaderFromSourceFile(QOpenGLShader::Fragment, m_fragmentShaderFile))
        result = false;

    // Restore actual message handler
    qInstallMessageHandler(handler);
    return result;
}

void ShaderHelper::bind()
{
    m_program->bind();
}

void ShaderHelper::release()
{
    m_program->release();
}

void ShaderHelper::setUniformValue(GLint uniform, const QVector2D &value)
{
    m_program->setUniformValue(uniform, value);
}

void ShaderHelper::setUniformValue(GLint uniform, const QVector3D &value)
{
    m_program->setUniformValue(uniform, value);
}

void ShaderHelper::setUniformValue(GLint uniform, const QVector4D &value)
{
    m_program->setUniformValue(uniform, value);
}

void ShaderHelper::setUniformValue(GLint uniform, const QMatrix4x4 &value)
{
    m_program->setUniformValue(uniform, value);
}

void ShaderHelper::setUniformValue(GLint uniform, GLfloat value)
{
    m_program->setUniformValue(uniform, value);
}

void ShaderHelper::setUniformValue(GLint uniform, GLint value)
{
    m_program->setUniformValue(uniform, value);
}

void ShaderHelper::setUniformValueArray(GLint uniform, const QVector4D *values, int count)
{
    m_program->setUniformValueArray(uniform, values, count);
}

GLint ShaderHelper::MVP()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_mvpMatrixUniform;
}

GLint ShaderHelper::view()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_viewMatrixUniform;
}

GLint ShaderHelper::model()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_modelMatrixUniform;
}

GLint ShaderHelper::nModel()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_invTransModelMatrixUniform;
}

GLint ShaderHelper::depth()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_depthMatrixUniform;
}

GLint ShaderHelper::lightP()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_lightPositionUniform;
}

GLint ShaderHelper::lightS()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_lightStrengthUniform;
}

GLint ShaderHelper::ambientS()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_ambientStrengthUniform;
}

GLint ShaderHelper::shadowQ()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_shadowQualityUniform;
}

GLint ShaderHelper::color()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_colorUniform;
}

GLint ShaderHelper::texture()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_textureUniform;
}

GLint ShaderHelper::shadow()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_shadowUniform;
}

GLint ShaderHelper::gradientMin()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_gradientMinUniform;
}

GLint ShaderHelper::gradientHeight()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_gradientHeightUniform;
}

GLint ShaderHelper::lightColor()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_lightColorUniform;
}

GLint ShaderHelper::volumeSliceIndices()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_volumeSliceIndicesUniform;
}

GLint ShaderHelper::colorIndex()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_colorIndexUniform;
}

GLint ShaderHelper::cameraPositionRelativeToModel()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_cameraPositionRelativeToModelUniform;
}

GLint ShaderHelper::color8Bit()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_color8BitUniform;
}

GLint ShaderHelper::textureDimensions()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_textureDimensionsUniform;
}

GLint ShaderHelper::sampleCount()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_sampleCountUniform;
}

GLint ShaderHelper::alphaMultiplier()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_alphaMultiplierUniform;
}

GLint ShaderHelper::preserveOpacity()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_preserveOpacityUniform;
}

GLint ShaderHelper::maxBounds()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_maxBoundsUniform;
}

GLint ShaderHelper::minBounds()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_minBoundsUniform;
}

GLint ShaderHelper::sliceFrameWidth()
{

    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_sliceFrameWidthUniform;
}

GLint ShaderHelper::posAtt()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_positionAttr;
}

GLint ShaderHelper::uvAtt()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_uvAttr;
}

GLint ShaderHelper::normalAtt()
{
    if (!m_initialized)
        qFatal("Shader not initialized");
    return m_normalAttr;
}

QT_END_NAMESPACE_DATAVISUALIZATION
