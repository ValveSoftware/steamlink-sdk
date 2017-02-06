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

#include "graphicshelpergl3_2_p.h"

#ifndef QT_OPENGL_ES_2
#include <QOpenGLFunctions_3_2_Core>
#include <QOpenGLFunctions_3_3_Core>
#include <QtOpenGLExtensions/qopenglextensions.h>
#include <Qt3DRender/private/renderlogging_p.h>
#include <private/attachmentpack_p.h>
#include <private/qgraphicsutils_p.h>

QT_BEGIN_NAMESPACE

# ifndef QT_OPENGL_3
#  define GL_PATCH_VERTICES 36466
#  define GL_ACTIVE_RESOURCES 0x92F5
#  define GL_ACTIVE_UNIFORM_BLOCKS 0x8A36
#  define GL_BUFFER_BINDING 0x9302
#  define GL_BUFFER_DATA_SIZE 0x9303
#  define GL_NUM_ACTIVE_VARIABLES 0x9304
#  define GL_SHADER_STORAGE_BLOCK 0x92E6
#  define GL_UNIFORM 0x92E1
#  define GL_UNIFORM_BLOCK 0x92E2
#  define GL_UNIFORM_BLOCK_INDEX 0x8A3A
#  define GL_UNIFORM_OFFSET 0x8A3B
#  define GL_UNIFORM_ARRAY_STRIDE 0x8A3C
#  define GL_UNIFORM_MATRIX_STRIDE 0x8A3D
#  define GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS 0x8A42
#  define GL_UNIFORM_BLOCK_BINDING 0x8A3F
#  define GL_UNIFORM_BLOCK_DATA_SIZE 0x8A40
# endif

