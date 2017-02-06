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

#ifndef QT3DRENDER_RENDER_GRAPHICSHELPERGL3_H
#define QT3DRENDER_RENDER_GRAPHICSHELPERGL3_H

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

#include <Qt3DRender/private/graphicshelperinterface_p.h>
#include <QtCore/qscopedpointer.h>

#ifndef QT_OPENGL_ES_2

QT_BEGIN_NAMESPACE

class QOpenGLFunctions_3_2_Core;
class QOpenGLExtension_ARB_tessellation_shader;

namespace Qt3DRender {
namespace Render {

class Q_AUTOTEST_EXPORT GraphicsHelperGL3_2 : public GraphicsHelperInterface
{
public:
    GraphicsHelperGL3_2();
    ~GraphicsHelperGL3_2();

    // QGraphicHelperInterface interface
    void alphaTest(GLenum mode1, GLenum mode2) Q_DECL_OVERRIDE;
    void bindBufferBase(GLenum target, GLuint index, GLuint buffer) Q_DECL_OVERRIDE;
    void bindFragDataLocation(GLuint shader, const QHash<QString, int> &outputs) Q_DECL_OVERRIDE;
    void bindFrameBufferAttachment(QOpenGLTexture *texture, const Attachment &attachment) Q_DECL_OVERRIDE;
    void bindFrameBufferObject(GLuint frameBufferId) Q_DECL_OVERRIDE;
    void bindShaderStorageBlock(GLuint programId, GLuint shaderStorageBlockIndex, GLuint shaderStorageBlockBinding) Q_DECL_OVERRIDE;
    void bindUniformBlock(GLuint programId, GLuint uniformBlockIndex, GLuint uniformBlockBinding) Q_DECL_OVERRIDE;
    void blendEquation(GLenum mode) Q_DECL_OVERRIDE;
    void blendFunci(GLuint buf, GLenum sfactor, GLenum dfactor) Q_DECL_OVERRIDE;
    void blendFuncSeparatei(GLuint buf, GLenum sRGB, GLenum dRGB, GLenum sAlpha, GLenum dAlpha) Q_DECL_OVERRIDE;
    void blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) Q_DECL_OVERRIDE;
    GLuint boundFrameBufferObject() Q_DECL_OVERRIDE;
    void buildUniformBuffer(const QVariant &v, const ShaderUniform &description, QByteArray &buffer) Q_DECL_OVERRIDE;
    bool checkFrameBufferComplete() Q_DECL_OVERRIDE;
    void clearBufferf(GLint drawbuffer, const QVector4D &values) Q_DECL_OVERRIDE;
    GLuint createFrameBufferObject() Q_DECL_OVERRIDE;
    void depthMask(GLenum mode) Q_DECL_OVERRIDE;
    void depthTest(GLenum mode) Q_DECL_OVERRIDE;
    void disableClipPlane(int clipPlane) Q_DECL_OVERRIDE;
    void disablei(GLenum cap, GLuint index) Q_DECL_OVERRIDE;
    void disablePrimitiveRestart() Q_DECL_OVERRIDE;
    void dispatchCompute(GLuint wx, GLuint wy, GLuint wz) Q_DECL_OVERRIDE;
    void drawArrays(GLenum primitiveType, GLint first, GLsizei count) Q_DECL_OVERRIDE;
    void drawArraysInstanced(GLenum primitiveType, GLint first, GLsizei count, GLsizei instances) Q_DECL_OVERRIDE;
    void drawArraysInstancedBaseInstance(GLenum primitiveType, GLint first, GLsizei count, GLsizei instances, GLsizei baseInstance) Q_DECL_OVERRIDE;
    void drawBuffers(GLsizei n, const int *bufs) Q_DECL_OVERRIDE;
    void drawElements(GLenum primitiveType, GLsizei primitiveCount, GLint indexType, void *indices, GLint baseVertex = 0) Q_DECL_OVERRIDE;
    void drawElementsInstancedBaseVertexBaseInstance(GLenum primitiveType, GLsizei primitiveCount, GLint indexType, void *indices, GLsizei instances, GLint baseVertex = 0,  GLint baseInstance = 0) Q_DECL_OVERRIDE;
    void enableClipPlane(int clipPlane) Q_DECL_OVERRIDE;
    void enablei(GLenum cap, GLuint index) Q_DECL_OVERRIDE;
    void enablePrimitiveRestart(int primitiveRestartIndex) Q_DECL_OVERRIDE;
    void frontFace(GLenum mode) Q_DECL_OVERRIDE;
    QSize getRenderBufferDimensions(GLuint renderBufferId) Q_DECL_OVERRIDE;
    QSize getTextureDimensions(GLuint textureId, GLenum target, uint level = 0) Q_DECL_OVERRIDE;
    void initializeHelper(QOpenGLContext *context, QAbstractOpenGLFunctions *functions) Q_DECL_OVERRIDE;
    void pointSize(bool programmable, GLfloat value) Q_DECL_OVERRIDE;
    GLint maxClipPlaneCount() Q_DECL_OVERRIDE;
    QVector<ShaderUniformBlock> programUniformBlocks(GLuint programId) Q_DECL_OVERRIDE;
    QVector<ShaderAttribute> programAttributesAndLocations(GLuint programId) Q_DECL_OVERRIDE;
    QVector<ShaderUniform> programUniformsAndLocations(GLuint programId) Q_DECL_OVERRIDE;
    QVector<ShaderStorageBlock> programShaderStorageBlocks(GLuint programId) Q_DECL_OVERRIDE;
    void releaseFrameBufferObject(GLuint frameBufferId) Q_DECL_OVERRIDE;
    void setMSAAEnabled(bool enable) Q_DECL_OVERRIDE;
    void setAlphaCoverageEnabled(bool enable) Q_DECL_OVERRIDE;
    void setClipPlane(int clipPlane, const QVector3D &normal, float distance) Q_DECL_OVERRIDE;
    void setSeamlessCubemap(bool enable) Q_DECL_OVERRIDE;
    void setVerticesPerPatch(GLint verticesPerPatch) Q_DECL_OVERRIDE;
    bool supportsFeature(Feature feature) const Q_DECL_OVERRIDE;
    uint uniformByteSize(const ShaderUniform &description) Q_DECL_OVERRIDE;
    void useProgram(GLuint programId) Q_DECL_OVERRIDE;
    void vertexAttribDivisor(GLuint index, GLuint divisor) Q_DECL_OVERRIDE;

