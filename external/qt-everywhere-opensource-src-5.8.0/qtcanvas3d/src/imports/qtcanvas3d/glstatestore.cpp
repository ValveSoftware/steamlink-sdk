/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
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

#include "glstatestore_p.h"

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

GLStateStore::GLStateStore(QOpenGLContext *context, GLint maxAttribs,
                           CanvasGlCommandQueue &commandQueue,
                           QObject *parent) :
    QObject(parent),
    QOpenGLFunctions(context),
    m_commandQueue(commandQueue),
    m_maxVertexAttribs(maxAttribs),
    m_highestUsedAttrib(-1)
{
    m_vertexAttribArrayEnabledStates = new bool[m_maxVertexAttribs];
    m_vertexAttribArrayBoundBuffers = new GLint[m_maxVertexAttribs];
    m_vertexAttribArraySizes = new GLint[m_maxVertexAttribs];
    m_vertexAttribArrayTypes = new GLenum[m_maxVertexAttribs];
    m_vertexAttribArrayNormalized = new GLboolean[m_maxVertexAttribs];
    m_vertexAttribArrayStrides = new GLint[m_maxVertexAttribs];
    m_vertexAttribArrayOffsets = new GLint[m_maxVertexAttribs];

    initGLDefaultState();
}

GLStateStore::~GLStateStore()
{
    delete[] m_vertexAttribArrayEnabledStates;
    delete[] m_vertexAttribArrayBoundBuffers;
    delete[] m_vertexAttribArraySizes;
    delete[] m_vertexAttribArrayTypes;
    delete[] m_vertexAttribArrayNormalized;
    delete[] m_vertexAttribArrayStrides;
    delete[] m_vertexAttribArrayOffsets;
}