namespace Qt3DRender {
namespace Render {

GraphicsHelperGL3_2::GraphicsHelperGL3_2()
    : m_funcs(nullptr)
    , m_tessFuncs()
{
}

GraphicsHelperGL3_2::~GraphicsHelperGL3_2()
{
}

void GraphicsHelperGL3_2::initializeHelper(QOpenGLContext *context,
                                          QAbstractOpenGLFunctions *functions)
{
    m_funcs = static_cast<QOpenGLFunctions_3_2_Core*>(functions);
    const bool ok = m_funcs->initializeOpenGLFunctions();
    Q_ASSERT(ok);
    Q_UNUSED(ok);

    if (context->hasExtension(QByteArrayLiteral("GL_ARB_tessellation_shader"))) {
        m_tessFuncs.reset(new QOpenGLExtension_ARB_tessellation_shader);
        m_tessFuncs->initializeOpenGLFunctions();
    }
}

void GraphicsHelperGL3_2::drawElementsInstancedBaseVertexBaseInstance(GLenum primitiveType,
                                                                    GLsizei primitiveCount,
                                                                    GLint indexType,
                                                                    void *indices,
                                                                    GLsizei instances,
                                                                    GLint baseVertex,
                                                                    GLint baseInstance)
{
    if (baseInstance != 0)
        qWarning() << "glDrawElementsInstancedBaseVertexBaseInstance is not supported with OpenGL ES 2";

    // glDrawElements OpenGL 3.1 or greater
    m_funcs->glDrawElementsInstancedBaseVertex(primitiveType,
                                               primitiveCount,
                                               indexType,
                                               indices,
                                               instances,
                                               baseVertex);
}

void GraphicsHelperGL3_2::drawArraysInstanced(GLenum primitiveType,
                                             GLint first,
                                             GLsizei count,
                                             GLsizei instances)
{
    // glDrawArraysInstanced OpenGL 3.1 or greater
    m_funcs->glDrawArraysInstanced(primitiveType,
                                   first,
                                   count,
                                   instances);
}

void GraphicsHelperGL3_2::drawArraysInstancedBaseInstance(GLenum primitiveType, GLint first, GLsizei count, GLsizei instances, GLsizei baseInstance)
{
    if (baseInstance != 0)
        qWarning() << "glDrawArraysInstancedBaseInstance is not supported with OpenGL 3";
    m_funcs->glDrawArraysInstanced(primitiveType,
                                   first,
                                   count,
                                   instances);
}

void GraphicsHelperGL3_2::drawElements(GLenum primitiveType,
                                      GLsizei primitiveCount,
                                      GLint indexType,
                                      void *indices,
                                      GLint baseVertex)
{
    m_funcs->glDrawElementsBaseVertex(primitiveType,
                                      primitiveCount,
                                      indexType,
                                      indices,
                                      baseVertex);
}

void GraphicsHelperGL3_2::drawArrays(GLenum primitiveType,
                                    GLint first,
                                    GLsizei count)
{
    m_funcs->glDrawArrays(primitiveType,
                          first,
                          count);
}

void GraphicsHelperGL3_2::setVerticesPerPatch(GLint verticesPerPatch)
{
#if defined(QT_OPENGL_4)
    if (!m_tessFuncs) {
        qWarning() << "Tessellation not supported with OpenGL 3 without GL_ARB_tessellation_shader";
        return;
    }

    m_tessFuncs->glPatchParameteri(GL_PATCH_VERTICES, verticesPerPatch);
#else
    Q_UNUSED(verticesPerPatch);
    qWarning() << "Tessellation not supported";
#endif
}

void GraphicsHelperGL3_2::useProgram(GLuint programId)
{
    m_funcs->glUseProgram(programId);
}

QVector<ShaderUniform> GraphicsHelperGL3_2::programUniformsAndLocations(GLuint programId)
{
    QVector<ShaderUniform> uniforms;

    GLint nbrActiveUniforms = 0;
    m_funcs->glGetProgramiv(programId, GL_ACTIVE_UNIFORMS, &nbrActiveUniforms);
    uniforms.reserve(nbrActiveUniforms);
    char uniformName[256];
    for (GLint i = 0; i < nbrActiveUniforms; i++) {
        ShaderUniform uniform;
        GLsizei uniformNameLength = 0;
        // Size is 1 for scalar and more for struct or arrays
        // Type is the GL Type
        m_funcs->glGetActiveUniform(programId, i, sizeof(uniformName) - 1, &uniformNameLength,
                                    &uniform.m_size, &uniform.m_type, uniformName);
        uniformName[sizeof(uniformName) - 1] = '\0';
        uniform.m_location = m_funcs->glGetUniformLocation(programId, uniformName);
        uniform.m_name = QString::fromUtf8(uniformName, uniformNameLength);
        m_funcs->glGetActiveUniformsiv(programId, 1, (GLuint*)&i, GL_UNIFORM_BLOCK_INDEX, &uniform.m_blockIndex);
        m_funcs->glGetActiveUniformsiv(programId, 1, (GLuint*)&i, GL_UNIFORM_OFFSET, &uniform.m_offset);
        m_funcs->glGetActiveUniformsiv(programId, 1, (GLuint*)&i, GL_UNIFORM_ARRAY_STRIDE, &uniform.m_arrayStride);
        m_funcs->glGetActiveUniformsiv(programId, 1, (GLuint*)&i, GL_UNIFORM_MATRIX_STRIDE, &uniform.m_matrixStride);
        uniform.m_rawByteSize = uniformByteSize(uniform);
        uniforms.append(uniform);
        qCDebug(Render::Rendering) << uniform.m_name << "size" << uniform.m_size
                                   << " offset" << uniform.m_offset
                                   << " rawSize" << uniform.m_rawByteSize;
    }

    return uniforms;
}

QVector<ShaderAttribute> GraphicsHelperGL3_2::programAttributesAndLocations(GLuint programId)
{
    QVector<ShaderAttribute> attributes;
    GLint nbrActiveAttributes = 0;
    m_funcs->glGetProgramiv(programId, GL_ACTIVE_ATTRIBUTES, &nbrActiveAttributes);
    attributes.reserve(nbrActiveAttributes);
    char attributeName[256];
    for (GLint i = 0; i < nbrActiveAttributes; i++) {
        ShaderAttribute attribute;
        GLsizei attributeNameLength = 0;
        // Size is 1 for scalar and more for struct or arrays
        // Type is the GL Type
        m_funcs->glGetActiveAttrib(programId, i, sizeof(attributeName) - 1, &attributeNameLength,
                                   &attribute.m_size, &attribute.m_type, attributeName);
        attributeName[sizeof(attributeName) - 1] = '\0';
        attribute.m_location = m_funcs->glGetAttribLocation(programId, attributeName);
        attribute.m_name = QString::fromUtf8(attributeName, attributeNameLength);
        attributes.append(attribute);
    }
    return attributes;
}

QVector<ShaderUniformBlock> GraphicsHelperGL3_2::programUniformBlocks(GLuint programId)
{
    QVector<ShaderUniformBlock> blocks;
    GLint nbrActiveUniformsBlocks = 0;
    m_funcs->glGetProgramiv(programId, GL_ACTIVE_UNIFORM_BLOCKS, &nbrActiveUniformsBlocks);
    blocks.reserve(nbrActiveUniformsBlocks);
    for (GLint i = 0; i < nbrActiveUniformsBlocks; i++) {
        QByteArray uniformBlockName(256, '\0');
        GLsizei length = 0;
        ShaderUniformBlock uniformBlock;
        m_funcs->glGetActiveUniformBlockName(programId, i, 256, &length, uniformBlockName.data());
        uniformBlock.m_name = QString::fromUtf8(uniformBlockName.left(length));
        uniformBlock.m_index = i;
        m_funcs->glGetActiveUniformBlockiv(programId, i, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &uniformBlock.m_activeUniformsCount);
        m_funcs->glGetActiveUniformBlockiv(programId, i, GL_UNIFORM_BLOCK_BINDING, &uniformBlock.m_binding);
        m_funcs->glGetActiveUniformBlockiv(programId, i, GL_UNIFORM_BLOCK_DATA_SIZE, &uniformBlock.m_size);
        blocks.append(uniformBlock);
    }
    return blocks;
}

QVector<ShaderStorageBlock> GraphicsHelperGL3_2::programShaderStorageBlocks(GLuint programId)
{
    Q_UNUSED(programId);
    QVector<ShaderStorageBlock> blocks;
    qWarning() << "SSBO are not supported by OpenGL 3.2 (since OpenGL 4.3)";
    return blocks;
}

void GraphicsHelperGL3_2::vertexAttribDivisor(GLuint index, GLuint divisor)
{
    Q_UNUSED(index);
    Q_UNUSED(divisor);
    qCWarning(Render::Rendering) << "Vertex attribute divisor not available with OpenGL 3.2 core";
}

void GraphicsHelperGL3_2::blendEquation(GLenum mode)
{
    m_funcs->glBlendEquation(mode);
}

void GraphicsHelperGL3_2::blendFunci(GLuint buf, GLenum sfactor, GLenum dfactor)
{
    Q_UNUSED(buf);
    Q_UNUSED(sfactor);
    Q_UNUSED(dfactor);

    qWarning() << "glBlendFunci() not supported by OpenGL 3.0 (since OpenGL 4.0)";
}

void GraphicsHelperGL3_2::blendFuncSeparatei(GLuint buf, GLenum sRGB, GLenum dRGB, GLenum sAlpha, GLenum dAlpha)
{
    Q_UNUSED(buf);
    Q_UNUSED(sRGB);
    Q_UNUSED(dRGB);
    Q_UNUSED(sAlpha);
    Q_UNUSED(dAlpha);

    qWarning() << "glBlendFuncSeparatei() not supported by OpenGL 3.0 (since OpenGL 4.0)";
}

void GraphicsHelperGL3_2::alphaTest(GLenum, GLenum)
{
    qCWarning(Render::Rendering) << "AlphaTest not available with OpenGL 3.2 core";
}

void GraphicsHelperGL3_2::depthTest(GLenum mode)
{
    m_funcs->glEnable(GL_DEPTH_TEST);
    m_funcs->glDepthFunc(mode);
}

void GraphicsHelperGL3_2::depthMask(GLenum mode)
{
    m_funcs->glDepthMask(mode);
}

void GraphicsHelperGL3_2::frontFace(GLenum mode)
{
    m_funcs->glFrontFace(mode);

}

void GraphicsHelperGL3_2::setMSAAEnabled(bool enabled)
{
    enabled ? m_funcs->glEnable(GL_MULTISAMPLE)
            : m_funcs->glDisable(GL_MULTISAMPLE);
}

void GraphicsHelperGL3_2::setAlphaCoverageEnabled(bool enabled)
{
    enabled ? m_funcs->glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE)
            : m_funcs->glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
}

GLuint GraphicsHelperGL3_2::createFrameBufferObject()
{
    GLuint id;
    m_funcs->glGenFramebuffers(1, &id);
    return id;
}

void GraphicsHelperGL3_2::releaseFrameBufferObject(GLuint frameBufferId)
{
    m_funcs->glDeleteFramebuffers(1, &frameBufferId);
}

void GraphicsHelperGL3_2::bindFrameBufferObject(GLuint frameBufferId)
{
    m_funcs->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBufferId);
}

