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

#include "graphicshelpergl2_p.h"
#ifndef QT_OPENGL_ES_2
#include <QOpenGLFunctions_2_0>
#include <private/attachmentpack_p.h>
#include <QtOpenGLExtensions/QOpenGLExtensions>
#include <private/qgraphicsutils_p.h>
#include <QDebug>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

GraphicsHelperGL2::GraphicsHelperGL2()
    : m_funcs(nullptr)
    , m_fboFuncs(nullptr)
{

}

void GraphicsHelperGL2::initializeHelper(QOpenGLContext *context,
                                         QAbstractOpenGLFunctions *functions)
{
    Q_UNUSED(context);
    m_funcs = static_cast<QOpenGLFunctions_2_0*>(functions);
    const bool ok = m_funcs->initializeOpenGLFunctions();
    Q_ASSERT(ok);
    Q_UNUSED(ok);
    if (context->hasExtension(QByteArrayLiteral("GL_ARB_framebuffer_object"))) {
        m_fboFuncs = new QOpenGLExtension_ARB_framebuffer_object();
        const bool extensionOk = m_fboFuncs->initializeOpenGLFunctions();
        Q_ASSERT(extensionOk);
        Q_UNUSED(extensionOk);
    }
}

void GraphicsHelperGL2::drawElementsInstancedBaseVertexBaseInstance(GLenum primitiveType,
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

void GraphicsHelperGL2::drawArraysInstanced(GLenum primitiveType,
                                            GLint first,
                                            GLsizei count,
                                            GLsizei instances)
{
    for (GLint i = 0; i < instances; i++)
        drawArrays(primitiveType,
                   first,
                   count);
}

void GraphicsHelperGL2::drawArraysInstancedBaseInstance(GLenum primitiveType, GLint first, GLsizei count, GLsizei instances, GLsizei baseInstance)
{
    if (baseInstance != 0)
        qWarning() << "glDrawArraysInstancedBaseInstance is not supported with OpenGL 2";
    for (GLint i = 0; i < instances; i++)
        drawArrays(primitiveType,
                   first,
                   count);
}

void GraphicsHelperGL2::drawElements(GLenum primitiveType,
                                     GLsizei primitiveCount,
                                     GLint indexType,
                                     void *indices,
                                     GLint baseVertex)
{
    if (baseVertex != 0)
        qWarning() << "glDrawElementsBaseVertex is not supported with OpenGL 2";

    m_funcs->glDrawElements(primitiveType,
                            primitiveCount,
                            indexType,
                            indices);
}

void GraphicsHelperGL2::drawArrays(GLenum primitiveType,
                                   GLint first,
                                   GLsizei count)
{
    m_funcs->glDrawArrays(primitiveType,
                          first,
                          count);
}

void GraphicsHelperGL2::setVerticesPerPatch(GLint verticesPerPatch)
{
    Q_UNUSED(verticesPerPatch);
    qWarning() << "Tessellation not supported with OpenGL 2";
}

void GraphicsHelperGL2::useProgram(GLuint programId)
{
    m_funcs->glUseProgram(programId);
}

QVector<ShaderUniform> GraphicsHelperGL2::programUniformsAndLocations(GLuint programId)
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

QVector<ShaderAttribute> GraphicsHelperGL2::programAttributesAndLocations(GLuint programId)
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

QVector<ShaderUniformBlock> GraphicsHelperGL2::programUniformBlocks(GLuint programId)
{
    Q_UNUSED(programId);
    QVector<ShaderUniformBlock> blocks;
    qWarning() << "UBO are not supported by OpenGL 2.0 (since OpenGL 3.1)";
    return blocks;
}

QVector<ShaderStorageBlock> GraphicsHelperGL2::programShaderStorageBlocks(GLuint programId)
{
    Q_UNUSED(programId);
    qWarning() << "SSBO are not supported by OpenGL 2.0 (since OpenGL 4.3)";
    return QVector<ShaderStorageBlock>();
}

void GraphicsHelperGL2::vertexAttribDivisor(GLuint index,
                                            GLuint divisor)
{
    Q_UNUSED(index);
    Q_UNUSED(divisor);
}

void GraphicsHelperGL2::blendEquation(GLenum mode)
{
    m_funcs->glBlendEquation(mode);
}

void GraphicsHelperGL2::blendFunci(GLuint buf, GLenum sfactor, GLenum dfactor)
{
    Q_UNUSED(buf);
    Q_UNUSED(sfactor);
    Q_UNUSED(dfactor);

    qWarning() << "glBlendFunci() not supported by OpenGL 2.0 (since OpenGL 4.0)";
}

void GraphicsHelperGL2::blendFuncSeparatei(GLuint buf, GLenum sRGB, GLenum dRGB, GLenum sAlpha, GLenum dAlpha)
{
    Q_UNUSED(buf);
    Q_UNUSED(sRGB);
    Q_UNUSED(dRGB);
    Q_UNUSED(sAlpha);
    Q_UNUSED(dAlpha);

    qWarning() << "glBlendFuncSeparatei() not supported by OpenGL 2.0 (since OpenGL 4.0)";
}

void GraphicsHelperGL2::alphaTest(GLenum mode1, GLenum mode2)
{
    m_funcs->glEnable(GL_ALPHA_TEST);
    m_funcs->glAlphaFunc(mode1, mode2);
}

void GraphicsHelperGL2::depthTest(GLenum mode)
{
    m_funcs->glEnable(GL_DEPTH_TEST);
    m_funcs->glDepthFunc(mode);
}

void GraphicsHelperGL2::depthMask(GLenum mode)
{
    m_funcs->glDepthMask(mode);
}

void GraphicsHelperGL2::frontFace(GLenum mode)
{
    m_funcs->glFrontFace(mode);
}

void GraphicsHelperGL2::setMSAAEnabled(bool enabled)
{
    enabled ? m_funcs->glEnable(GL_MULTISAMPLE)
            : m_funcs->glDisable(GL_MULTISAMPLE);
}

void GraphicsHelperGL2::setAlphaCoverageEnabled(bool enabled)
{
    enabled ? m_funcs->glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE)
            : m_funcs->glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
}

