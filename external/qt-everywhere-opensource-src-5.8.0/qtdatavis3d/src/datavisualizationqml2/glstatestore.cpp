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

#include "glstatestore_p.h"
#include <QDebug>
#include <QColor>
#include <QFile>

#ifdef VERBOSE_STATE_STORE
static QFile *beforeFile = 0;
static QFile *afterFile = 0;
#endif

GLStateStore::GLStateStore(QOpenGLContext *context, QObject *parent) :
    QObject(parent),
    QOpenGLFunctions(context)
  #ifdef VERBOSE_STATE_STORE
  , m_map(EnumToStringMap::newInstance())
  #endif
{
    GLint maxVertexAttribs;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);

#ifdef VERBOSE_STATE_STORE
    qDebug() << "GL_MAX_VERTEX_ATTRIBS: " << maxVertexAttribs;
    if (!beforeFile) {
        beforeFile = new QFile(QStringLiteral("state_before.txt"));
        afterFile = new QFile(QStringLiteral("state_after.txt"));
        beforeFile->open(QIODevice::WriteOnly);
        afterFile->open(QIODevice::WriteOnly);
        QDebug beforeInit(beforeFile);
        QDebug afterInit(afterFile);
        beforeInit << "GL states before 'context switch'" << endl;
        afterInit << "GL states after 'context switch'" << endl;
    }
#endif

    m_maxVertexAttribs = qMin(maxVertexAttribs, 2); // Datavis only uses 2 attribs max
    m_vertexAttribArrayEnabledStates.reset(new GLint[maxVertexAttribs]);
    m_vertexAttribArrayBoundBuffers.reset(new GLint[maxVertexAttribs]);
    m_vertexAttribArraySizes.reset(new GLint[maxVertexAttribs]);
    m_vertexAttribArrayTypes.reset(new GLint[maxVertexAttribs]);
    m_vertexAttribArrayNormalized.reset(new GLint[maxVertexAttribs]);
    m_vertexAttribArrayStrides.reset(new GLint[maxVertexAttribs]);
    m_vertexAttribArrayOffsets.reset(new void *[maxVertexAttribs]);

    initGLDefaultState();
}

GLStateStore::~GLStateStore()
{
#ifdef VERBOSE_STATE_STORE
    EnumToStringMap::deleteInstance();
    m_map = 0;
#endif
}

void GLStateStore::storeGLState()
{
#ifdef VERBOSE_STATE_STORE
    printCurrentState(true);
#endif

#if !defined(QT_OPENGL_ES_2)
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_drawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_readFramebuffer);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &m_renderbuffer);
#endif
    glGetFloatv(GL_COLOR_CLEAR_VALUE, m_clearColor);
    m_isBlendingEnabled = glIsEnabled(GL_BLEND);
    m_isDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &m_isDepthWriteEnabled);
    glGetFloatv(GL_DEPTH_CLEAR_VALUE, &m_clearDepth);
    glGetIntegerv(GL_DEPTH_FUNC, &m_depthFunc);
    glGetBooleanv(GL_POLYGON_OFFSET_FILL, &m_polygonOffsetFillEnabled);
    glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &m_polygonOffsetFactor);
    glGetFloatv(GL_POLYGON_OFFSET_UNITS, &m_polygonOffsetUnits);

    glGetIntegerv(GL_CURRENT_PROGRAM, &m_currentProgram);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &m_activeTexture);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_texBinding2D);
    glGetIntegerv(GL_FRONT_FACE, &m_frontFace);
    m_isCullFaceEnabled = glIsEnabled(GL_CULL_FACE);
    glGetIntegerv(GL_CULL_FACE_MODE, &m_cullFaceMode);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &m_blendEquationRGB);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &m_blendEquationAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &m_blendDestAlpha);
    glGetIntegerv(GL_BLEND_DST_RGB, &m_blendDestRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &m_blendSrcAlpha);
    glGetIntegerv(GL_BLEND_SRC_RGB, &m_blendSrcRGB);
    glGetIntegerv(GL_SCISSOR_BOX, m_scissorBox);
    m_isScissorTestEnabled = glIsEnabled(GL_SCISSOR_TEST);

    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &m_boundArrayBuffer);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &m_boundElementArrayBuffer);

    for (int i = 0; i < m_maxVertexAttribs;i++) {
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED,
                            &m_vertexAttribArrayEnabledStates[i]);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
                            &m_vertexAttribArrayBoundBuffers[i]);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &m_vertexAttribArraySizes[i]);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, &m_vertexAttribArrayTypes[i]);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,
                            &m_vertexAttribArrayNormalized[i]);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &m_vertexAttribArrayStrides[i]);
    }
}