GLuint GraphicsHelperGL3_2::boundFrameBufferObject()
{
    GLint id = 0;
    m_funcs->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &id);
    return id;
}

bool GraphicsHelperGL3_2::checkFrameBufferComplete()
{
    return (m_funcs->glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
}

void GraphicsHelperGL3_2::bindFrameBufferAttachment(QOpenGLTexture *texture, const Attachment &attachment)
{
    GLenum attr = GL_DEPTH_STENCIL_ATTACHMENT;

    if (attachment.m_point <= QRenderTargetOutput::Color15)
        attr = GL_COLOR_ATTACHMENT0 + attachment.m_point;
    else if (attachment.m_point == QRenderTargetOutput::Depth)
        attr = GL_DEPTH_ATTACHMENT;
    else if (attachment.m_point == QRenderTargetOutput::Stencil)
        attr = GL_STENCIL_ATTACHMENT;

    texture->bind();
    QOpenGLTexture::Target target = texture->target();
    if (target == QOpenGLTexture::Target1DArray || target == QOpenGLTexture::Target2DArray ||
            target == QOpenGLTexture::Target2DMultisampleArray || target == QOpenGLTexture::Target3D)
        m_funcs->glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, attr, texture->textureId(), attachment.m_mipLevel, attachment.m_layer);
    else if (target == QOpenGLTexture::TargetCubeMapArray)
        m_funcs->glFramebufferTexture3D(GL_DRAW_FRAMEBUFFER, attr, attachment.m_face, texture->textureId(), attachment.m_mipLevel, attachment.m_layer);
    else if (target == QOpenGLTexture::TargetCubeMap)
        m_funcs->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attr, attachment.m_face, texture->textureId(), attachment.m_mipLevel);
    else
        m_funcs->glFramebufferTexture(GL_DRAW_FRAMEBUFFER, attr, texture->textureId(), attachment.m_mipLevel);
    texture->release();
}