void GLStateStore::storeStateCommand(const GlCommand &command)
{
    switch (command.id) {
    case CanvasGlCommandQueue::glActiveTexture: {
        m_activeTexture = GLenum(command.i1);
        break;
    }
    case CanvasGlCommandQueue::glBindBuffer: {
        GLenum type = GLenum(command.i1);
        GLuint id = m_commandQueue.getGlId(command.i2);
        if (type == GL_ARRAY_BUFFER)
            m_boundArrayBuffer = id;
        else if (type == GL_ELEMENT_ARRAY_BUFFER)
            m_boundElementArrayBuffer = id;
        break;
    }
        // Note: glBindFrameBuffer is handled by renderer
    case CanvasGlCommandQueue::glBindRenderbuffer: {
        m_boundRenderbuffer = m_commandQueue.getGlId(command.i2);
        break;
    }
    case CanvasGlCommandQueue::glBindTexture: {
        GLenum type = GLenum(command.i1);
        GLuint id = m_commandQueue.getGlId(command.i2);
        if (type == GL_TEXTURE_2D)
            m_boundTexture2D = id;
        else if (type == GL_TEXTURE_CUBE_MAP)
            m_boundTextureCubeMap = id;
        break;
    }
    case CanvasGlCommandQueue::glBlendColor: {
        m_blendColor[0] = GLclampf(command.f1);
        m_blendColor[1] = GLclampf(command.f2);
        m_blendColor[2] = GLclampf(command.f3);
        m_blendColor[3] = GLclampf(command.f4);
        break;
    }
    case CanvasGlCommandQueue::glBlendEquation: {
        m_blendEquationRGB = GLenum(command.i1);
        m_blendEquationAlpha = m_blendEquationRGB;
        break;
    }
    case CanvasGlCommandQueue::glBlendEquationSeparate: {
        m_blendEquationRGB = GLenum(command.i1);
        m_blendEquationAlpha = GLenum(command.i2);
        break;
    }
    case CanvasGlCommandQueue::glBlendFunc: {
        m_blendFuncSrcRGB = GLenum(command.i1);
        m_blendFuncSrcAlpha = m_blendFuncSrcRGB;
        m_blendFuncDestRGB = GLenum(command.i2);
        m_blendFuncDestAlpha = m_blendFuncDestRGB;
        break;
    }
    case CanvasGlCommandQueue::glBlendFuncSeparate: {
        m_blendFuncSrcRGB = GLenum(command.i1);
        m_blendFuncSrcAlpha = GLenum(command.i3);
        m_blendFuncDestRGB = GLenum(command.i2);
        m_blendFuncDestAlpha = GLenum(command.i4);
        break;
    }
    case CanvasGlCommandQueue::glClearColor: {
        m_clearColor[0] = GLclampf(command.f1);
        m_clearColor[1] = GLclampf(command.f2);
        m_clearColor[2] = GLclampf(command.f3);
        m_clearColor[3] = GLclampf(command.f4);
        break;
    }
    case CanvasGlCommandQueue::glClearDepthf: {
        m_clearDepth = GLclampf(command.f1);
        break;
    }
    case CanvasGlCommandQueue::glClearStencil: {
        m_clearStencil = command.i1;
        break;
    }
    case CanvasGlCommandQueue::glColorMask: {
        m_colorMask[0] = GLboolean(command.i1);
        m_colorMask[1] = GLboolean(command.i2);
        m_colorMask[2] = GLboolean(command.i3);
        m_colorMask[3] = GLboolean(command.i4);
        break;
    }
    case CanvasGlCommandQueue::glCullFace: {
        m_cullFace = GLenum(command.i1);
        break;
    }
    case CanvasGlCommandQueue::glDepthFunc: {
        m_depthFunc = GLenum(command.i1);
        break;
    }
    case CanvasGlCommandQueue::glDepthMask: {
        m_depthMask = GLboolean(command.i1);
        break;
    }
    case CanvasGlCommandQueue::glDepthRangef: {
        m_depthRange[0] = GLclampf(command.f1);
        m_depthRange[1] = GLclampf(command.f2);
        break;
    }
    case CanvasGlCommandQueue::glDisable: {
        enableDisable(false, GLenum(command.i1));
        break;
    }
    case CanvasGlCommandQueue::glDisableVertexAttribArray: {
        if (command.i1 >= 0 && command.i1 < m_maxVertexAttribs)
            m_vertexAttribArrayEnabledStates[command.i1] = false;
        break;
    }
    case CanvasGlCommandQueue::glEnable: {
        enableDisable(true, GLenum(command.i1));
        break;
    }
    case CanvasGlCommandQueue::glEnableVertexAttribArray: {
        if (command.i1 >= 0 && command.i1 < m_maxVertexAttribs) {
            if (command.i1 > m_highestUsedAttrib)
                m_highestUsedAttrib = command.i1;
            m_vertexAttribArrayEnabledStates[command.i1] = true;
        }
        break;
    }
    case CanvasGlCommandQueue::glFrontFace: {
        m_frontFace = GLenum(command.i1);
        break;
    }
    case CanvasGlCommandQueue::glHint: {
        if (GLenum(command.i1) == GL_GENERATE_MIPMAP_HINT)
            m_hintMode = GLenum(command.i2);
        break;
    }
    case CanvasGlCommandQueue::glLineWidth: {
        m_lineWidth = command.f1;
        break;
    }
    case CanvasGlCommandQueue::glPixelStorei: {
        GLenum type = GLenum(command.i1);
        if (type == GL_PACK_ALIGNMENT)
            m_packAlignment = command.i2;
        else if (type == GL_UNPACK_ALIGNMENT)
            m_unpackAlignment = command.i2;
        break;
    }
    case CanvasGlCommandQueue::glPolygonOffset: {
        m_polygonOffsetFactor = command.f1;
        m_polygonOffsetUnits = command.f2;
        break;
    }
    case CanvasGlCommandQueue::glSampleCoverage: {
        m_sampleCoverageValue = GLclampf(command.f1);
        m_sampleCoverageInvert = GLboolean(command.i1);
        break;
    }
    case CanvasGlCommandQueue::glScissor: {
        m_scissorBox[0] = command.i1;
        m_scissorBox[1] = command.i2;
        m_scissorBox[2] = command.i3;
        m_scissorBox[3] = command.i4;
        break;
    }
    case CanvasGlCommandQueue::glStencilFunc: {
        m_stencilFuncFront = GLenum(command.i1);
        m_stencilFuncRefFront = command.i2;
        m_stencilFuncMaskFront = GLuint(command.i3);
        m_stencilFuncBack = m_stencilFuncFront;
        m_stencilFuncRefBack = m_stencilFuncRefFront;
        m_stencilFuncMaskBack = m_stencilFuncMaskFront;
        break;
    }
    case CanvasGlCommandQueue::glStencilFuncSeparate: {
        GLenum target = GLenum(command.i1);
        if (target == GL_FRONT || target == GL_FRONT_AND_BACK) {
            m_stencilFuncFront = GLenum(command.i2);
            m_stencilFuncRefFront = command.i3;
            m_stencilFuncMaskFront = GLuint(command.i4);
        }
        if (target == GL_BACK || target == GL_FRONT_AND_BACK) {
            m_stencilFuncBack = GLenum(command.i2);
            m_stencilFuncRefBack = command.i3;
            m_stencilFuncMaskBack = GLuint(command.i4);
        }
        break;
    }
    case CanvasGlCommandQueue::glStencilMask: {
        m_stencilMaskFront = GLuint(command.i1);
        m_stencilMaskBack = m_stencilMaskFront;
        break;
    }
    case CanvasGlCommandQueue::glStencilMaskSeparate: {
        GLenum target = GLenum(command.i1);
        if (target == GL_FRONT || target == GL_FRONT_AND_BACK)
            m_stencilMaskFront = GLuint(command.i2);
        if (target == GL_BACK || target == GL_FRONT_AND_BACK)
            m_stencilMaskBack = GLuint(command.i2);
        break;
    }
    case CanvasGlCommandQueue::glStencilOp: {
        m_stancilOpSFailFront = GLenum(command.i1);
        m_stancilOpDpFailFront = GLenum(command.i2);
        m_stancilOpDpPassFront = GLenum(command.i3);
        m_stancilOpSFailBack = m_stancilOpSFailFront;
        m_stancilOpDpFailBack = m_stancilOpDpFailFront;
        m_stancilOpDpPassBack = m_stancilOpDpPassFront;
        break;
    }
    case CanvasGlCommandQueue::glStencilOpSeparate: {
        GLenum target = GLenum(command.i1);
        if (target == GL_FRONT || target == GL_FRONT_AND_BACK) {
            m_stancilOpSFailFront = GLenum(command.i2);
            m_stancilOpDpFailFront = GLenum(command.i3);
            m_stancilOpDpPassFront = GLenum(command.i4);
        }
        if (target == GL_BACK || target == GL_FRONT_AND_BACK) {
            m_stancilOpSFailBack = GLenum(command.i2);
            m_stancilOpDpFailBack = GLenum(command.i3);
            m_stancilOpDpPassBack = GLenum(command.i4);
        }
        break;
    }
    case CanvasGlCommandQueue::glUseProgram: {
        QOpenGLShaderProgram *program = m_commandQueue.getProgram(command.i1);
        if (program)
            m_boundProgram = program->programId();
        break;
    }
    case CanvasGlCommandQueue::glVertexAttribPointer: {
        if (command.i1 >= 0 && command.i1 < m_maxVertexAttribs) {
            if (command.i1 > m_highestUsedAttrib)
                m_highestUsedAttrib = command.i1;
            m_vertexAttribArrayBoundBuffers[command.i1] = m_boundArrayBuffer;
            m_vertexAttribArraySizes[command.i1] = command.i2;
            m_vertexAttribArrayTypes[command.i1] = GLenum(command.i3);
            m_vertexAttribArrayNormalized[command.i1] = GLboolean(command.i4);
            m_vertexAttribArrayStrides[command.i1] = command.i5;
            m_vertexAttribArrayOffsets[command.i1] = command.i6;
        }
        break;
    }
        // Note: glViewport is handled by renderer
    default:
        // Silently ignore other commands, as they do not change global state
        break;
    }
}

