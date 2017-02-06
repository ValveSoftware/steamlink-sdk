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

#include "graphicshelperes2_p.h"
#include <Qt3DRender/private/renderlogging_p.h>
#include <private/attachmentpack_p.h>
#include <private/qgraphicsutils_p.h>
#include <QtGui/private/qopenglextensions_p.h>

QT_BEGIN_NAMESPACE

// ES 3.0+
#ifndef GL_SAMPLER_3D
#define GL_SAMPLER_3D                     0x8B5F
#endif
#ifndef GL_SAMPLER_2D_SHADOW
#define GL_SAMPLER_2D_SHADOW              0x8B62
#endif
#ifndef GL_SAMPLER_CUBE_SHADOW
#define GL_SAMPLER_CUBE_SHADOW            0x8DC5
#endif
#ifndef GL_SAMPLER_2D_ARRAY
#define GL_SAMPLER_2D_ARRAY               0x8DC1
#endif
#ifndef GL_SAMPLER_2D_ARRAY_SHADOW
#define GL_SAMPLER_2D_ARRAY_SHADOW        0x8DC4
#endif

namespace Qt3DRender {
namespace Render {

GraphicsHelperES2::GraphicsHelperES2() :
    m_funcs(0)
{
}

GraphicsHelperES2::~GraphicsHelperES2()
{
}

void GraphicsHelperES2::initializeHelper(QOpenGLContext *context,
                                          QAbstractOpenGLFunctions *)
{
    Q_ASSERT(context);
    m_funcs = context->functions();
    Q_ASSERT(m_funcs);
}

void GraphicsHelperES2::drawElementsInstancedBaseVertexBaseInstance(GLenum primitiveType,
                                                                    GLsizei primitiveCount,
                                                                    GLint indexType,
                                                                    void *indices,
                                                                    GLsizei instances,
                                                                    GLint baseVertex,
                                                                    GLint baseInstance)
{
    if (baseInstance != 0)
        qWarning() << "glDrawElementsInstancedBaseVertexBaseInstance is not supported with OpenGL ES 2";

    if (baseVertex != 0)
        qWarning() << "glDrawElementsInstancedBaseVertex is not supported with OpenGL ES 2";

    for (GLint i = 0; i < instances; i++)
        drawElements(primitiveType,
                     primitiveCount,
                     indexType,
                     indices);
}

void GraphicsHelperES2::drawArraysInstanced(GLenum primitiveType,
                                             GLint first,
                                             GLsizei count,
                                             GLsizei instances)
{
    for (GLint i = 0; i < instances; i++)
        drawArrays(primitiveType,
                   first,
                   count);
}

void GraphicsHelperES2::drawArraysInstancedBaseInstance(GLenum primitiveType, GLint first, GLsizei count, GLsizei instances, GLsizei baseInstance)
{
    if (baseInstance != 0)
        qWarning() << "glDrawArraysInstancedBaseInstance is not supported with OpenGL ES 2";
    for (GLint i = 0; i < instances; i++)
        drawArrays(primitiveType,
                   first,
                   count);
}

void GraphicsHelperES2::drawElements(GLenum primitiveType,
                                      GLsizei primitiveCount,
                                      GLint indexType,
                                      void *indices,
                                      GLint baseVertex)
{
    if (baseVertex != 0)
        qWarning() << "glDrawElementsBaseVertex is not supported with OpenGL ES 2";
    QOpenGLExtensions *xfuncs = static_cast<QOpenGLExtensions *>(m_funcs);
    if (indexType == GL_UNSIGNED_INT && !xfuncs->hasOpenGLExtension(QOpenGLExtensions::ElementIndexUint)) {
        static bool warnShown = false;
        if (!warnShown) {
            warnShown = true;
            qWarning("GL_UNSIGNED_INT index type not supported on this system, skipping draw call.");
        }
        return;
    }
    m_funcs->glDrawElements(primitiveType,
                            primitiveCount,
                            indexType,
                            indices);
}

void GraphicsHelperES2::drawArrays(GLenum primitiveType,
                                    GLint first,
                                    GLsizei count)
{
    m_funcs->glDrawArrays(primitiveType,
                          first,
                          count);
}

void GraphicsHelperES2::setVerticesPerPatch(GLint verticesPerPatch)
{
    Q_UNUSED(verticesPerPatch);
    qWarning() << "Tessellation not supported with OpenGL ES 2";
}

void GraphicsHelperES2::useProgram(GLuint programId)
{
    m_funcs->glUseProgram(programId);
}

QVector<ShaderUniform> GraphicsHelperES2::programUniformsAndLocations(GLuint programId)
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
        uniforms.append(uniform);
    }
    return uniforms;
}