bool GraphicsHelperGL3_2::supportsFeature(GraphicsHelperInterface::Feature feature) const
{
    switch (feature) {
    case MRT:
    case UniformBufferObject:
    case PrimitiveRestart:
    case RenderBufferDimensionRetrieval:
    case TextureDimensionRetrieval:
    case BindableFragmentOutputs:
    case BlitFramebuffer:
        return true;
    case Tessellation:
        return !m_tessFuncs.isNull();
    default:
        return false;
    }
}

void GraphicsHelperGL3_2::drawBuffers(GLsizei n, const int *bufs)
{
    // Use QVarLengthArray here
    QVarLengthArray<GLenum, 16> drawBufs(n);

    for (int i = 0; i < n; i++)
        drawBufs[i] = GL_COLOR_ATTACHMENT0 + bufs[i];
    m_funcs->glDrawBuffers(n, drawBufs.constData());
}

void GraphicsHelperGL3_2::bindFragDataLocation(GLuint shader, const QHash<QString, int> &outputs)
{
    for (auto it = outputs.begin(), end = outputs.end(); it != end; ++it)
        m_funcs->glBindFragDataLocation(shader, it.value(), it.key().toStdString().c_str());
}

void GraphicsHelperGL3_2::bindUniformBlock(GLuint programId, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    m_funcs->glUniformBlockBinding(programId, uniformBlockIndex, uniformBlockBinding);
}

void GraphicsHelperGL3_2::bindShaderStorageBlock(GLuint programId, GLuint shaderStorageBlockIndex, GLuint shaderStorageBlockBinding)
{
    Q_UNUSED(programId);
    Q_UNUSED(shaderStorageBlockIndex);
    Q_UNUSED(shaderStorageBlockBinding);
    qWarning() << "SSBO are not supported by OpenGL 3.0 (since OpenGL 4.3)";
}

void GraphicsHelperGL3_2::bindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
    m_funcs->glBindBufferBase(target, index, buffer);
}