void GLStateStore::initGLDefaultState()
{
    // Initialize to default OpenGL values
    m_activeTexture = GL_TEXTURE0;
    m_boundArrayBuffer = 0;
    m_boundElementArrayBuffer = 0;
    m_boundRenderbuffer = 0;
    m_boundTexture2D = 0;
    m_boundTextureCubeMap = 0;
    m_blendColor[0] = m_blendColor[1] = m_blendColor[2] = m_blendColor[3] = 0.0f;
    m_blendEquationRGB = m_blendEquationAlpha = GL_FUNC_ADD;
    m_blendFuncSrcRGB = m_blendFuncSrcAlpha = GL_ONE;
    m_blendFuncDestRGB = m_blendFuncDestAlpha = GL_ZERO;
    m_clearColor[0] = m_clearColor[1] = m_clearColor[2] = m_clearColor[3] = 0.0f;
    m_clearDepth = 1.0f;
    m_clearStencil = 0;
    m_colorMask[0] = m_colorMask[1] = m_colorMask[2] = m_colorMask[3] = GL_TRUE;
    m_cullFace = GL_BACK;
    m_depthFunc = GL_LESS;
    m_depthMask = GL_TRUE;
    m_depthRange[0] = 0;
    m_depthRange[1] = 1;

    m_blendEnabled = GL_FALSE;
    m_cullFaceEnabled = GL_FALSE;
    m_depthTestEnabled = GL_FALSE;
    m_ditherEnabled = GL_TRUE;
    m_polygonOffsetFillEnabled = GL_FALSE;
    m_sampleAlphaToCoverageEnabled = GL_FALSE;
    m_sampleCoverageEnabled = GL_FALSE;
    m_scissorTestEnabled = GL_FALSE;
    m_stencilTestEnabled = GL_FALSE;

    m_frontFace = GL_CCW;
    m_hintMode = GL_DONT_CARE;
    m_lineWidth = 1;
    m_packAlignment = m_unpackAlignment = 4;
    m_polygonOffsetFactor = m_polygonOffsetUnits = 0.0f;
    m_sampleCoverageValue = 1.0f;
    m_sampleCoverageInvert = GL_FALSE;

    // Initialize scissor box to the actual scissor box as we initialize it in canvas
    glGetIntegerv(GL_SCISSOR_BOX, m_scissorBox);

    m_stencilFuncFront = m_stencilFuncBack = GL_ALWAYS;
    m_stencilFuncRefFront = m_stencilFuncRefBack = 0;
    m_stencilFuncMaskFront = m_stencilFuncMaskBack = 0xffffffff;
    m_stencilMaskFront = m_stencilMaskBack = 0xffffffff;
    m_stancilOpSFailFront = m_stancilOpSFailBack = GL_KEEP;
    m_stancilOpDpFailFront = m_stancilOpDpFailBack = GL_KEEP;
    m_stancilOpDpPassFront = m_stancilOpDpPassBack = GL_KEEP;

    m_boundProgram = 0;

    for (int i = 0; i < m_maxVertexAttribs;i++) {
        m_vertexAttribArrayEnabledStates[i] = GL_FALSE;
        m_vertexAttribArrayBoundBuffers[i] = 0;
        m_vertexAttribArraySizes[i] = 4;
        m_vertexAttribArrayTypes[i] = GL_FLOAT;
        m_vertexAttribArrayNormalized[i] = GL_FALSE;
        m_vertexAttribArrayStrides[i] = 0;
        m_vertexAttribArrayOffsets[i] = 0;
    }
}