GLuint GraphicsHelperGL2::createFrameBufferObject()
{
    if (m_fboFuncs != nullptr) {
        GLuint id;
        m_fboFuncs->glGenFramebuffers(1, &id);
        return id;
    }
    qWarning() << "FBO not supported by your OpenGL hardware";
    return 0;
}

void GraphicsHelperGL2::releaseFrameBufferObject(GLuint frameBufferId)
{
    if (m_fboFuncs != nullptr)
        m_fboFuncs->glDeleteFramebuffers(1, &frameBufferId);
    else
        qWarning() << "FBO not supported by your OpenGL hardware";
}

bool GraphicsHelperGL2::checkFrameBufferComplete()
{
    if (m_fboFuncs != nullptr)
        return (m_fboFuncs->glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    return false;
}

void GraphicsHelperGL2::bindFrameBufferAttachment(QOpenGLTexture *texture, const Attachment &attachment)
{
    if (m_fboFuncs != nullptr) {
        GLenum attr = GL_DEPTH_STENCIL_ATTACHMENT;

        if (attachment.m_point <= QRenderTargetOutput::Color15)
            attr = GL_COLOR_ATTACHMENT0 + attachment.m_point;
        else if (attachment.m_point == QRenderTargetOutput::Depth)
            attr = GL_DEPTH_ATTACHMENT;
        else if (attachment.m_point == QRenderTargetOutput::Stencil)
            attr = GL_STENCIL_ATTACHMENT;
        else
            qCritical() << "DepthStencil Attachment not supported on OpenGL 2.0";

        const QOpenGLTexture::Target target = texture->target();

        if (target == QOpenGLTexture::TargetCubeMap && attachment.m_face == QAbstractTexture::AllFaces) {
            qWarning() << "OpenGL 2.0 doesn't handle attaching all the faces of a cube map texture at once to an FBO";
            return;
        }

        texture->bind();
        if (target == QOpenGLTexture::Target3D)
            m_fboFuncs->glFramebufferTexture3D(GL_DRAW_FRAMEBUFFER, attr, target, texture->textureId(), attachment.m_mipLevel, attachment.m_layer);
        else if (target == QOpenGLTexture::TargetCubeMap)
            m_fboFuncs->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attr, attachment.m_face, texture->textureId(), attachment.m_mipLevel);
        else if (target == QOpenGLTexture::Target1D)
            m_fboFuncs->glFramebufferTexture1D(GL_DRAW_FRAMEBUFFER, attr, target, texture->textureId(), attachment.m_mipLevel);
        else if (target == QOpenGLTexture::Target2D || target == QOpenGLTexture::TargetRectangle)
            m_fboFuncs->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attr, target, texture->textureId(), attachment.m_mipLevel);
        else
            qCritical() << "Texture format not supported for Attachment on OpenGL 2.0";
        texture->release();
    }
}

bool GraphicsHelperGL2::supportsFeature(GraphicsHelperInterface::Feature feature) const
{
    switch (feature) {
    case MRT:
        return (m_fboFuncs != nullptr);
    case TextureDimensionRetrieval:
        return true;
    default:
        return false;
    }
}

void GraphicsHelperGL2::drawBuffers(GLsizei n, const int *bufs)
{
    QVarLengthArray<GLenum, 16> drawBufs(n);

    for (int i = 0; i < n; i++)
        drawBufs[i] = GL_COLOR_ATTACHMENT0 + bufs[i];
    m_funcs->glDrawBuffers(n, drawBufs.constData());
}