void GraphicsHelperGL3_2::buildUniformBuffer(const QVariant &v, const ShaderUniform &description, QByteArray &buffer)
{
    char *bufferData = buffer.data();

    switch (description.m_type) {

    case GL_FLOAT: {
        const GLfloat *data = QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 1);
        QGraphicsUtils::fillDataArray(bufferData, data, description, 1);
        break;
    }

    case GL_FLOAT_VEC2: {
        const GLfloat *data = QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 2);
        QGraphicsUtils::fillDataArray(bufferData, data, description, 2);
        break;
    }

    case GL_FLOAT_VEC3: {
        const GLfloat *data = QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 3);
        QGraphicsUtils::fillDataArray(bufferData, data, description, 3);
        break;
    }

    case GL_FLOAT_VEC4: {
        const GLfloat *data = QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 4);
        QGraphicsUtils::fillDataArray(bufferData, data, description, 4);
        break;
    }

    case GL_FLOAT_MAT2: {
        const GLfloat *data = QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 4);
        QGraphicsUtils::fillDataMatrixArray(bufferData, data, description, 2, 2);
        break;
    }

    case GL_FLOAT_MAT2x3: {
        const GLfloat *data = QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 6);
        QGraphicsUtils::fillDataMatrixArray(bufferData, data, description, 2, 3);
        break;
    }

    case GL_FLOAT_MAT2x4: {
        const GLfloat *data = QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 8);
        QGraphicsUtils::fillDataMatrixArray(bufferData, data, description, 2, 4);
        break;
    }

    case GL_FLOAT_MAT3: {
        const GLfloat *data = QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 9);
        QGraphicsUtils::fillDataMatrixArray(bufferData, data, description, 3, 3);
        break;
    }

    case GL_FLOAT_MAT3x2: {
        const GLfloat *data = QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 6);
        QGraphicsUtils::fillDataMatrixArray(bufferData, data, description, 3, 2);
        break;
    }

    case GL_FLOAT_MAT3x4: {
        const GLfloat *data = QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 12);
        QGraphicsUtils::fillDataMatrixArray(bufferData, data, description, 3, 4);
        break;
    }

    case GL_FLOAT_MAT4: {
        const GLfloat *data = QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 16);
        QGraphicsUtils::fillDataMatrixArray(bufferData, data, description, 4, 4);
        break;
    }

    case GL_FLOAT_MAT4x2: {
        const GLfloat *data = QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 8);
        QGraphicsUtils::fillDataMatrixArray(bufferData, data, description, 4, 2);
        break;
    }

    case GL_FLOAT_MAT4x3: {
        const GLfloat *data = QGraphicsUtils::valueArrayFromVariant<GLfloat>(v, description.m_size, 12);
        QGraphicsUtils::fillDataMatrixArray(bufferData, data, description, 4, 3);
        break;
    }

    case GL_INT: {
        const GLint *data = QGraphicsUtils::valueArrayFromVariant<GLint>(v, description.m_size, 1);
        QGraphicsUtils::fillDataArray(bufferData, data, description, 1);
        break;
    }

    case GL_INT_VEC2: {
        const GLint *data = QGraphicsUtils::valueArrayFromVariant<GLint>(v, description.m_size, 2);
        QGraphicsUtils::fillDataArray(bufferData, data, description, 2);
        break;
    }

    case GL_INT_VEC3: {
        const GLint *data = QGraphicsUtils::valueArrayFromVariant<GLint>(v, description.m_size, 3);
        QGraphicsUtils::fillDataArray(bufferData, data, description, 3);
        break;
    }

    case GL_INT_VEC4: {
        const GLint *data = QGraphicsUtils::valueArrayFromVariant<GLint>(v, description.m_size, 4);
        QGraphicsUtils::fillDataArray(bufferData, data, description, 4);
        break;
    }

    case GL_UNSIGNED_INT: {
        const GLuint *data = QGraphicsUtils::valueArrayFromVariant<GLuint>(v, description.m_size, 1);
        QGraphicsUtils::fillDataArray(bufferData, data, description, 1);
        break;
    }

    case GL_UNSIGNED_INT_VEC2: {
        const GLuint *data = QGraphicsUtils::valueArrayFromVariant<GLuint>(v, description.m_size, 2);
        QGraphicsUtils::fillDataArray(bufferData, data, description, 2);
        break;
    }

    case GL_UNSIGNED_INT_VEC3: {
        const GLuint *data = QGraphicsUtils::valueArrayFromVariant<GLuint>(v, description.m_size, 3);
        QGraphicsUtils::fillDataArray(bufferData, data, description, 3);
        break;
    }

    case GL_UNSIGNED_INT_VEC4: {
        const GLuint *data = QGraphicsUtils::valueArrayFromVariant<GLuint>(v, description.m_size, 4);
        QGraphicsUtils::fillDataArray(bufferData, data, description, 4);
        break;
    }

    case GL_BOOL: {
        const GLboolean *data = QGraphicsUtils::valueArrayFromVariant<GLboolean>(v, description.m_size, 1);
        QGraphicsUtils::fillDataArray(bufferData, data, description, 1);
        break;
    }

    case GL_BOOL_VEC2: {
        const GLboolean *data = QGraphicsUtils::valueArrayFromVariant<GLboolean>(v, description.m_size, 2);
        QGraphicsUtils::fillDataArray(bufferData, data, description, 2);
        break;
    }

    case GL_BOOL_VEC3: {
        const GLboolean *data = QGraphicsUtils::valueArrayFromVariant<GLboolean>(v, description.m_size, 3);
        QGraphicsUtils::fillDataArray(bufferData, data, description, 3);
        break;
    }

    case GL_BOOL_VEC4: {
        const GLboolean *data = QGraphicsUtils::valueArrayFromVariant<GLboolean>(v, description.m_size, 4);
        QGraphicsUtils::fillDataArray(bufferData, data, description, 4);
        break;
    }

    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_BUFFER:
    case GL_SAMPLER_2D_RECT:
    case GL_INT_SAMPLER_1D:
    case GL_INT_SAMPLER_2D:
    case GL_INT_SAMPLER_3D:
    case GL_INT_SAMPLER_CUBE:
    case GL_INT_SAMPLER_BUFFER:
    case GL_INT_SAMPLER_2D_RECT:
    case GL_UNSIGNED_INT_SAMPLER_1D:
    case GL_UNSIGNED_INT_SAMPLER_2D:
    case GL_UNSIGNED_INT_SAMPLER_3D:
    case GL_UNSIGNED_INT_SAMPLER_CUBE:
    case GL_UNSIGNED_INT_SAMPLER_BUFFER:
    case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_CUBE_SHADOW:
    case GL_SAMPLER_1D_ARRAY:
    case GL_SAMPLER_2D_ARRAY:
    case GL_INT_SAMPLER_1D_ARRAY:
    case GL_INT_SAMPLER_2D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
    case GL_SAMPLER_1D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_RECT_SHADOW:
    case GL_SAMPLER_2D_MULTISAMPLE:
    case GL_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: {
        Q_ASSERT(description.m_size == 1);
        int value = v.toInt();
        QGraphicsUtils::fillDataArray<GLint>(bufferData, &value, description, 1);
        break;
    }

    default:
        qWarning() << Q_FUNC_INFO << "unsupported uniform type:" << description.m_type << "for " << description.m_name;
        break;
    }
}