#ifdef VERBOSE_STATE_STORE
void GLStateStore::printCurrentState(bool in)
{
    QFile *file;
    if (in)
        file = beforeFile;
    else
        file = afterFile;

    if (file->isOpen()) {
        QDebug msg(file);
#if !defined(QT_OPENGL_ES_2)
        GLint drawFramebuffer;
        GLint readFramebuffer;
        GLint renderbuffer;
#endif
        GLfloat clearColor[4];
        GLfloat clearDepth;
        GLboolean isBlendingEnabled = glIsEnabled(GL_BLEND);
        GLboolean isDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
        GLint depthFunc;
        GLboolean isDepthWriteEnabled;
        GLint currentProgram;
        GLint *vertexAttribArrayEnabledStates = new GLint[m_maxVertexAttribs];
        GLint *vertexAttribArrayBoundBuffers = new GLint[m_maxVertexAttribs];
        GLint *vertexAttribArraySizes = new GLint[m_maxVertexAttribs];
        GLint *vertexAttribArrayTypes = new GLint[m_maxVertexAttribs];
        GLint *vertexAttribArrayNormalized = new GLint[m_maxVertexAttribs];
        GLint *vertexAttribArrayStrides = new GLint[m_maxVertexAttribs];
        GLint activeTexture;
        GLint texBinding2D;
        GLint arrayBufferBinding;
        GLint frontFace;
        GLboolean isCullFaceEnabled = glIsEnabled(GL_CULL_FACE);
        GLint cullFaceMode;
        GLint blendEquationRGB;
        GLint blendEquationAlpha;

        GLint blendDestAlpha;
        GLint blendDestRGB;
        GLint blendSrcAlpha;
        GLint blendSrcRGB;
        GLint scissorBox[4];
        GLboolean isScissorTestEnabled = glIsEnabled(GL_SCISSOR_TEST);
        GLint boundElementArrayBuffer;
        GLboolean polygonOffsetFillEnabled;
        GLfloat polygonOffsetFactor;
        GLfloat polygonOffsetUnits;

        glGetBooleanv(GL_DEPTH_WRITEMASK, &isDepthWriteEnabled);
#if !defined(QT_OPENGL_ES_2)
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
#endif
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
        glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
        glGetBooleanv(GL_POLYGON_OFFSET_FILL, &polygonOffsetFillEnabled);
        glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &polygonOffsetFactor);
        glGetFloatv(GL_POLYGON_OFFSET_UNITS, &polygonOffsetUnits);

        glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texBinding2D );
        glGetIntegerv(GL_FRONT_FACE, &frontFace);
        glGetIntegerv(GL_CULL_FACE_MODE, &cullFaceMode);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRGB);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestAlpha);
        glGetIntegerv(GL_BLEND_DST_RGB, &blendDestRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
        glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRGB);
        glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &boundElementArrayBuffer);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBufferBinding);

        for (int i = 0; i < m_maxVertexAttribs;i++) {
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED,
                                &vertexAttribArrayEnabledStates[i]);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
                                &vertexAttribArrayBoundBuffers[i]);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &vertexAttribArraySizes[i]);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, &vertexAttribArrayTypes[i]);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,
                                &vertexAttribArrayNormalized[i]);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &vertexAttribArrayStrides[i]);
        }

        QColor color;
        color.setRgbF(clearColor[0], clearColor[1], clearColor[2]);
        color.setAlphaF(clearColor[3]);

