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

#include "canvasglstatedump_p.h"
#include "enumtostringmap_p.h"
#include "glcommandqueue_p.h"

#include <QtCore/QDebug>
#include <QtGui/QColor>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>

#define BOOL_TO_STR(a) a ? "true" : "false"

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE


/*!
   \qmltype GLStateDumpExt
   \since QtCanvas3D 1.0
   \inqmlmodule QtCanvas3D
   \brief Provides means to print current GL driver state info.

   An uncreatable QML type that provides an extension API that can be used to dump current OpenGL
   driver state as a string that can be then, for example, be printed on the console log.
   You can get it by calling \l{Context3D::getExtension}{Context3D.getExtension} with
   \c{"QTCANVAS3D_gl_state_dump"} as parameter.

   Typical usage could be something like this:
   \code
    // Declare the variable to contain the extension
    var stateDumpExt;
    .
    .
    // After the context has been created from Canvas3D get the extension
    stateDumpExt = gl.getExtension("QTCANVAS3D_gl_state_dump");
    .
    .
    // When you want to print the current GL state with everything enabled
    // Check that you indeed have a valid extension (for portability) then use it
    if (stateDumpExt)
        log("GL STATE DUMP:\n"+stateDumpExt.getGLStateDump(stateDumpExt.DUMP_FULL));
    \endcode

   \sa Context3D
 */
CanvasGLStateDump::CanvasGLStateDump(CanvasContext *canvasContext, bool isEs, QObject *parent) :
    QObject(parent),
    m_maxVertexAttribs(0),
    m_map(EnumToStringMap::newInstance()),
    m_isOpenGLES2(isEs),
    m_canvasContext(canvasContext),
    m_options(DUMP_FULL)
{
}

CanvasGLStateDump::~CanvasGLStateDump()
{
    EnumToStringMap::deleteInstance();
    m_map = 0;
}

void CanvasGLStateDump::getGLArrayObjectDump(int target, int arrayObject, int type)
{
    if (!arrayObject)
        m_stateDumpStr.append("no buffer bound");

    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();

    funcs->glBindBuffer(target, arrayObject);

    GLint size;
    funcs->glGetBufferParameteriv(target, GL_BUFFER_SIZE, &size);

    if (type == GL_FLOAT) {
        m_stateDumpStr.append("ARRAY_BUFFER_TYPE......................FLOAT\n");

        m_stateDumpStr.append("ARRAY_BUFFER_SIZE......................");
        m_stateDumpStr.append(QString::number(size));
        m_stateDumpStr.append("\n");

    } else if (type == GL_UNSIGNED_SHORT) {
        m_stateDumpStr.append("ARRAY_BUFFER_TYPE......................UNSIGNED_SHORT\n");

        m_stateDumpStr.append("ARRAY_BUFFER_SIZE......................");
        m_stateDumpStr.append(QString::number(size));
        m_stateDumpStr.append("\n");
    }
}

/*!
 * This function does the actual state dump. It must be called from renderer thread and
 * inside a valid context.
 */
void CanvasGLStateDump::doGLStateDump()
{
#if !defined(QT_OPENGL_ES_2)
    GLint drawFramebuffer;
    GLint readFramebuffer;
    GLboolean polygonOffsetLineEnabled;
    GLboolean polygonOffsetPointEnabled;
    GLint boundVertexArray;
#endif

    QOpenGLFunctions *funcs = QOpenGLContext::currentContext()->functions();

    if (!m_maxVertexAttribs)
        funcs->glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &m_maxVertexAttribs);

    GLint renderbuffer;
    GLfloat clearColor[4];
    GLfloat clearDepth;
    GLboolean isBlendingEnabled = funcs->glIsEnabled(GL_BLEND);
    GLboolean isDepthTestEnabled = funcs->glIsEnabled(GL_DEPTH_TEST);
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
    GLboolean isCullFaceEnabled = funcs->glIsEnabled(GL_CULL_FACE);
    GLint cullFaceMode;
    GLint blendEquationRGB;
    GLint blendEquationAlpha;

    GLint blendDestAlpha;
    GLint blendDestRGB;
    GLint blendSrcAlpha;
    GLint blendSrcRGB;
    GLint scissorBox[4];
    GLboolean isScissorTestEnabled = funcs->glIsEnabled(GL_SCISSOR_TEST);
    GLint boundElementArrayBuffer;
    GLboolean polygonOffsetFillEnabled;
    GLfloat polygonOffsetFactor;
    GLfloat polygonOffsetUnits;