QVector<ShaderAttribute> GraphicsHelperES2::programAttributesAndLocations(GLuint programId)
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

QVector<ShaderUniformBlock> GraphicsHelperES2::programUniformBlocks(GLuint programId)
{
    Q_UNUSED(programId);
    QVector<ShaderUniformBlock> blocks;
    qWarning() << "UBO are not supported by OpenGL ES 2.0 (since OpenGL ES 3.0)";
    return blocks;
}

QVector<ShaderStorageBlock> GraphicsHelperES2::programShaderStorageBlocks(GLuint programId)
{
    Q_UNUSED(programId);
    QVector<ShaderStorageBlock> blocks;
    qWarning() << "SSBO are not supported by OpenGL ES 2.0 (since OpenGL ES 3.1)";
    return blocks;
}

void GraphicsHelperES2::vertexAttribDivisor(GLuint index, GLuint divisor)
{
    Q_UNUSED(index);
    Q_UNUSED(divisor);
}

void GraphicsHelperES2::blendEquation(GLenum mode)
{
    m_funcs->glBlendEquation(mode);
}

void GraphicsHelperES2::blendFunci(GLuint buf, GLenum sfactor, GLenum dfactor)
{
    Q_UNUSED(buf);
    Q_UNUSED(sfactor);
    Q_UNUSED(dfactor);

    qWarning() << "glBlendFunci() not supported by OpenGL ES 2.0";
}

void GraphicsHelperES2::blendFuncSeparatei(GLuint buf, GLenum sRGB, GLenum dRGB, GLenum sAlpha, GLenum dAlpha)
{
    Q_UNUSED(buf);
    Q_UNUSED(sRGB);
    Q_UNUSED(dRGB);
    Q_UNUSED(sAlpha);
    Q_UNUSED(dAlpha);

    qWarning() << "glBlendFuncSeparatei() not supported by OpenGL ES 2.0";
}

void GraphicsHelperES2::alphaTest(GLenum, GLenum)
{
    qCWarning(Render::Rendering) << Q_FUNC_INFO << "AlphaTest not available with OpenGL ES 2.0";
}

void GraphicsHelperES2::depthTest(GLenum mode)
{
    m_funcs->glEnable(GL_DEPTH_TEST);
    m_funcs->glDepthFunc(mode);
}

void GraphicsHelperES2::depthMask(GLenum mode)
{
    m_funcs->glDepthMask(mode);
}

void GraphicsHelperES2::frontFace(GLenum mode)
{
    m_funcs->glFrontFace(mode);
}

void GraphicsHelperES2::setMSAAEnabled(bool enabled)
{
    Q_UNUSED(enabled);
    qWarning() << "MSAA not available with OpenGL ES 2.0";
}

void GraphicsHelperES2::setAlphaCoverageEnabled(bool enabled)
{
    enabled ? m_funcs->glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE)
            : m_funcs->glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
}

GLuint GraphicsHelperES2::createFrameBufferObject()
{
    GLuint id;
    m_funcs->glGenFramebuffers(1, &id);
    return id;
}

void GraphicsHelperES2::releaseFrameBufferObject(GLuint frameBufferId)
{
    m_funcs->glDeleteFramebuffers(1, &frameBufferId);
}

void GraphicsHelperES2::bindFrameBufferObject(GLuint frameBufferId)
{
    m_funcs->glBindFramebuffer(GL_FRAMEBUFFER, frameBufferId);
}

GLuint GraphicsHelperES2::boundFrameBufferObject()
{
    GLint id = 0;
    m_funcs->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &id);
    return id;
}