void GraphicsHelperGL2::bindFragDataLocation(GLuint, const QHash<QString, int> &)
{
    qCritical() << "bindFragDataLocation is not supported by GL 2.0";
}

void GraphicsHelperGL2::bindFrameBufferObject(GLuint frameBufferId)
{
    if (m_fboFuncs != nullptr)
        m_fboFuncs->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBufferId);
    else
        qWarning() << "FBO not supported by your OpenGL hardware";
}

GLuint GraphicsHelperGL2::boundFrameBufferObject()
{
    GLint id = 0;
    m_funcs->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &id);
    return id;
}

void GraphicsHelperGL2::bindUniformBlock(GLuint programId, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    Q_UNUSED(programId);
    Q_UNUSED(uniformBlockIndex);
    Q_UNUSED(uniformBlockBinding);
    qWarning() << "UBO are not supported by OpenGL 2.0 (since OpenGL 3.1)";
}

void GraphicsHelperGL2::bindShaderStorageBlock(GLuint programId, GLuint shaderStorageBlockIndex, GLuint shaderStorageBlockBinding)
{
    Q_UNUSED(programId);
    Q_UNUSED(shaderStorageBlockIndex);
    Q_UNUSED(shaderStorageBlockBinding);
    qWarning() << "SSBO are not supported by OpenGL 2.0 (since OpenGL 4.3)";
}

void GraphicsHelperGL2::bindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
    Q_UNUSED(target);
    Q_UNUSED(index);
    Q_UNUSED(buffer);
    qWarning() << "bindBufferBase is not supported by OpenGL 2.0 (since OpenGL 3.0)";
}

void GraphicsHelperGL2::buildUniformBuffer(const QVariant &v, const ShaderUniform &description, QByteArray &buffer)
{
    Q_UNUSED(v);
    Q_UNUSED(description);
    Q_UNUSED(buffer);
    qWarning() << "UBO are not supported by OpenGL 2.0 (since OpenGL 3.1)";
}

uint GraphicsHelperGL2::uniformByteSize(const ShaderUniform &description)
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
    case GL_SAMPLER_1D:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
        rawByteSize = 4;
        break;

    default:
        Q_UNREACHABLE();
    }

    return arrayStride ? rawByteSize * arrayStride : rawByteSize;
}

void GraphicsHelperGL2::enableClipPlane(int clipPlane)
{
    m_funcs->glEnable(GL_CLIP_DISTANCE0 + clipPlane);
}

void GraphicsHelperGL2::disableClipPlane(int clipPlane)
{
    m_funcs->glDisable(GL_CLIP_DISTANCE0 + clipPlane);
}

void GraphicsHelperGL2::setClipPlane(int clipPlane, const QVector3D &normal, float distance)
{
    double plane[4];
    plane[0] = normal.x();
    plane[1] = normal.y();
    plane[2] = normal.z();
    plane[3] = distance;

    m_funcs->glClipPlane(GL_CLIP_PLANE0 + clipPlane, plane);
}

GLint GraphicsHelperGL2::maxClipPlaneCount()
{
    GLint max = 0;
    m_funcs->glGetIntegerv(GL_MAX_CLIP_DISTANCES, &max);
    return max;
}

void GraphicsHelperGL2::enablePrimitiveRestart(int)
{
}

void GraphicsHelperGL2::disablePrimitiveRestart()
{
}

void GraphicsHelperGL2::clearBufferf(GLint drawbuffer, const QVector4D &values)
{
    Q_UNUSED(drawbuffer);
    Q_UNUSED(values);
    qWarning() << "glClearBuffer*() not supported by OpenGL 2.0";
}

void GraphicsHelperGL2::pointSize(bool programmable, GLfloat value)
{
    // Print a warning once for trying to set GL_PROGRAM_POINT_SIZE
    if (programmable) {
        static bool warned = false;
        if (!warned) {
            qWarning() << "GL_PROGRAM_POINT_SIZE is not supported by OpenGL 2.0 (since 3.2)";
            warned = true;
        }
    }
    m_funcs->glPointSize(value);
}

void GraphicsHelperGL2::enablei(GLenum cap, GLuint index)
{
    Q_UNUSED(cap);
    Q_UNUSED(index);
    qWarning() << "glEnablei() not supported by OpenGL 2.0 (since 3.0)";
}

void GraphicsHelperGL2::disablei(GLenum cap, GLuint index)
{
    Q_UNUSED(cap);
    Q_UNUSED(index);
    qWarning() << "glDisablei() not supported by OpenGL 2.0 (since 3.0)";
}

void GraphicsHelperGL2::setSeamlessCubemap(bool enable)
{
    Q_UNUSED(enable);
    qWarning() << "GL_TEXTURE_CUBE_MAP_SEAMLESS not supported by OpenGL 2.0 (since 3.2)";
}