#if !defined(QT_OPENGL_ES_2)
    if (!m_isOpenGLES2) {
        funcs->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
        funcs->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
        funcs->glGetBooleanv(GL_POLYGON_OFFSET_LINE, &polygonOffsetLineEnabled);
        funcs->glGetBooleanv(GL_POLYGON_OFFSET_POINT, &polygonOffsetPointEnabled);
        funcs->glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &boundVertexArray);
    }
#endif

    funcs->glGetBooleanv(GL_DEPTH_WRITEMASK, &isDepthWriteEnabled);
    funcs->glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
    funcs->glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
    funcs->glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
    funcs->glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
    funcs->glGetBooleanv(GL_POLYGON_OFFSET_FILL, &polygonOffsetFillEnabled);
    funcs->glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &polygonOffsetFactor);
    funcs->glGetFloatv(GL_POLYGON_OFFSET_UNITS, &polygonOffsetUnits);

    funcs->glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    funcs->glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    funcs->glGetIntegerv(GL_TEXTURE_BINDING_2D, &texBinding2D );
    funcs->glGetIntegerv(GL_FRONT_FACE, &frontFace);
    funcs->glGetIntegerv(GL_CULL_FACE_MODE, &cullFaceMode);
    funcs->glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRGB);
    funcs->glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);
    funcs->glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestAlpha);
    funcs->glGetIntegerv(GL_BLEND_DST_RGB, &blendDestRGB);
    funcs->glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
    funcs->glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRGB);
    funcs->glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
    funcs->glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &boundElementArrayBuffer);
    funcs->glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBufferBinding);


#if !defined(QT_OPENGL_ES_2)
    if (!m_isOpenGLES2) {
        m_stateDumpStr.append("GL_DRAW_FRAMEBUFFER_BINDING.....");
        m_stateDumpStr.append(QString::number(drawFramebuffer));
        m_stateDumpStr.append("\n");

        m_stateDumpStr.append("GL_READ_FRAMEBUFFER_BINDING.....");
        m_stateDumpStr.append(QString::number(readFramebuffer));
        m_stateDumpStr.append("\n");
    }
#endif

    m_stateDumpStr.append("GL_RENDERBUFFER_BINDING.........");
    m_stateDumpStr.append(QString::number(renderbuffer));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_SCISSOR_TEST.................");
    m_stateDumpStr.append(BOOL_TO_STR(isScissorTestEnabled));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_SCISSOR_BOX..................");
    m_stateDumpStr.append(QString::number(scissorBox[0]));
    m_stateDumpStr.append(", ");
    m_stateDumpStr.append(QString::number(scissorBox[1]));
    m_stateDumpStr.append(", ");
    m_stateDumpStr.append(QString::number(scissorBox[2]));
    m_stateDumpStr.append(", ");
    m_stateDumpStr.append(QString::number(scissorBox[3]));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_COLOR_CLEAR_VALUE............");
    m_stateDumpStr.append("r:");
    m_stateDumpStr.append(QString::number(clearColor[0]));
    m_stateDumpStr.append(" g:");
    m_stateDumpStr.append(QString::number(clearColor[1]));
    m_stateDumpStr.append(" b:");
    m_stateDumpStr.append(QString::number(clearColor[2]));
    m_stateDumpStr.append(" a:");
    m_stateDumpStr.append(QString::number(clearColor[3]));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_DEPTH_CLEAR_VALUE............");
    m_stateDumpStr.append(QString::number(clearDepth));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_BLEND........................");
    m_stateDumpStr.append(BOOL_TO_STR(isBlendingEnabled));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_BLEND_EQUATION_RGB...........");
    m_stateDumpStr.append(m_map->lookUp(blendEquationRGB));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_BLEND_EQUATION_ALPHA.........");
    m_stateDumpStr.append(m_map->lookUp(blendEquationAlpha));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_DEPTH_TEST...................");
    m_stateDumpStr.append(BOOL_TO_STR(isDepthTestEnabled));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_DEPTH_FUNC...................");
    m_stateDumpStr.append(m_map->lookUp(depthFunc));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_DEPTH_WRITEMASK..............");
    m_stateDumpStr.append(BOOL_TO_STR(isDepthWriteEnabled));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_POLYGON_OFFSET_FILL..........");
    m_stateDumpStr.append(BOOL_TO_STR(polygonOffsetFillEnabled));
    m_stateDumpStr.append("\n");

