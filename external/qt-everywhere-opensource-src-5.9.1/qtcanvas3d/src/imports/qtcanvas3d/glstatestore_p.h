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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtCanvas3D API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#ifndef GLSTATESTORE_P_H
#define GLSTATESTORE_P_H

#include "glcommandqueue_p.h"
#include <QtGui/QOpenGLFunctions>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

class GLStateStore : public QObject, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit GLStateStore(QOpenGLContext *context, GLint maxAttribs,
                          CanvasGlCommandQueue &commandQueue, QObject *parent = 0);
    ~GLStateStore();

    void initGLDefaultState();
    void storeStateCommand(const GlCommand &command);
    void restoreStoredState();

private:
    void enableDisable(bool enable, GLenum flag);

    CanvasGlCommandQueue &m_commandQueue;

    GLenum m_activeTexture;
    GLuint m_boundArrayBuffer;
    GLuint m_boundElementArrayBuffer;
    GLuint m_boundRenderbuffer;
    GLuint m_boundTexture2D;
    GLuint m_boundTextureCubeMap;
    GLclampf m_blendColor[4];
    GLenum m_blendEquationRGB;
    GLenum m_blendEquationAlpha;
    GLenum m_blendFuncSrcRGB;
    GLenum m_blendFuncSrcAlpha;
    GLenum m_blendFuncDestRGB;
    GLenum m_blendFuncDestAlpha;
    GLclampf m_clearColor[4];
    GLclampf m_clearDepth;
    GLint m_clearStencil;
    GLboolean m_colorMask[4];
    GLenum m_cullFace;
    GLenum m_depthFunc;
    GLboolean m_depthMask;
    GLclampf m_depthRange[2];

    // enable/disable flags
    GLboolean m_blendEnabled;
    GLboolean m_cullFaceEnabled;
    GLboolean m_depthTestEnabled;
    GLboolean m_ditherEnabled;
    GLboolean m_polygonOffsetFillEnabled;
    GLboolean m_sampleAlphaToCoverageEnabled;
    GLboolean m_sampleCoverageEnabled;
    GLboolean m_scissorTestEnabled;
    GLboolean m_stencilTestEnabled;

    GLenum m_frontFace;
    GLenum m_hintMode;
    GLint m_lineWidth;
    GLint m_packAlignment;
    GLint m_unpackAlignment;
    GLfloat m_polygonOffsetFactor;
    GLfloat m_polygonOffsetUnits;
    GLclampf m_sampleCoverageValue;
    GLboolean m_sampleCoverageInvert;
    GLint m_scissorBox[4];

    GLenum m_stencilFuncFront;
    GLint m_stencilFuncRefFront;
    GLuint m_stencilFuncMaskFront;
    GLenum m_stencilFuncBack;
    GLint m_stencilFuncRefBack;
    GLuint m_stencilFuncMaskBack;
    GLuint m_stencilMaskFront;
    GLuint m_stencilMaskBack;
    GLenum m_stancilOpSFailFront;
    GLenum m_stancilOpDpFailFront;
    GLenum m_stancilOpDpPassFront;
    GLenum m_stancilOpSFailBack;
    GLenum m_stancilOpDpFailBack;
    GLenum m_stancilOpDpPassBack;

    GLuint m_boundProgram;

    GLint m_maxVertexAttribs;
    GLint m_highestUsedAttrib;
    bool *m_vertexAttribArrayEnabledStates;
    GLint *m_vertexAttribArrayBoundBuffers;
    GLint *m_vertexAttribArraySizes;
    GLenum *m_vertexAttribArrayTypes;
    GLboolean *m_vertexAttribArrayNormalized;
    GLint *m_vertexAttribArrayStrides;
    GLint *m_vertexAttribArrayOffsets;
};

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE

#endif // GLSTATESTORE_P_H