void GLStateStore::restoreStoredState()
{
    glActiveTexture(m_activeTexture);

    glBindRenderbuffer(GL_RENDERBUFFER, m_boundRenderbuffer);
    glBindTexture(GL_TEXTURE_2D, m_boundTexture2D);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_boundTextureCubeMap);

    if (m_blendEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (m_cullFaceEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);

    if (m_depthTestEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (m_ditherEnabled)
        glEnable(GL_DITHER);
    else
        glDisable(GL_DITHER);

    if (m_polygonOffsetFillEnabled)
        glEnable(GL_POLYGON_OFFSET_FILL);
    else
        glDisable(GL_POLYGON_OFFSET_FILL);

    if (m_sampleAlphaToCoverageEnabled)
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    else
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

    if (m_sampleCoverageEnabled)
        glEnable(GL_SAMPLE_COVERAGE);
    else
        glDisable(GL_SAMPLE_COVERAGE);

    if (m_scissorTestEnabled)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    if (m_stencilTestEnabled)
        glEnable(GL_STENCIL_TEST);
    else
        glDisable(GL_STENCIL_TEST);

    glBlendColor(m_blendColor[0], m_blendColor[1], m_blendColor[2], m_blendColor[3]);
    glBlendEquationSeparate(m_blendEquationRGB, m_blendEquationAlpha);
    glBlendFuncSeparate(m_blendFuncSrcRGB, m_blendFuncDestRGB,
                        m_blendFuncSrcAlpha, m_blendFuncDestAlpha);

    glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
    glClearDepthf(m_clearDepth);
    glClearStencil(m_clearStencil);

    glColorMask(m_colorMask[0], m_colorMask[1], m_colorMask[2], m_colorMask[3]);
    glCullFace(m_cullFace);
    glDepthFunc(m_depthFunc);
    glDepthMask(m_depthMask);
    glDepthRangef(m_depthRange[0], m_depthRange[1]);

    glFrontFace(m_frontFace);
    glHint(GL_GENERATE_MIPMAP_HINT, m_hintMode);
    glLineWidth(m_lineWidth);
    glPixelStorei(GL_PACK_ALIGNMENT, m_packAlignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, m_unpackAlignment);
    glPolygonOffset(m_polygonOffsetFactor, m_polygonOffsetUnits);
    glSampleCoverage(m_sampleCoverageValue, m_sampleCoverageInvert);

    glScissor(m_scissorBox[0], m_scissorBox[1], m_scissorBox[2], m_scissorBox[3]);

    glStencilFuncSeparate(GL_FRONT, m_stencilFuncFront, m_stencilFuncRefFront,
                          m_stencilFuncMaskFront);
    glStencilFuncSeparate(GL_BACK, m_stencilFuncBack, m_stencilFuncRefBack,
                          m_stencilFuncMaskBack);
    glStencilMaskSeparate(GL_FRONT, m_stencilMaskFront);
    glStencilMaskSeparate(GL_BACK, m_stencilMaskBack);
    glStencilOpSeparate(GL_FRONT, m_stancilOpSFailFront, m_stancilOpDpFailFront,
                        m_stancilOpDpPassFront);
    glStencilOpSeparate(GL_BACK, m_stancilOpSFailBack, m_stancilOpDpFailBack,
                        m_stancilOpDpPassBack);

    glUseProgram(m_boundProgram);

    for (int i = 0; i <= m_highestUsedAttrib; i++) {
        if (m_vertexAttribArrayEnabledStates[i])
            glEnableVertexAttribArray(i);
        else
            glDisableVertexAttribArray(i);

        glBindBuffer(GL_ARRAY_BUFFER, m_vertexAttribArrayBoundBuffers[i]);
        glVertexAttribPointer(i, m_vertexAttribArraySizes[i],
                              m_vertexAttribArrayTypes[i],
                              m_vertexAttribArrayNormalized[i],
                              m_vertexAttribArrayStrides[i],
                              reinterpret_cast<const GLvoid *>(m_vertexAttribArrayOffsets[i]));
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_boundArrayBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_boundElementArrayBuffer);
}

void GLStateStore::enableDisable(bool enable, GLenum flag)
{
    switch (flag) {
    case GL_BLEND:
        m_blendEnabled = enable;
        break;
    case GL_CULL_FACE:
        m_cullFaceEnabled = enable;
        break;
    case GL_DEPTH_TEST:
        m_depthTestEnabled = enable;
        break;
    case GL_DITHER:
        m_ditherEnabled = enable;
        break;
    case GL_POLYGON_OFFSET_FILL:
        m_polygonOffsetFillEnabled = enable;
        break;
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
        m_sampleAlphaToCoverageEnabled = enable;
        break;
    case GL_SAMPLE_COVERAGE:
        m_sampleCoverageEnabled = enable;
        break;
    case GL_SCISSOR_TEST:
        m_scissorTestEnabled = enable;
        break;
    case GL_STENCIL_TEST:
        m_stencilTestEnabled = enable;
        break;
    default:
        // Unsupported flag, just ignore
        break;
    }
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