uint GraphicsHelperGL3_2::uniformByteSize(const ShaderUniform &description)
{
    uint rawByteSize = 0;
    int arrayStride = qMax(description.m_arrayStride, 0);
    int matrixStride = qMax(description.m_matrixStride, 0);

    switch (description.m_type) {

    case GL_FLOAT_VEC2:
    case GL_INT_VEC2:
    case GL_UNSIGNED_INT_VEC2:
        rawByteSize = 8;
        break;

    case GL_FLOAT_VEC3:
    case GL_INT_VEC3:
    case GL_UNSIGNED_INT_VEC3:
        rawByteSize = 12;
        break;

    case GL_FLOAT_VEC4:
    case GL_INT_VEC4:
    case GL_UNSIGNED_INT_VEC4:
        rawByteSize = 16;
        break;

    case GL_FLOAT_MAT2:
        rawByteSize = matrixStride ? 2 * matrixStride : 16;
        break;

    case GL_FLOAT_MAT2x4:
        rawByteSize = matrixStride ? 2 * matrixStride : 32;
        break;

    case GL_FLOAT_MAT4x2:
        rawByteSize = matrixStride ? 4 * matrixStride : 32;
        break;

    case GL_FLOAT_MAT3:
        rawByteSize = matrixStride ? 3 * matrixStride : 36;
        break;

    case GL_FLOAT_MAT2x3:
        rawByteSize = matrixStride ? 2 * matrixStride : 24;
        break;

    case GL_FLOAT_MAT3x2:
        rawByteSize = matrixStride ? 3 * matrixStride : 24;
        break;

    case GL_FLOAT_MAT4:
        rawByteSize = matrixStride ? 4 * matrixStride : 64;
        break;

    case GL_FLOAT_MAT4x3:
        rawByteSize = matrixStride ? 4 * matrixStride : 48;
        break;

    case GL_FLOAT_MAT3x4:
        rawByteSize = matrixStride ? 3 * matrixStride : 48;
        break;

    case GL_BOOL:
        rawByteSize = 1;
        break;

    case GL_BOOL_VEC2:
        rawByteSize = 2;
        break;

    case GL_BOOL_VEC3:
        rawByteSize = 3;
        break;

    case GL_BOOL_VEC4:
        rawByteSize = 4;
        break;

    case GL_INT:
    case GL_FLOAT:
    case GL_UNSIGNED_INT:
    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_BUFFER:
    case GL_SAMPLER_2D_RECT:
    case GL_INT_SAMPLER_1D:
    case GL_INT_SAMPLER_2D:
    case GL_INT_SAMPLER_3D:
    case GL_INT_SAMPLER_CUBE:
    case GL_INT_SAMPLER_BUFFER:
    case GL_INT_SAMPLER_2D_RECT:
    case GL_UNSIGNED_INT_SAMPLER_1D:
    case GL_UNSIGNED_INT_SAMPLER_2D:
    case GL_UNSIGNED_INT_SAMPLER_3D:
    case GL_UNSIGNED_INT_SAMPLER_CUBE:
    case GL_UNSIGNED_INT_SAMPLER_BUFFER:
    case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_CUBE_SHADOW:
    case GL_SAMPLER_1D_ARRAY:
    case GL_SAMPLER_2D_ARRAY:
    case GL_INT_SAMPLER_1D_ARRAY:
    case GL_INT_SAMPLER_2D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
    case GL_SAMPLER_1D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_RECT_SHADOW:
    case GL_SAMPLER_2D_MULTISAMPLE:
    case GL_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
        rawByteSize = 4;
        break;
    }

    return arrayStride ? rawByteSize * arrayStride : rawByteSize;
}

void GraphicsHelperGL3_2::enableClipPlane(int clipPlane)
{
    m_funcs->glEnable(GL_CLIP_DISTANCE0 + clipPlane);
}