bool GraphicsHelperES2::checkFrameBufferComplete()
{
    return (m_funcs->glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
}

void GraphicsHelperES2::bindFrameBufferAttachment(QOpenGLTexture *texture, const Attachment &attachment)
{
    GLenum attr = GL_COLOR_ATTACHMENT0;

    if (attachment.m_point == QRenderTargetOutput::Color0)
        attr = GL_COLOR_ATTACHMENT0;
    else if (attachment.m_point == QRenderTargetOutput::Depth)
        attr = GL_DEPTH_ATTACHMENT;
    else if (attachment.m_point == QRenderTargetOutput::Stencil)
        attr = GL_STENCIL_ATTACHMENT;
    else
        qCritical() << "Unsupported FBO attachment OpenGL ES 2.0";

    const QOpenGLTexture::Target target = texture->target();

    if (target == QOpenGLTexture::TargetCubeMap && attachment.m_face == QAbstractTexture::AllFaces) {
        qWarning() << "OpenGL ES 2.0 doesn't handle attaching all the faces of a cube map texture at once to an FBO";
        return;
    }

    texture->bind();
    if (target == QOpenGLTexture::Target2D)
        m_funcs->glFramebufferTexture2D(GL_FRAMEBUFFER, attr, target, texture->textureId(), attachment.m_mipLevel);
    else if (target == QOpenGLTexture::TargetCubeMap)
        m_funcs->glFramebufferTexture2D(GL_FRAMEBUFFER, attr, attachment.m_face, texture->textureId(), attachment.m_mipLevel);
    else
        qCritical() << "Unsupported Texture FBO attachment format";
    texture->release();
}

bool GraphicsHelperES2::supportsFeature(GraphicsHelperInterface::Feature feature) const
{
    switch (feature) {
    case RenderBufferDimensionRetrieval:
        return true;
    default:
        return false;
    }
}
void GraphicsHelperES2::drawBuffers(GLsizei, const int *)
{
    qWarning() << "drawBuffers is not supported by ES 2.0";
}

void GraphicsHelperES2::bindFragDataLocation(GLuint , const QHash<QString, int> &)
{
    qCritical() << "bindFragDataLocation is not supported by ES 2.0";
}

void GraphicsHelperES2::bindUniformBlock(GLuint programId, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    Q_UNUSED(programId);
    Q_UNUSED(uniformBlockIndex);
    Q_UNUSED(uniformBlockBinding);
    qWarning() << "UBO are not supported by ES 2.0 (since ES 3.0)";
}

void GraphicsHelperES2::bindShaderStorageBlock(GLuint programId, GLuint shaderStorageBlockIndex, GLuint shaderStorageBlockBinding)
{
    Q_UNUSED(programId);
    Q_UNUSED(shaderStorageBlockIndex);
    Q_UNUSED(shaderStorageBlockBinding);
    qWarning() << "SSBO are not supported by ES 2.0 (since ES 3.1)";
}

void GraphicsHelperES2::bindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
    Q_UNUSED(target);
    Q_UNUSED(index);
    Q_UNUSED(buffer);
    qWarning() << "bindBufferBase is not supported by ES 2.0 (since ES 3.0)";
}

void GraphicsHelperES2::buildUniformBuffer(const QVariant &v, const ShaderUniform &description, QByteArray &buffer)
{
    Q_UNUSED(v);
    Q_UNUSED(description);
    Q_UNUSED(buffer);
    qWarning() << "UBO are not supported by ES 2.0 (since ES 3.0)";
}