#if !defined(QT_OPENGL_ES_2)
    if (!m_isOpenGLES2) {
        m_stateDumpStr.append("GL_POLYGON_OFFSET_LINE..........");
        m_stateDumpStr.append(BOOL_TO_STR(polygonOffsetLineEnabled));
        m_stateDumpStr.append("\n");

        m_stateDumpStr.append("GL_POLYGON_OFFSET_POINT.........");
        m_stateDumpStr.append(BOOL_TO_STR(polygonOffsetPointEnabled));
        m_stateDumpStr.append("\n");
    }
#endif

    m_stateDumpStr.append("GL_POLYGON_OFFSET_FACTOR........");
    m_stateDumpStr.append(QString::number(polygonOffsetFactor));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_POLYGON_OFFSET_UNITS.........");
    m_stateDumpStr.append(QString::number(polygonOffsetUnits));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_CULL_FACE....................");
    m_stateDumpStr.append(BOOL_TO_STR(isCullFaceEnabled));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_CULL_FACE_MODE...............");
    m_stateDumpStr.append(m_map->lookUp(cullFaceMode));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_FRONT_FACE...................");
    m_stateDumpStr.append(m_map->lookUp(frontFace));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_CURRENT_PROGRAM..............");
    m_stateDumpStr.append(QString::number(currentProgram));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_ACTIVE_TEXTURE...............");
    m_stateDumpStr.append(m_map->lookUp(activeTexture));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_TEXTURE_BINDING_2D...........");
    m_stateDumpStr.append(QString::number(texBinding2D));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_ELEMENT_ARRAY_BUFFER_BINDING.");
    m_stateDumpStr.append(QString::number(boundElementArrayBuffer));
    m_stateDumpStr.append("\n");

    m_stateDumpStr.append("GL_ARRAY_BUFFER_BINDING.........");
    m_stateDumpStr.append(QString::number(arrayBufferBinding));
    m_stateDumpStr.append("\n");

#if !defined(QT_OPENGL_ES_2)
    if (!m_isOpenGLES2) {
        m_stateDumpStr.append("GL_VERTEX_ARRAY_BINDING.........");
        m_stateDumpStr.append(QString::number(boundVertexArray));
        m_stateDumpStr.append("\n");
    }