void GraphicsHelperGL3_2::disableClipPlane(int clipPlane)
{
    m_funcs->glDisable(GL_CLIP_DISTANCE0 + clipPlane);
}

void GraphicsHelperGL3_2::setClipPlane(int clipPlane, const QVector3D &normal, float distance)
{
    // deprecated
    Q_UNUSED(clipPlane);
    Q_UNUSED(normal);
    Q_UNUSED(distance);
}

GLint GraphicsHelperGL3_2::maxClipPlaneCount()
{
    GLint max = 0;
    m_funcs->glGetIntegerv(GL_MAX_CLIP_DISTANCES, &max);
    return max;
}

void GraphicsHelperGL3_2::enablePrimitiveRestart(int primitiveRestartIndex)
{
    m_funcs->glPrimitiveRestartIndex(primitiveRestartIndex);
    m_funcs->glEnable(GL_PRIMITIVE_RESTART);
}

void GraphicsHelperGL3_2::disablePrimitiveRestart()
{
    m_funcs->glDisable(GL_PRIMITIVE_RESTART);
}

void GraphicsHelperGL3_2::clearBufferf(GLint drawbuffer, const QVector4D &values)
{
    GLfloat vec[4] = {values[0], values[1], values[2], values[3]};
    m_funcs->glClearBufferfv(GL_COLOR, drawbuffer, vec);
}

void GraphicsHelperGL3_2::pointSize(bool programmable, GLfloat value)
{
    if (programmable) {
        m_funcs->glEnable(GL_PROGRAM_POINT_SIZE);
    } else {
        m_funcs->glDisable(GL_PROGRAM_POINT_SIZE);
        m_funcs->glPointSize(value);
    }
}

void GraphicsHelperGL3_2::enablei(GLenum cap, GLuint index)
{
    m_funcs->glEnablei(cap, index);
}

void GraphicsHelperGL3_2::disablei(GLenum cap, GLuint index)
{
    m_funcs->glDisablei(cap, index);
}

void GraphicsHelperGL3_2::setSeamlessCubemap(bool enable)
{
    if (enable)
        m_funcs->glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    else
        m_funcs->glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

QSize GraphicsHelperGL3_2::getRenderBufferDimensions(GLuint renderBufferId)
{
    GLint width = 0;
    GLint height = 0;

    m_funcs->glBindRenderbuffer(GL_RENDERBUFFER, renderBufferId);
    m_funcs->glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
    m_funcs->glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
    m_funcs->glBindRenderbuffer(GL_RENDERBUFFER, 0);

    return QSize(width, height);
}

QSize GraphicsHelperGL3_2::getTextureDimensions(GLuint textureId, GLenum target, uint level)
{
    GLint width = 0;
    GLint height = 0;

    m_funcs->glBindTexture(target, textureId);
    m_funcs->glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &width);
    m_funcs->glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &height);
    m_funcs->glBindTexture(target, 0);

    return QSize(width, height);
}

void GraphicsHelperGL3_2::dispatchCompute(GLuint wx, GLuint wy, GLuint wz)
{
    Q_UNUSED(wx);
    Q_UNUSED(wy);
    Q_UNUSED(wz);
    qWarning() << "Compute Shaders are not supported by OpenGL 3.2 (since OpenGL 4.3)";
}

void GraphicsHelperGL3_2::glUniform1fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniform1fv(location, count, values);
}

void GraphicsHelperGL3_2::glUniform2fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniform2fv(location, count, values);
}

void GraphicsHelperGL3_2::glUniform3fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniform3fv(location, count, values);
}

void GraphicsHelperGL3_2::glUniform4fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniform4fv(location, count, values);
}

void GraphicsHelperGL3_2::glUniform1iv(GLint location, GLsizei count, const GLint *values)
{
    m_funcs->glUniform1iv(location, count, values);
}

void GraphicsHelperGL3_2::glUniform2iv(GLint location, GLsizei count, const GLint *values)
{
    m_funcs->glUniform2iv(location, count, values);
}

void GraphicsHelperGL3_2::glUniform3iv(GLint location, GLsizei count, const GLint *values)
{
    m_funcs->glUniform3iv(location, count, values);
}

void GraphicsHelperGL3_2::glUniform4iv(GLint location, GLsizei count, const GLint *values)
{
    m_funcs->glUniform4iv(location, count, values);
}

void GraphicsHelperGL3_2::glUniform1uiv(GLint location, GLsizei count, const GLuint *values)
{
    m_funcs->glUniform1uiv(location, count, values);
}

void GraphicsHelperGL3_2::glUniform2uiv(GLint location, GLsizei count, const GLuint *values)
{
    m_funcs->glUniform2uiv(location, count, values);
}