#if !defined(QT_OPENGL_ES_2)
        msg << "---" << endl;
        msg << "    GL_DRAW_FRAMEBUFFER_BINDING "<< drawFramebuffer << endl;
        msg << "    GL_READ_FRAMEBUFFER_BINDING "<< readFramebuffer << endl;
        msg << "    GL_RENDERBUFFER_BINDING " << renderbuffer << endl;
#endif
        msg << "    GL_SCISSOR_TEST " << bool(isScissorTestEnabled) << endl;
        msg << "    GL_SCISSOR_BOX " << m_scissorBox[0] << m_scissorBox[1] << m_scissorBox[2]
            << m_scissorBox[3] << endl;
        msg << "    GL_COLOR_CLEAR_VALUE "<< color << endl;
        msg << "    GL_DEPTH_CLEAR_VALUE "<< clearDepth << endl;
        msg << "    GL_BLEND "<< bool(isBlendingEnabled) << endl;
        msg << "    GL_BLEND_EQUATION_RGB" << m_map->lookUp(blendEquationRGB) << endl;
        msg << "    GL_BLEND_EQUATION_ALPHA" << m_map->lookUp(blendEquationAlpha) << endl;
        msg << "    GL_BLEND_DST_ALPHA" << m_map->lookUp(blendDestAlpha) << endl;
        msg << "    GL_BLEND_DST_RGB" << m_map->lookUp(blendDestRGB) << endl;
        msg << "    GL_BLEND_SRC_ALPHA" << m_map->lookUp(blendSrcAlpha) << endl;
        msg << "    GL_BLEND_SRC_RGB" << m_map->lookUp(blendSrcRGB) << endl;
        msg << "    GL_DEPTH_TEST "<< bool(isDepthTestEnabled) << endl;
        msg << "    GL_DEPTH_WRITEMASK "<< bool(isDepthWriteEnabled) << endl;
        msg << "    GL_POLYGON_OFFSET_FILL" << bool(polygonOffsetFillEnabled) << endl;
        msg << "    GL_POLYGON_OFFSET_FACTOR "<< polygonOffsetFactor << endl;
        msg << "    GL_POLYGON_OFFSET_UNITS "<< polygonOffsetUnits << endl;
        msg << "    GL_CULL_FACE "<< bool(isCullFaceEnabled) << endl;
        msg << "    GL_CULL_FACE_MODE "<< m_map->lookUp(cullFaceMode) << endl;
        msg << "    GL_DEPTH_FUNC "<< m_map->lookUp(depthFunc) << endl;
        msg << "    GL_FRONT_FACE "<< m_map->lookUp(frontFace) << endl;
        msg << "    GL_CURRENT_PROGRAM "<< currentProgram << endl;
        msg << "    GL_ACTIVE_TEXTURE "<< QString("0x%1").arg(activeTexture, 0, 16) << endl;
        msg << "    GL_TEXTURE_BINDING_2D "<< texBinding2D << endl;
        msg << "    GL_ELEMENT_ARRAY_BUFFER_BINDING "<< boundElementArrayBuffer << endl;
        msg << "    GL_ARRAY_BUFFER_BINDING "<< arrayBufferBinding << endl;
        for (int i = 0; i < m_maxVertexAttribs;i++) {
            msg << "    GL_VERTEX_ATTRIB_ARRAY_ENABLED "<< i << "       = "
                <<  bool(vertexAttribArrayEnabledStates[i]) << endl;
            msg << "    GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING"<< i << " = "
                <<  vertexAttribArrayBoundBuffers[i] << endl;
            msg << "    GL_VERTEX_ATTRIB_ARRAY_SIZE"<< i << " = "
                <<  vertexAttribArraySizes[i] << endl;
            msg << "    GL_VERTEX_ATTRIB_ARRAY_TYPE"<< i << " = "
                <<  vertexAttribArrayTypes[i] << endl;
            msg << "    GL_VERTEX_ATTRIB_ARRAY_NORMALIZED"<< i << " = "
                <<  vertexAttribArrayNormalized[i] << endl;
            msg << "    GL_VERTEX_ATTRIB_ARRAY_STRIDE"<< i << " = "
                <<  vertexAttribArrayStrides[i] << endl;
        }
    }
}
#endif

