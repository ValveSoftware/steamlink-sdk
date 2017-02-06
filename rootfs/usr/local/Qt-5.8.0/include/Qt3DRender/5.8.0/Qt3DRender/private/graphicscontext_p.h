/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#ifndef QT3DRENDER_RENDER_GRAPHICSCONTEXT_H
#define QT3DRENDER_RENDER_GRAPHICSCONTEXT_H

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

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QHash>
#include <QColor>
#include <QMatrix4x4>
#include <QBitArray>
#include <QImage>
#include <Qt3DRender/private/shaderparameterpack_p.h>
#include <Qt3DRender/qclearbuffers.h>
#include <Qt3DRender/private/shader_p.h>
#include <Qt3DRender/private/glbuffer_p.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DRender/private/handle_types_p.h>
#include <Qt3DRender/private/qgraphicsapifilter_p.h>
#include <Qt3DRender/private/shadercache_p.h>
#include <Qt3DRender/private/uniform_p.h>

QT_BEGIN_NAMESPACE

class QOpenGLDebugLogger;
class QOpenGLShaderProgram;
class QAbstractOpenGLFunctions;

namespace Qt3DRender {

namespace Render {

class Renderer;
class GraphicsHelperInterface;
class RenderStateSet;
class Material;
class GLTexture;
class RenderCommand;
class RenderTarget;
class AttachmentPack;
class Attribute;
class Buffer;

enum TextureScope
{
    TextureScopeMaterial = 0,
    TextureScopeTechnique
    // per-pass for deferred rendering?
};

typedef QPair<QString, int> NamedUniformLocation;

class Q_AUTOTEST_EXPORT GraphicsContext
{
public:
    GraphicsContext();
    ~GraphicsContext();

    int id() const; // unique, small integer ID of this context

    bool beginDrawing(QSurface *surface);
    void clearBackBuffer(QClearBuffers::BufferTypeFlags buffers);
    void endDrawing(bool swapBuffers);

    void setViewport(const QRectF &viewport, const QSize &surfaceSize);
    QRectF viewport() const { return m_viewport; }

    /**
     * @brief releaseGL - release all OpenGL objects associated with
     * this context
     */
    void releaseOpenGL();
    void setOpenGLContext(QOpenGLContext* ctx);
    QOpenGLContext *openGLContext() { return m_gl; }
    bool makeCurrent(QSurface *surface);
    void doneCurrent();
    void activateGLHelper();
    bool hasValidGLHelper() const;
    bool isInitialized() const;

    QOpenGLShaderProgram *createShaderProgram(Shader *shaderNode);
    void loadShader(Shader* shader);
    void activateShader(ProgramDNA shaderDNA);
    void removeShaderProgramReference(Shader *shaderNode);

    GLuint activeFBO() const { return m_activeFBO; }
    GLuint defaultFBO() const { return m_defaultFBO; }
    void activateRenderTarget(const Qt3DCore::QNodeId id, const AttachmentPack &attachments, GLuint defaultFboId);

    Material* activeMaterial() const { return m_material; }

    void setActiveMaterial(Material* rmat);

    void executeCommand(const RenderCommand *command);

    QSize renderTargetSize(const QSize &surfaceSize) const;

    /**
     * @brief activeShader
     * @return
     */
    QOpenGLShaderProgram* activeShader() const;

    void setRenderer(Renderer *renderer);

    void specifyAttribute(const Attribute *attribute, Buffer *buffer, int attributeLocation);
    void specifyIndices(Buffer *buffer);
    void updateBuffer(Buffer *buffer);
    void releaseBuffer(Qt3DCore::QNodeId bufferId);
    bool hasGLBufferForBuffer(Buffer *buffer);

    void setParameters(ShaderParameterPack &parameterPack);

    /**
     * @brief glBufferFor - given a client-side (CPU) buffer, provide the
     * context-specific object. Initial call will create the buffer.
     * @param buf
     * @return
     */
    GLBuffer *glBufferForRenderBuffer(Buffer *buf);

    /**
     * @brief activateTexture - make a texture active on a hardware unit
     * @param tex - the texture to activate
     * @param onUnit - option, specify the explicit unit to activate on
     * @return - the unit the texture was activated on
     */
    int activateTexture(TextureScope scope, GLTexture* tex, int onUnit = -1);

    void deactivateTexture(GLTexture *tex);