#endif

    if (m_options & DUMP_VERTEX_ATTRIB_ARRAYS_BIT) {
        for (int i = 0; i < m_maxVertexAttribs;i++) {
            funcs->glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &vertexAttribArrayEnabledStates[i]);
            funcs->glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &vertexAttribArrayBoundBuffers[i]);
            funcs->glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &vertexAttribArraySizes[i]);
            funcs->glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, &vertexAttribArrayTypes[i]);
            funcs->glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &vertexAttribArrayNormalized[i]);
            funcs->glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &vertexAttribArrayStrides[i]);
        }


        for (int i = 0; i < m_maxVertexAttribs;i++) {
            m_stateDumpStr.append("GL_VERTEX_ATTRIB_ARRAY_");
            m_stateDumpStr.append(QString::number(i));
            m_stateDumpStr.append("\n");

            m_stateDumpStr.append("GL_VERTEX_ATTRIB_ARRAY_ENABLED.........");
            m_stateDumpStr.append(BOOL_TO_STR(vertexAttribArrayEnabledStates[i]));
            m_stateDumpStr.append("\n");

            m_stateDumpStr.append("GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING..");
            m_stateDumpStr.append(QString::number(vertexAttribArrayBoundBuffers[i]));
            m_stateDumpStr.append("\n");

            m_stateDumpStr.append("GL_VERTEX_ATTRIB_ARRAY_SIZE............");
            m_stateDumpStr.append(QString::number(vertexAttribArraySizes[i]));
            m_stateDumpStr.append("\n");

            m_stateDumpStr.append("GL_VERTEX_ATTRIB_ARRAY_TYPE............");
            m_stateDumpStr.append(m_map->lookUp(vertexAttribArrayTypes[i]));
            m_stateDumpStr.append("\n");

            m_stateDumpStr.append("GL_VERTEX_ATTRIB_ARRAY_NORMALIZED......");
            m_stateDumpStr.append(QString::number(vertexAttribArrayNormalized[i]));
            m_stateDumpStr.append("\n");

            m_stateDumpStr.append("GL_VERTEX_ATTRIB_ARRAY_STRIDE..........");
            m_stateDumpStr.append(QString::number(vertexAttribArrayStrides[i]));
            m_stateDumpStr.append("\n");
        }
    }

    if (m_options & DUMP_VERTEX_ATTRIB_ARRAYS_BUFFERS_BIT) {
        if (boundElementArrayBuffer != 0) {
            m_stateDumpStr.append("GL_ELEMENT_ARRAY_BUFFER................");
            m_stateDumpStr.append(QString::number(boundElementArrayBuffer));
            m_stateDumpStr.append("\n");

            getGLArrayObjectDump(GL_ELEMENT_ARRAY_BUFFER, boundElementArrayBuffer,
                                 GL_UNSIGNED_SHORT);
        }

        for (int i = 0; i < m_maxVertexAttribs;i++) {
            if (vertexAttribArrayEnabledStates[i]) {
                m_stateDumpStr.append("GL_ARRAY_BUFFER........................");
                m_stateDumpStr.append(QString::number(vertexAttribArrayBoundBuffers[i]));
                m_stateDumpStr.append("\n");

                getGLArrayObjectDump(GL_ARRAY_BUFFER, vertexAttribArrayBoundBuffers[i],
                                     vertexAttribArrayTypes[i]);
            }
        }
    }


    delete[] vertexAttribArrayEnabledStates;
    delete[] vertexAttribArrayBoundBuffers;
    delete[] vertexAttribArraySizes;
    delete[] vertexAttribArrayTypes;
    delete[] vertexAttribArrayNormalized;
    delete[] vertexAttribArrayStrides;
}

/*!
 * \qmlmethod string GLStateDumpExt::getGLStateDump(stateDumpEnums options)
 * \return OpenGL driver state with given options as a human readable string that can be printed.
 * Optional paremeter \a options may contain bitfields masked together from following options:
 * \list
 * \li \c{GLStateDumpExt.DUMP_BASIC_ONLY} Includes only very basic OpenGL state information.
 * \li \c{GLStateDumpExt.DUMP_VERTEX_ATTRIB_ARRAYS_BIT} Includes all vertex attribute array
 * information.
 * \li \c{GLStateDumpExt.DUMP_VERTEX_ATTRIB_ARRAYS_BUFFERS_BIT} Includes size and type
 * from all currently active vertex attribute arrays (including the currently bound element array)
 * to verify that there are actual values in the array.
 * \li \c{GLStateDumpExt.DUMP_FULL} Includes everything.
 * \endlist
 * This command is handled synchronously.
 */
QString CanvasGLStateDump::getGLStateDump(CanvasGLStateDump::stateDumpEnums options)
{
    if (m_canvasContext->isContextLost())
        return QString();

    m_options = options;
    m_stateDumpStr.clear();

    // Schedule a synchronous dump job
    GlSyncCommand syncCommand(CanvasGlCommandQueue::extStateDump);
    syncCommand.returnValue = static_cast<void *>(this);
    m_canvasContext->scheduleSyncCommand(&syncCommand);

    return m_stateDumpStr;
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