void GLStateStore::restoreGLState()
{
#if !defined(QT_OPENGL_ES_2)
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_readFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_drawFramebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER_BINDING, m_renderbuffer);
#endif

    if (m_isScissorTestEnabled)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    glScissor(m_scissorBox[0], m_scissorBox[1], m_scissorBox[2], m_scissorBox[3]);
    glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
    glClearDepthf(m_clearDepth);
    if (m_isBlendingEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (m_isDepthTestEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (m_isCullFaceEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);

    glCullFace(m_cullFaceMode);

    glBlendEquationSeparate(m_blendEquationRGB, m_blendEquationAlpha);
    glBlendFuncSeparate(m_blendSrcRGB, m_blendDestRGB, m_blendSrcAlpha, m_blendDestAlpha);

    glDepthMask(m_isDepthWriteEnabled);
    glDepthFunc(m_depthFunc);
    glFrontFace(m_frontFace);

    if (m_polygonOffsetFillEnabled)
        glEnable(GL_POLYGON_OFFSET_FILL);
    else
        glDisable(GL_POLYGON_OFFSET_FILL);

    glPolygonOffset(m_polygonOffsetFactor, m_polygonOffsetUnits);

    glUseProgram(m_currentProgram);

    glActiveTexture(m_activeTexture);
    glBindTexture(GL_TEXTURE_2D, m_texBinding2D);

    // Restore bound element array buffer and array buffers
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_boundElementArrayBuffer);
    for (int i = 0; i < m_maxVertexAttribs; i++) {
        if (m_vertexAttribArrayEnabledStates[i])
            glEnableVertexAttribArray(i);
        else
            glDisableVertexAttribArray(i);

        glBindBuffer(GL_ARRAY_BUFFER, m_vertexAttribArrayBoundBuffers[i]);
        glVertexAttribPointer(i, m_vertexAttribArraySizes[i],
                              m_vertexAttribArrayTypes[i],
                              m_vertexAttribArrayNormalized[i],
                              m_vertexAttribArrayStrides[i],
                              m_vertexAttribArrayOffsets[i]);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_boundArrayBuffer);

#ifdef VERBOSE_STATE_STORE
    printCurrentState(false);
#endif
}

void GLStateStore::initGLDefaultState()
{
#if !defined(QT_OPENGL_ES_2)
    m_drawFramebuffer = 0;
    m_readFramebuffer = 0;
    m_renderbuffer = 0;
#endif
    m_clearColor[0] = m_clearColor[1] = m_clearColor[2] = m_clearColor[3] = 1.0f;
    m_clearDepth = 1.0f;
    m_isBlendingEnabled = GL_FALSE;
    m_isDepthTestEnabled = GL_FALSE;
    m_depthFunc = GL_LESS;
    m_isDepthWriteEnabled = GL_TRUE;
    m_currentProgram = 0;
    m_texBinding2D = 0;
    for (int i = 0; i < m_maxVertexAttribs;i++) {
        m_vertexAttribArrayEnabledStates[i] = GL_FALSE;
        m_vertexAttribArrayBoundBuffers[i] = 0;
        m_vertexAttribArraySizes[i] = 4;
        m_vertexAttribArrayTypes[i] = GL_FLOAT;
        m_vertexAttribArrayNormalized[i] = GL_FALSE;
        m_vertexAttribArrayStrides[i] = 0;
        m_vertexAttribArrayOffsets[i] = 0;
    }
    m_activeTexture = GL_TEXTURE0;
    m_frontFace = GL_CCW;
    m_isCullFaceEnabled = false;
    m_cullFaceMode = GL_BACK;
    m_blendEquationRGB = GL_FUNC_ADD;
    m_blendEquationAlpha = GL_FUNC_ADD;
    m_scissorBox[0] = 0;
    m_scissorBox[1] = 0;
    m_scissorBox[2] = 0;
    m_scissorBox[3] = 0;
    m_isScissorTestEnabled = GL_FALSE;

    m_polygonOffsetFillEnabled = GL_FALSE;
    m_polygonOffsetFactor = 0.0;
    m_polygonOffsetUnits = 0.0;
}