uint GraphicsHelperES2::uniformByteSize(const ShaderUniform &description)
{
    uint rawByteSize = 0;
    int arrayStride = qMax(description.m_arrayStride, 0);
    int matrixStride = qMax(description.m_matrixStride, 0);

    switch (description.m_type) {

    case GL_FLOAT_VEC2:
    case GL_INT_VEC2:
        rawByteSize = 8;
        break;

    case GL_FLOAT_VEC3:
    case GL_INT_VEC3:
        rawByteSize = 12;
        break;

    case GL_FLOAT_VEC4:
    case GL_INT_VEC4:
        rawByteSize = 16;
        break;

    case GL_FLOAT_MAT2:
        rawByteSize = matrixStride ? 2 * matrixStride : 16;
        break;

    case GL_FLOAT_MAT3:
        rawByteSize = matrixStride ? 3 * matrixStride : 36;
        break;

    case GL_FLOAT_MAT4:
        rawByteSize = matrixStride ? 4 * matrixStride : 64;
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
    case GL_SAMPLER_2D:
    case GL_SAMPLER_CUBE:
        rawByteSize = 4;
        break;
    }

    return arrayStride ? rawByteSize * arrayStride : rawByteSize;
}

void GraphicsHelperES2::enableClipPlane(int)
{
}

void GraphicsHelperES2::disableClipPlane(int)
{
}

void GraphicsHelperES2::setClipPlane(int, const QVector3D &, float)
{
    qWarning() << "Clip planes not supported by OpenGL ES 2.0";
}

GLint GraphicsHelperES2::maxClipPlaneCount()
{
    return 0;
}

void GraphicsHelperES2::enablePrimitiveRestart(int)
{
}

void GraphicsHelperES2::disablePrimitiveRestart()
{
}

void GraphicsHelperES2::clearBufferf(GLint drawbuffer, const QVector4D &values)
{
    Q_UNUSED(drawbuffer);
    Q_UNUSED(values);
    qWarning() << "glClearBuffer*() not supported by OpenGL ES 2.0";
}

void GraphicsHelperES2::pointSize(bool programmable, GLfloat value)
{
    // If this is not a reset to default values, print a warning
    if (programmable || !qFuzzyCompare(value, 1.0f)) {
        static bool warned = false;
        if (!warned) {
            qWarning() << "glPointSize() and GL_PROGRAM_POINT_SIZE are not supported by ES 2.0";
            warned = true;
        }
    }
}

void GraphicsHelperES2::enablei(GLenum cap, GLuint index)
{
    Q_UNUSED(cap);
    Q_UNUSED(index);
    qWarning() << "glEnablei() not supported by OpenGL ES 2.0";
}

void GraphicsHelperES2::disablei(GLenum cap, GLuint index)
{
    Q_UNUSED(cap);
    Q_UNUSED(index);
    qWarning() << "glDisablei() not supported by OpenGL ES 2.0";
}

void GraphicsHelperES2::setSeamlessCubemap(bool enable)
{
    Q_UNUSED(enable);
    qWarning() << "GL_TEXTURE_CUBE_MAP_SEAMLESS not supported by OpenGL ES 2.0";
}

QSize GraphicsHelperES2::getRenderBufferDimensions(GLuint renderBufferId)
{
    GLint width = 0;
    GLint height = 0;

    m_funcs->glBindRenderbuffer(GL_RENDERBUFFER, renderBufferId);
    m_funcs->glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
    m_funcs->glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
    m_funcs->glBindRenderbuffer(GL_RENDERBUFFER, 0);

    return QSize(width, height);
}

QSize GraphicsHelperES2::getTextureDimensions(GLuint textureId, GLenum target, uint level)
{
    Q_UNUSED(textureId);
    Q_UNUSED(target);
    Q_UNUSED(level);
    qCritical() << "getTextureDimensions is not supported by ES 2.0";
    return QSize(0, 0);
}

void GraphicsHelperES2::dispatchCompute(GLuint wx, GLuint wy, GLuint wz)
{
    Q_UNUSED(wx);
    Q_UNUSED(wy);
    Q_UNUSED(wz);
    qWarning() << "Compute Shaders are not supported by ES 2.0 (since ES 3.1)";
}

void GraphicsHelperES2::glUniform1fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniform1fv(location, count, values);
}

void GraphicsHelperES2::glUniform2fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniform2fv(location, count, values);
}

void GraphicsHelperES2::glUniform3fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniform3fv(location, count, values);
}

void GraphicsHelperES2::glUniform4fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniform4fv(location, count, values);
}

void GraphicsHelperES2::glUniform1iv(GLint location, GLsizei count, const GLint *values)
{
    m_funcs->glUniform1iv(location, count, values);
}