QSize GraphicsHelperGL2::getRenderBufferDimensions(GLuint renderBufferId)
{
    Q_UNUSED(renderBufferId);
    qCritical() << "RenderBuffer dimensions retrival not supported on OpenGL 2.0";
    return QSize(0,0);
}

QSize GraphicsHelperGL2::getTextureDimensions(GLuint textureId, GLenum target, uint level)
{
    GLint width = 0;
    GLint height = 0;

    m_funcs->glBindTexture(target, textureId);
    m_funcs->glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &width);
    m_funcs->glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &height);
    m_funcs->glBindTexture(target, 0);

    return QSize(width, height);
}

void GraphicsHelperGL2::dispatchCompute(GLuint wx, GLuint wy, GLuint wz)
{
    Q_UNUSED(wx);
    Q_UNUSED(wy);
    Q_UNUSED(wz);
    qWarning() << "Compute Shaders are not supported by OpenGL 2.0 (since OpenGL 4.3)";
}

void GraphicsHelperGL2::glUniform1fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniform1fv(location, count, values);
}

void GraphicsHelperGL2::glUniform2fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniform2fv(location, count, values);
}

void GraphicsHelperGL2::glUniform3fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniform3fv(location, count, values);
}

void GraphicsHelperGL2::glUniform4fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniform4fv(location, count, values);
}

void GraphicsHelperGL2::glUniform1iv(GLint location, GLsizei count, const GLint *values)
{
    m_funcs->glUniform1iv(location, count, values);
}

void GraphicsHelperGL2::glUniform2iv(GLint location, GLsizei count, const GLint *values)
{
    m_funcs->glUniform2iv(location, count, values);
}

void GraphicsHelperGL2::glUniform3iv(GLint location, GLsizei count, const GLint *values)
{
    m_funcs->glUniform3iv(location, count, values);
}

void GraphicsHelperGL2::glUniform4iv(GLint location, GLsizei count, const GLint *values)
{
    m_funcs->glUniform4iv(location, count, values);
}

void GraphicsHelperGL2::glUniform1uiv(GLint , GLsizei , const GLuint *)
{
    qWarning() << "glUniform1uiv not supported by GL 2";
}

void GraphicsHelperGL2::glUniform2uiv(GLint , GLsizei , const GLuint *)
{
    qWarning() << "glUniform2uiv not supported by GL 2";
}

void GraphicsHelperGL2::glUniform3uiv(GLint , GLsizei , const GLuint *)
{
    qWarning() << "glUniform3uiv not supported by GL 2";
}

void GraphicsHelperGL2::glUniform4uiv(GLint , GLsizei , const GLuint *)
{
    qWarning() << "glUniform4uiv not supported by GL 2";
}

void GraphicsHelperGL2::glUniformMatrix2fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniformMatrix2fv(location, count, false, values);
}

void GraphicsHelperGL2::glUniformMatrix3fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniformMatrix3fv(location, count, false, values);
}

void GraphicsHelperGL2::glUniformMatrix4fv(GLint location, GLsizei count, const GLfloat *values)
{
    m_funcs->glUniformMatrix4fv(location, count, false, values);
}

void GraphicsHelperGL2::glUniformMatrix2x3fv(GLint , GLsizei , const GLfloat *)
{
    qWarning() << "glUniformMatrix2x3fv not supported by GL 2";
}

void GraphicsHelperGL2::glUniformMatrix3x2fv(GLint , GLsizei , const GLfloat *)
{
    qWarning() << "glUniformMatrix3x2fv not supported by GL 2";
}

void GraphicsHelperGL2::glUniformMatrix2x4fv(GLint , GLsizei , const GLfloat *)
{
    qWarning() << "glUniformMatrix2x4fv not supported by GL 2";
}

void GraphicsHelperGL2::glUniformMatrix4x2fv(GLint , GLsizei , const GLfloat *)
{
    qWarning() << "glUniformMatrix4x2fv not supported by GL 2";
}

void GraphicsHelperGL2::glUniformMatrix3x4fv(GLint , GLsizei , const GLfloat *)
{
    qWarning() << "glUniformMatrix3x4fv not supported by GL 2";
}

void GraphicsHelperGL2::glUniformMatrix4x3fv(GLint , GLsizei , const GLfloat *)
{
    qWarning() << "glUniformMatrix4x3fv not supported by GL 2";
}

UniformType GraphicsHelperGL2::uniformTypeFromGLType(GLenum type)
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

    case GL_SAMPLER_1D:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_3D:
        return UniformType::Sampler;

    default:
        Q_UNREACHABLE();
        return UniformType::Float;
    }
}

void GraphicsHelperGL2::blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
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

#endif // !QT_OPENGL_ES_2