void GraphicsHelperGL3_2::glUniform3uiv(GLint location, GLsizei count, const GLuint *values)
{
    m_funcs->glUniform3uiv(location, count, values);
}

void GraphicsHelperGL3_2::glUniform4uiv(GLint location, GLsizei count, const GLuint *values)
{
    m_funcs->glUniform4uiv(location, count, values);
}

void GraphicsHelperGL3_2::glUniformMatrix2fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniformMatrix2fv(location, count, false, values);
}

void GraphicsHelperGL3_2::glUniformMatrix3fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniformMatrix3fv(location, count, false, values);
}

void GraphicsHelperGL3_2::glUniformMatrix4fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniformMatrix4fv(location, count, false, values);
}

void GraphicsHelperGL3_2::glUniformMatrix2x3fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniformMatrix2x3fv(location, count, false, values);
}

void GraphicsHelperGL3_2::glUniformMatrix3x2fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniformMatrix3x2fv(location, count, false, values);
}

void GraphicsHelperGL3_2::glUniformMatrix2x4fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniformMatrix2x4fv(location, count, false, values);
}

void GraphicsHelperGL3_2::glUniformMatrix4x2fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniformMatrix4x2fv(location, count, false, values);
}

void GraphicsHelperGL3_2::glUniformMatrix3x4fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniformMatrix3x4fv(location, count, false, values);
}

void GraphicsHelperGL3_2::glUniformMatrix4x3fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniformMatrix4x3fv(location, count, false, values);
}

UniformType GraphicsHelperGL3_2::uniformTypeFromGLType(GLenum type)
{
    switch (type) {
    case GL_FLOAT:
        return UniformType::Float;
    case GL_FLOAT_VEC2:
        return UniformType::Vec2;
    case GL_FLOAT_VEC3:
        return UniformType::Vec3;
    case GL_FLOAT_VEC4:
        return UniformType::Vec4;
    case GL_FLOAT_MAT2:
        return UniformType::Mat2;
    case GL_FLOAT_MAT3:
        return UniformType::Mat3;
    case GL_FLOAT_MAT4:
        return UniformType::Mat4;
    case GL_FLOAT_MAT2x3:
        return UniformType::Mat2x3;
    case GL_FLOAT_MAT3x2:
        return UniformType::Mat3x2;
    case GL_FLOAT_MAT2x4:
        return UniformType::Mat2x4;
    case GL_FLOAT_MAT4x2:
        return UniformType::Mat4x2;
    case GL_FLOAT_MAT3x4:
        return UniformType::Mat3x4;
    case GL_FLOAT_MAT4x3:
        return UniformType::Mat4x3;
    case GL_INT:
        return UniformType::Int;
    case GL_INT_VEC2:
        return UniformType::IVec2;
    case GL_INT_VEC3:
        return UniformType::IVec3;
    case GL_INT_VEC4:
        return UniformType::IVec4;
    case GL_UNSIGNED_INT:
        return UniformType::UInt;
    case GL_UNSIGNED_INT_VEC2:
        return UniformType::UIVec2;
    case GL_UNSIGNED_INT_VEC3:
        return UniformType::UIVec3;
    case GL_UNSIGNED_INT_VEC4:
        return UniformType::UIVec4;
    case GL_BOOL:
        return UniformType::Bool;
    case GL_BOOL_VEC2:
        return UniformType::BVec2;
    case GL_BOOL_VEC3:
        return UniformType::BVec3;
    case GL_BOOL_VEC4:
        return UniformType::BVec4;

    case GL_SAMPLER_BUFFER:
    case GL_SAMPLER_1D:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_2D_RECT:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_2D_RECT_SHADOW:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_CUBE_SHADOW:
    case GL_SAMPLER_1D_ARRAY:
    case GL_SAMPLER_2D_ARRAY:
    case GL_SAMPLER_2D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_MULTISAMPLE:
    case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_SAMPLER_3D:
    case GL_INT_SAMPLER_BUFFER:
    case GL_INT_SAMPLER_1D:
    case GL_INT_SAMPLER_2D:
    case GL_INT_SAMPLER_3D:
    case GL_INT_SAMPLER_CUBE:
    case GL_INT_SAMPLER_1D_ARRAY:
    case GL_INT_SAMPLER_2D_ARRAY:
    case GL_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_BUFFER:
    case GL_UNSIGNED_INT_SAMPLER_1D:
    case GL_UNSIGNED_INT_SAMPLER_2D:
    case GL_UNSIGNED_INT_SAMPLER_3D:
    case GL_UNSIGNED_INT_SAMPLER_CUBE:
    case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
        return UniformType::Sampler;
    default:
        Q_UNREACHABLE();
        return UniformType::Float;
    }
}

void GraphicsHelperGL3_2::blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    m_funcs->glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // !QT_OPENGL_ES_2