void GraphicsHelperES2::glUniform2iv(GLint location, GLsizei count, const GLint *values)
{
    m_funcs->glUniform2iv(location, count, values);
}

void GraphicsHelperES2::glUniform3iv(GLint location, GLsizei count, const GLint *values)
{
    m_funcs->glUniform3iv(location, count, values);
}

void GraphicsHelperES2::glUniform4iv(GLint location, GLsizei count, const GLint *values)
{
    m_funcs->glUniform4iv(location, count, values);
}

void GraphicsHelperES2::glUniform1uiv(GLint , GLsizei , const GLuint *)
{
    qWarning() << "glUniform1uiv not supported by ES 2";
}

void GraphicsHelperES2::glUniform2uiv(GLint , GLsizei , const GLuint *)
{
    qWarning() << "glUniform2uiv not supported by ES 2";
}

void GraphicsHelperES2::glUniform3uiv(GLint , GLsizei , const GLuint *)
{
    qWarning() << "glUniform3uiv not supported by ES 2";
}

void GraphicsHelperES2::glUniform4uiv(GLint , GLsizei , const GLuint *)
{
    qWarning() << "glUniform4uiv not supported by ES 2";
}

void GraphicsHelperES2::glUniformMatrix2fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniformMatrix2fv(location, count, false, values);
}

void GraphicsHelperES2::glUniformMatrix3fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniformMatrix3fv(location, count, false, values);
}

void GraphicsHelperES2::glUniformMatrix4fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniformMatrix4fv(location, count, false, values);
}

void GraphicsHelperES2::glUniformMatrix2x3fv(GLint , GLsizei , const GLfloat *)
{
    qWarning() << "glUniformMatrix2x3fv not supported by ES 2";
}

void GraphicsHelperES2::glUniformMatrix3x2fv(GLint , GLsizei , const GLfloat *)
{
    qWarning() << "glUniformMatrix3x2fv not supported by ES 2";
}

void GraphicsHelperES2::glUniformMatrix2x4fv(GLint , GLsizei , const GLfloat *)
{
    qWarning() << "glUniformMatrix2x4fv not supported by ES 2";
}

void GraphicsHelperES2::glUniformMatrix4x2fv(GLint , GLsizei , const GLfloat *)
{
    qWarning() << "glUniformMatrix4x2fv not supported by ES 2";
}

void GraphicsHelperES2::glUniformMatrix3x4fv(GLint , GLsizei , const GLfloat *)
{
    qWarning() << "glUniformMatrix3x4fv not supported by ES 2";
}

void GraphicsHelperES2::glUniformMatrix4x3fv(GLint , GLsizei , const GLfloat *)
{
    qWarning() << "glUniformMatrix4x3fv not supported by ES 2";
}

UniformType GraphicsHelperES2::uniformTypeFromGLType(GLenum type)
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
    case GL_INT:
        return UniformType::Int;
    case GL_INT_VEC2:
        return UniformType::IVec2;
    case GL_INT_VEC3:
        return UniformType::IVec3;
    case GL_INT_VEC4:
        return UniformType::IVec4;
    case GL_BOOL:
        return UniformType::Bool;
    case GL_BOOL_VEC2:
        return UniformType::BVec2;
    case GL_BOOL_VEC3:
        return UniformType::BVec3;
    case GL_BOOL_VEC4:
        return UniformType::BVec4;

    case GL_SAMPLER_2D:
    case GL_SAMPLER_CUBE:
        return UniformType::Sampler;
    default:
        Q_UNREACHABLE();
        return UniformType::Float;
    }
}

void GraphicsHelperES2::blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    Q_UNUSED(srcX0);
    Q_UNUSED(srcX1);
    Q_UNUSED(srcY0);
    Q_UNUSED(srcY1);
    Q_UNUSED(dstX0);
    Q_UNUSED(dstX1);
    Q_UNUSED(dstY0);
    Q_UNUSED(dstY1);
    Q_UNUSED(mask);
    Q_UNUSED(filter);
    qWarning() << "Framebuffer blits are not supported by ES 2.0 (since ES 3.1)";
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