    void glUniform1fv(GLint location, GLsizei count, const GLfloat *value) Q_DECL_OVERRIDE;
    void glUniform2fv(GLint location, GLsizei count, const GLfloat *value) Q_DECL_OVERRIDE;
    void glUniform3fv(GLint location, GLsizei count, const GLfloat *value) Q_DECL_OVERRIDE;
    void glUniform4fv(GLint location, GLsizei count, const GLfloat *value) Q_DECL_OVERRIDE;

    void glUniform1iv(GLint location, GLsizei count, const GLint *value) Q_DECL_OVERRIDE;
    void glUniform2iv(GLint location, GLsizei count, const GLint *value) Q_DECL_OVERRIDE;
    void glUniform3iv(GLint location, GLsizei count, const GLint *value) Q_DECL_OVERRIDE;
    void glUniform4iv(GLint location, GLsizei count, const GLint *value) Q_DECL_OVERRIDE;

    void glUniform1uiv(GLint location, GLsizei count, const GLuint *value) Q_DECL_OVERRIDE;
    void glUniform2uiv(GLint location, GLsizei count, const GLuint *value) Q_DECL_OVERRIDE;
    void glUniform3uiv(GLint location, GLsizei count, const GLuint *value) Q_DECL_OVERRIDE;
    void glUniform4uiv(GLint location, GLsizei count, const GLuint *value) Q_DECL_OVERRIDE;

    void glUniformMatrix2fv(GLint location, GLsizei count, const GLfloat *value) Q_DECL_OVERRIDE;
    void glUniformMatrix3fv(GLint location, GLsizei count, const GLfloat *value) Q_DECL_OVERRIDE;
    void glUniformMatrix4fv(GLint location, GLsizei count, const GLfloat *value) Q_DECL_OVERRIDE;
    void glUniformMatrix2x3fv(GLint location, GLsizei count, const GLfloat *value) Q_DECL_OVERRIDE;
    void glUniformMatrix3x2fv(GLint location, GLsizei count, const GLfloat *value) Q_DECL_OVERRIDE;
    void glUniformMatrix2x4fv(GLint location, GLsizei count, const GLfloat *value) Q_DECL_OVERRIDE;
    void glUniformMatrix4x2fv(GLint location, GLsizei count, const GLfloat *value) Q_DECL_OVERRIDE;
    void glUniformMatrix3x4fv(GLint location, GLsizei count, const GLfloat *value) Q_DECL_OVERRIDE;
    void glUniformMatrix4x3fv(GLint location, GLsizei count, const GLfloat *value) Q_DECL_OVERRIDE;

    UniformType uniformTypeFromGLType(GLenum glType) Q_DECL_OVERRIDE;

private:
    QOpenGLFunctions_3_2_Core *m_funcs;
    QScopedPointer<QOpenGLExtension_ARB_tessellation_shader> m_tessFuncs;
};

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // !QT_OPENGL_ES_2

#endif // QT3DRENDER_RENDER_GRAPHICSHELPERGL3_H
