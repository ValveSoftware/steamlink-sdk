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

#ifndef QT3DRENDER_RENDER_GRAPHICSHELPERINTERFACE_H
#define QT3DRENDER_RENDER_GRAPHICSHELPERINTERFACE_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QVector>
#include <Qt3DRender/private/shadervariables_p.h>
#include <Qt3DRender/private/uniform_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

struct Attachment;

class GraphicsHelperInterface
{
public:
    enum Feature {
        MRT = 0,
        Tessellation,
        UniformBufferObject,
        BindableFragmentOutputs,
        PrimitiveRestart,
        RenderBufferDimensionRetrieval,
        TextureDimensionRetrieval,
        ShaderStorageObject,
        Compute,
        DrawBuffersBlend,
        BlitFramebuffer
    };

    virtual ~GraphicsHelperInterface() {}
    virtual void    alphaTest(GLenum mode1, GLenum mode2) = 0;
    virtual void    bindBufferBase(GLenum target, GLuint index, GLuint buffer) = 0;
    virtual void    bindFragDataLocation(GLuint shader, const QHash<QString, int> &outputs) = 0;
    virtual void    bindFrameBufferAttachment(QOpenGLTexture *texture, const Attachment &attachment) = 0;
    virtual void    bindFrameBufferObject(GLuint frameBufferId) = 0;
    virtual void    bindShaderStorageBlock(GLuint programId, GLuint shaderStorageBlockIndex, GLuint shaderStorageBlockBinding) = 0;
    virtual void    bindUniformBlock(GLuint programId, GLuint uniformBlockIndex, GLuint uniformBlockBinding) = 0;
    virtual void    blendEquation(GLenum mode) = 0;
    virtual void    blendFunci(GLuint buf, GLenum sfactor, GLenum dfactor) = 0;
    virtual void    blendFuncSeparatei(GLuint buf, GLenum sRGB, GLenum dRGB, GLenum sAlpha, GLenum dAlpha) = 0;
    virtual void    blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) = 0;
    virtual GLuint  boundFrameBufferObject() = 0;
    virtual void    buildUniformBuffer(const QVariant &v, const ShaderUniform &description, QByteArray &buffer) = 0;
    virtual bool    checkFrameBufferComplete() = 0;
    virtual void    clearBufferf(GLint drawbuffer, const QVector4D &values) = 0;
    virtual GLuint  createFrameBufferObject() = 0;
    virtual void    depthMask(GLenum mode) = 0;
    virtual void    depthTest(GLenum mode) = 0;
    virtual void    disableClipPlane(int clipPlane) = 0;
    virtual void    disablei(GLenum cap, GLuint index) = 0;
    virtual void    disablePrimitiveRestart() = 0;
    virtual void    dispatchCompute(GLuint wx, GLuint wy, GLuint wz) = 0;
    virtual void    drawArrays(GLenum primitiveType, GLint first, GLsizei count) = 0;
    virtual void    drawArraysInstanced(GLenum primitiveType, GLint first, GLsizei count, GLsizei instances) = 0;
    virtual void    drawArraysInstancedBaseInstance(GLenum primitiveType, GLint first, GLsizei count, GLsizei instances, GLsizei baseinstance) = 0;
    virtual void    drawBuffers(GLsizei n, const int *bufs) = 0;
    virtual void    drawElements(GLenum primitiveType, GLsizei primitiveCount, GLint indexType, void * indices, GLint baseVertex) = 0;
    virtual void    drawElementsInstancedBaseVertexBaseInstance(GLenum primitiveType, GLsizei primitiveCount, GLint indexType, void * indices, GLsizei instances, GLint baseVertex, GLint baseInstance) = 0;
    virtual void    enableClipPlane(int clipPlane) = 0;
    virtual void    enablei(GLenum cap, GLuint index) = 0;
    virtual void    enablePrimitiveRestart(int primitiveRestartIndex) = 0;
    virtual void    frontFace(GLenum mode) = 0;
    virtual QSize   getRenderBufferDimensions(GLuint renderBufferId) = 0;
    virtual QSize   getTextureDimensions(GLuint textureId, GLenum target, uint level = 0) = 0;
    virtual void    initializeHelper(QOpenGLContext *context, QAbstractOpenGLFunctions *functions) = 0;
    virtual GLint   maxClipPlaneCount() = 0;
    virtual void    pointSize(bool programmable, GLfloat value) = 0;
    virtual QVector<ShaderAttribute> programAttributesAndLocations(GLuint programId) = 0;
    virtual QVector<ShaderUniform> programUniformsAndLocations(GLuint programId) = 0;
    virtual QVector<ShaderUniformBlock> programUniformBlocks(GLuint programId) = 0;
    virtual QVector<ShaderStorageBlock> programShaderStorageBlocks(GLuint programId) = 0;
    virtual void    releaseFrameBufferObject(GLuint frameBufferId) = 0;
    virtual void    setAlphaCoverageEnabled(bool enable) = 0;
    virtual void    setClipPlane(int clipPlane, const QVector3D &normal, float distance) = 0;
    virtual void    setMSAAEnabled(bool enable) = 0;
    virtual void    setSeamlessCubemap(bool enable) = 0;
    virtual void    setVerticesPerPatch(GLint verticesPerPatch) = 0;
    virtual bool    supportsFeature(Feature feature) const = 0;
    virtual uint    uniformByteSize(const ShaderUniform &description) = 0;
    virtual void    useProgram(GLuint programId) = 0;
    virtual void    vertexAttribDivisor(GLuint index, GLuint divisor) = 0;

    virtual void glUniform1fv(GLint location, GLsizei count, const GLfloat *value) = 0;
    virtual void glUniform2fv(GLint location, GLsizei count, const GLfloat *value) = 0;
    virtual void glUniform3fv(GLint location, GLsizei count, const GLfloat *value) = 0;
    virtual void glUniform4fv(GLint location, GLsizei count, const GLfloat *value) = 0;

    virtual void glUniform1iv(GLint location, GLsizei count, const GLint *value) = 0;
    virtual void glUniform2iv(GLint location, GLsizei count, const GLint *value) = 0;
    virtual void glUniform3iv(GLint location, GLsizei count, const GLint *value) = 0;
    virtual void glUniform4iv(GLint location, GLsizei count, const GLint *value) = 0;

    virtual void glUniform1uiv(GLint location, GLsizei count, const GLuint *value) = 0;
    virtual void glUniform2uiv(GLint location, GLsizei count, const GLuint *value) = 0;
    virtual void glUniform3uiv(GLint location, GLsizei count, const GLuint *value) = 0;
    virtual void glUniform4uiv(GLint location, GLsizei count, const GLuint *value) = 0;

    virtual void glUniformMatrix2fv(GLint location, GLsizei count, const GLfloat *value) = 0;
    virtual void glUniformMatrix3fv(GLint location, GLsizei count, const GLfloat *value) = 0;
    virtual void glUniformMatrix4fv(GLint location, GLsizei count, const GLfloat *value) = 0;
    virtual void glUniformMatrix2x3fv(GLint location, GLsizei count, const GLfloat *value) = 0;
    virtual void glUniformMatrix3x2fv(GLint location, GLsizei count, const GLfloat *value) = 0;
    virtual void glUniformMatrix2x4fv(GLint location, GLsizei count, const GLfloat *value) = 0;
    virtual void glUniformMatrix4x2fv(GLint location, GLsizei count, const GLfloat *value) = 0;
    virtual void glUniformMatrix3x4fv(GLint location, GLsizei count, const GLfloat *value) = 0;
    virtual void glUniformMatrix4x3fv(GLint location, GLsizei count, const GLfloat *value) = 0;

    virtual UniformType uniformTypeFromGLType(GLenum glType) = 0;
};


} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // QT3DRENDER_RENDER_GRAPHICSHELPERINTERFACE_H