    void setCurrentStateSet(RenderStateSet* ss);
    RenderStateSet *currentStateSet() const;
    const GraphicsApiFilterData *contextInfo() const;

    // Wrapper methods
    void    alphaTest(GLenum mode1, GLenum mode2);
    void    bindFramebuffer(GLuint fbo);
    void    bindBufferBase(GLenum target, GLuint bindingIndex, GLuint buffer);
    void    bindFragOutputs(GLuint shader, const QHash<QString, int> &outputs);
    void    bindUniformBlock(GLuint programId, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
    void    bindShaderStorageBlock(GLuint programId, GLuint shaderStorageBlockIndex, GLuint shaderStorageBlockBinding);
    void    blendEquation(GLenum mode);
    void    blendFunci(GLuint buf, GLenum sfactor, GLenum dfactor);
    void    blendFuncSeparatei(GLuint buf, GLenum sRGB, GLenum dRGB, GLenum sAlpha, GLenum dAlpha);
    GLuint  boundFrameBufferObject();
    void    buildUniformBuffer(const QVariant &v, const ShaderUniform &description, QByteArray &buffer);
    void    clearBufferf(GLint drawbuffer, const QVector4D &values);
    void    clearColor(const QColor &color);
    void    clearDepthValue(float depth);
    void    clearStencilValue(int stencil);
    void    depthMask(GLenum mode);
    void    depthTest(GLenum mode);
    void    disableClipPlane(int clipPlane);
    void    disablei(GLenum cap, GLuint index);
    void    disablePrimitiveRestart();
    void    dispatchCompute(int x, int y, int z);
    void    drawArrays(GLenum primitiveType, GLint first, GLsizei count);
    void    drawArraysInstanced(GLenum primitiveType, GLint first, GLsizei count, GLsizei instances);
    void    drawArraysInstancedBaseInstance(GLenum primitiveType, GLint first, GLsizei count, GLsizei instances, GLsizei baseinstance);
    void    drawElements(GLenum primitiveType, GLsizei primitiveCount, GLint indexType, void * indices, GLint baseVertex);
    void    drawElementsInstancedBaseVertexBaseInstance(GLenum primitiveType, GLsizei primitiveCount, GLint indexType, void * indices, GLsizei instances, GLint baseVertex, GLint baseInstance);
    void    enableClipPlane(int clipPlane);
    void    enablei(GLenum cap, GLuint index);
    void    enablePrimitiveRestart(int restartIndex);
    void    frontFace(GLenum mode);
    GLint   maxClipPlaneCount();
    void    pointSize(bool programmable, GLfloat value);
    void    setMSAAEnabled(bool enabled);
    void    setAlphaCoverageEnabled(bool enabled);
    void    setClipPlane(int clipPlane, const QVector3D &normal, float distance);
    void    setSeamlessCubemap(bool enable);
    void    setVerticesPerPatch(GLint verticesPerPatch);

    // Helper methods
    static GLint elementType(GLint type);
    static GLint tupleSizeFromType(GLint type);
    static GLuint byteSizeFromType(GLint type);
    static GLint glDataTypeFromAttributeDataType(QAttribute::VertexBaseType dataType);

    bool supportsDrawBuffersBlend() const;
    bool supportsVAO() const { return m_supportsVAO; }

    QImage readFramebuffer(QSize size);

private:
    void initialize();

    void decayTextureScores();

    GLint assignUnitForTexture(GLTexture* tex);
    void deactivateTexturesWithScope(TextureScope ts);

    GraphicsHelperInterface *resolveHighestOpenGLFunctions();

    void bindFrameBufferAttachmentHelper(GLuint fboId, const AttachmentPack &attachments);
    void activateDrawBuffers(const AttachmentPack &attachments);
    HGLBuffer createGLBufferFor(Buffer *buffer);
    void uploadDataToGLBuffer(Buffer *buffer, GLBuffer *b, bool releaseBuffer = false);
    bool bindGLBuffer(GLBuffer *buffer, GLBuffer::Type type);
    void resolveRenderTargetFormat();

    bool m_initialized;
    const unsigned int m_id;
    QOpenGLContext *m_gl;
    QSurface *m_surface;
    GraphicsHelperInterface *m_glHelper;
    bool m_ownCurrent;

    ShaderCache m_shaderCache;
    QOpenGLShaderProgram *m_activeShader;
    ProgramDNA m_activeShaderDNA;

    QHash<Qt3DCore::QNodeId, HGLBuffer> m_renderBufferHash;
    QHash<Qt3DCore::QNodeId, GLuint> m_renderTargets;
    QHash<GLuint, QSize> m_renderTargetsSize;
    QAbstractTexture::TextureFormat m_renderTargetFormat;

    QHash<QSurface *, GraphicsHelperInterface*> m_glHelpers;

    // active textures, indexed by texture unit
    struct ActiveTexture {
        GLTexture *texture = nullptr;
        int score = 0;
        TextureScope scope = TextureScopeMaterial;
        bool pinned = false;
    };
    QVector<ActiveTexture> m_activeTextures;

    // cache some current state, to make sure we don't issue unnecessary GL calls
    int m_currClearStencilValue;
    float m_currClearDepthValue;
    QColor m_currClearColorValue;

    Material* m_material;
    QRectF m_viewport;
    GLuint m_activeFBO;
    GLuint m_defaultFBO;

    GLBuffer *m_boundArrayBuffer;

    RenderStateSet* m_stateSet;

    Renderer *m_renderer;
    GraphicsApiFilterData m_contextInfo;

    QByteArray m_uboTempArray;

    bool m_supportsVAO;
    QScopedPointer<QOpenGLDebugLogger> m_debugLogger;

    friend class OpenGLVertexArrayObject;
    OpenGLVertexArrayObject *m_currentVAO;

    struct VAOVertexAttribute
    {
        HGLBuffer bufferHandle;
        GLBuffer::Type bufferType;
        int location;
        GLint dataType;
        uint byteOffset;
        uint vertexSize;
        uint byteStride;
        uint divisor;
    };

    using VAOIndexAttribute = HGLBuffer;

    void enableAttribute(const VAOVertexAttribute &attr);
    void disableAttribute(const VAOVertexAttribute &attr);

    void applyUniform(const ShaderUniform &description, const UniformValue &v);

    template<UniformType>
    void applyUniformHelper(int, int, const UniformValue &) const
    {
        Q_ASSERT_X(false, Q_FUNC_INFO, "Uniform: Didn't provide specialized apply() implementation");
    }
};

#define QT3D_UNIFORM_TYPE_PROTO(UniformTypeEnum, BaseType, Func) \
template<> \
void GraphicsContext::applyUniformHelper<UniformTypeEnum>(int location, int count, const UniformValue &value) const;

#define QT3D_UNIFORM_TYPE_IMPL(UniformTypeEnum, BaseType, Func) \
    template<> \
    void GraphicsContext::applyUniformHelper<UniformTypeEnum>(int location, int count, const UniformValue &value) const \
{ \
    m_glHelper->Func(location, count, value.constData<BaseType>()); \
}


QT3D_UNIFORM_TYPE_PROTO(UniformType::Float, float, glUniform1fv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::Vec2, float, glUniform2fv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::Vec3, float, glUniform3fv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::Vec4, float, glUniform4fv)

// OpenGL expects int* as values for booleans
QT3D_UNIFORM_TYPE_PROTO(UniformType::Bool, int, glUniform1iv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::BVec2, int, glUniform2iv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::BVec3, int, glUniform3iv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::BVec4, int, glUniform4iv)

QT3D_UNIFORM_TYPE_PROTO(UniformType::Int, int, glUniform1iv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::IVec2, int, glUniform2iv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::IVec3, int, glUniform3iv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::IVec4, int, glUniform4iv)

QT3D_UNIFORM_TYPE_PROTO(UniformType::UInt, uint, glUniform1uiv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::UIVec2, uint, glUniform2uiv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::UIVec3, uint, glUniform3uiv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::UIVec4, uint, glUniform4uiv)

QT3D_UNIFORM_TYPE_PROTO(UniformType::Mat2, float, glUniformMatrix2fv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::Mat3, float, glUniformMatrix3fv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::Mat4, float, glUniformMatrix4fv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::Mat2x3, float, glUniformMatrix2x3fv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::Mat3x2, float, glUniformMatrix3x2fv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::Mat2x4, float, glUniformMatrix2x4fv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::Mat4x2, float, glUniformMatrix4x2fv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::Mat3x4, float, glUniformMatrix3x4fv)
QT3D_UNIFORM_TYPE_PROTO(UniformType::Mat4x3, float, glUniformMatrix4x3fv)

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // QT3DRENDER_RENDER_GRAPHICSCONTEXT_H
