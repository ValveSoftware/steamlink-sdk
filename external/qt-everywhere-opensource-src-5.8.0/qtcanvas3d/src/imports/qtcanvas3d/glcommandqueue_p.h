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

#ifndef GLCOMMANDQUEUE_P_H
#define GLCOMMANDQUEUE_P_H

#include "canvas3dcommon_p.h"
#include "canvastextureprovider_p.h"

#include <QtCore/QVariantList>
#include <QtCore/QMutex>
#include <QtCore/QPointer>
#include <QtGui/qopengl.h>
#include <QtGui/QOpenGLShader>
#include <QtGui/QOpenGLShaderProgram>
#include <QtQuick/QQuickItem>
#include <QtQuick/QSGTextureProvider>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

class GlCommand;

class CanvasGlCommandQueue : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(CanvasGlCommandQueue)

public:
    Q_ENUMS(GlCommandId)

    enum GlCommandId {
        internalNoCommand = 0,
        glActiveTexture,
        glAttachShader,
        glBindAttribLocation,
        glBindBuffer,
        glBindFramebuffer,
        glBindRenderbuffer,
        glBindTexture,
        glBlendColor,
        glBlendEquation,
        glBlendEquationSeparate,
        glBlendFunc,
        glBlendFuncSeparate,
        glBufferData,
        glBufferSubData,
        glCheckFramebufferStatus,
        glClear,
        glClearColor,
        glClearDepthf,
        glClearStencil,
        glColorMask,
        glCompileShader,
        glCompressedTexImage2D,
        glCompressedTexSubImage2D,
        glCopyTexImage2D,
        glCopyTexSubImage2D,
        glCreateProgram,
        glCreateShader,
        glCullFace,
        glDeleteBuffers,
        glDeleteFramebuffers,
        glDeleteProgram,
        glDeleteRenderbuffers,
        glDeleteShader,
        glDeleteTextures,
        glDepthFunc,
        glDepthMask,
        glDepthRangef,
        glDetachShader,
        glDisable,
        glDisableVertexAttribArray,
        glDrawArrays,
        glDrawElements,
        glEnable,
        glEnableVertexAttribArray,
        glFlush,
        glFinish,
        glFramebufferRenderbuffer,
        glFramebufferTexture2D,
        glFrontFace,
        glGenBuffers,
        glGenerateMipmap,
        glGenFramebuffers,
        glGenRenderbuffers,
        glGenTextures,
        glGetActiveAttrib,
        glGetActiveUniform,
        glGetAttribLocation,
        glGetBooleanv,
        glGetBufferParameteriv,
        glGetError,
        glGetFloatv,
        glGetFramebufferAttachmentParameteriv,
        glGetIntegerv,
        glGetProgramInfoLog,
        glGetProgramiv,
        glGetRenderbufferParameteriv,
        glGetShaderInfoLog,
        glGetShaderiv,
        glGetShaderPrecisionFormat,
        glGetString,
        glGetTexParameteriv,
        glGetUniformfv,
        glGetUniformiv,
        glGetUniformLocation,
        glGetVertexAttribPointerv,
        glGetVertexAttribfv,
        glGetVertexAttribiv,
        glHint,
        glIsBuffer,
        glIsEnabled,
        glIsFramebuffer,
        glIsProgram,
        glIsRenderbuffer,
        glIsShader,
        glIsTexture,
        glLineWidth,
        glLinkProgram,
        glPixelStorei,
        glPolygonOffset,
        glReadPixels,
        glRenderbufferStorage,
        glSampleCoverage,
        glScissor,
        glStencilFunc,
        glStencilFuncSeparate,
        glStencilMask,
        glStencilMaskSeparate,
        glStencilOp,
        glStencilOpSeparate,
        glTexImage2D,
        glTexParameterf,
        glTexParameteri,
        glTexSubImage2D,
        glUniform1f,
        glUniform2f,
        glUniform3f,
        glUniform4f,
        glUniform1i,
        glUniform2i,
        glUniform3i,
        glUniform4i,
        glUniform1fv,
        glUniform2fv,
        glUniform3fv,
        glUniform4fv,
        glUniform1iv,
        glUniform2iv,
        glUniform3iv,
        glUniform4iv,
        glUniformMatrix2fv,
        glUniformMatrix3fv,
        glUniformMatrix4fv,
        glUseProgram,
        glValidateProgram,
        glVertexAttrib1f,
        glVertexAttrib2f,
        glVertexAttrib3f,
        glVertexAttrib4f,
        glVertexAttrib1fv,
        glVertexAttrib2fv,
        glVertexAttrib3fv,
        glVertexAttrib4fv,
        glVertexAttribPointer,
        glViewport,
        internalGetUniformType,
        internalClearLocation, // Used to clear a mapped uniform location from map when no longer needed
        internalBeginPaint, // Indicates the beginning of the paintGL(). Needed when rendering to Qt context.
        internalTextureComplete, // Indicates texture is complete and needs to be updated to screen at this point
        internalClearQuickItemAsTexture, // Used to clear mapped quick item texture ids when no longer needed
        extStateDump
    };

    class GlResource
    {
    public:
        GlResource() : glId(0), commandId(internalNoCommand) {}
        GlResource(GLuint id, GlCommandId command)
            : glId(id), commandId(command) {}
        GLuint glId;
        GlCommandId commandId; // Command that allocated this resource

    };

    CanvasGlCommandQueue(int initialSize, int maxSize, QObject *parent = 0);
    ~CanvasGlCommandQueue();

    int queuedCount() const { return m_queuedCount; }

    GlCommand &queueCommand(GlCommandId id);
    GlCommand &queueCommand(GlCommandId id,
                            GLint p1, GLint p2 = 0, GLint p3 = 0, GLint p4 = 0,
                            GLint p5 = 0, GLint p6 = 0, GLint p7 = 0, GLint p8 = 0);
    GlCommand &queueCommand(GlCommandId id,
                            GLfloat p1, GLfloat p2 = 0.0f, GLfloat p3 = 0.0f, GLfloat p4 = 0.0f);
    GlCommand &queueCommand(GlCommandId id,
                            GLint i1, GLfloat p1, GLfloat p2 = 0.0f, GLfloat p3 = 0.0f,
                            GLfloat p4 = 0.0f);
    GlCommand &queueCommand(GlCommandId id,
                            GLint i1, GLint i2, GLfloat p1, GLfloat p2 = 0.0f, GLfloat p3 = 0.0f,
                            GLfloat p4 = 0.0f);

    void transferCommands(QVector<GlCommand> &executeQueue);
    void resetQueue(int size);
    void deleteUntransferedCommandData();

    // Id map manipulation
    GLint createResourceId();
    void setGlIdToMap(GLint id, GLuint glId, GlCommandId commandId);
    void removeResourceIdFromMap(GLint id);
    GLuint getGlId(GLint id);
    GLint getCanvasId(GLuint glId, GlCommandId type);

    void setShaderToMap(GLint id, QOpenGLShader *shader);
    void setProgramToMap(GLint id, QOpenGLShaderProgram *program);
    QOpenGLShader *takeShaderFromMap(GLint id);
    QOpenGLShaderProgram *takeProgramFromMap(GLint id);
    QOpenGLShader *getShader(GLint id);
    QOpenGLShaderProgram *getProgram(GLint id);

    void clearResourceMaps();

    GLuint takeSingleIdParam(const GlCommand &command);
    void handleGenerateCommand(const GlCommand &command, GLuint glId);

    QMutex *resourceMutex() { return &m_resourceMutex; }

    void addQuickItemAsTexture(QQuickItem *quickItem, GLint textureId);
    void clearQuickItemAsTextureList();

    void removeFromClearMask(GLbitfield mask);
    GLbitfield resetClearMask();

    struct ProviderCacheItem {
        ProviderCacheItem(QSGTextureProvider *provider, QQuickItem *item) :
            providerPtr(provider),
            quickItem(item) {}

        QPointer<QSGTextureProvider> providerPtr;
        QQuickItem *quickItem; // Not owned, nor accessed - only used as identifier
    };
    QMap<GLint, ProviderCacheItem *> &providerCache() { return m_providerCache; }

signals:
    void queueFull();

private:
    QVector<GlCommand> m_queue;
    int m_maxSize;
    int m_size;
    int m_queuedCount;

    QMap<GLint, GlResource> m_resourceIdMap;
    QMap<GLint, QOpenGLShader *> m_shaderMap;
    QMap<GLint, QOpenGLShaderProgram *> m_programMap;
    GLint m_nextResourceId;
    bool m_resourceIdOverflow;
    QMutex m_resourceMutex;

    struct ItemAndId {
        ItemAndId(QQuickItem *item, GLint itemId) :
            itemPtr(item),
            id(itemId) {}

        QPointer<QQuickItem> itemPtr;
        GLint id;
    };
    QList<ItemAndId *> m_quickItemsAsTextureList;

    QMap<GLint, ProviderCacheItem *> m_providerCache;
    GLbitfield m_clearMask;
};

inline bool operator==(CanvasGlCommandQueue::GlResource lhs, CanvasGlCommandQueue::GlResource rhs) Q_DECL_NOTHROW
{
    return lhs.glId == rhs.glId && lhs.commandId == rhs.commandId;
}
inline bool operator!=(CanvasGlCommandQueue::GlResource lhs, CanvasGlCommandQueue::GlResource rhs) Q_DECL_NOTHROW
{
    return !operator==(lhs, rhs);
}

class GlCommand
{
public:
    GlCommand()
        : data(0),
          id(CanvasGlCommandQueue::internalNoCommand),
          i1(0), i2(0), i3(0), i4(0), i5(0), i6(0), i7(0), i8(0)
    {}
    GlCommand(CanvasGlCommandQueue::GlCommandId command)
        : data(0),
          id(command),
          i1(0), i2(0), i3(0), i4(0), i5(0), i6(0), i7(0), i8(0)
    {}
    GlCommand(CanvasGlCommandQueue::GlCommandId command,
              GLint p1, GLint p2 = 0, GLint p3 = 0, GLint p4 = 0, GLint p5 = 0, GLint p6 = 0,
              GLint p7 = 0, GLint p8 = 0)
        : data(0),
          id(command),
          i1(p1), i2(p2), i3(p3), i4(p4), i5(p5), i6(p6), i7(p7), i8(p8)
    {}
    GlCommand(CanvasGlCommandQueue::GlCommandId command,
              GLfloat p1, GLfloat p2 = 0.0f, GLfloat p3 = 0.0f, GLfloat p4 = 0.0f)
        : data(0),
          id(command),
          i1(0), i2(0), i3(0), i4(0),
          f1(p1), f2(p2), f3(p3), f4(p4)
    {}
    ~GlCommand() {}

    void deleteData() {
        delete data;
        data = 0;
    }

    // Optional extra data such as buffer contents for commands that need it. The data is deleted
    // after the command is handled. Not owned by command itself.
    // Queue owns the data before it is transferred to execution, after which the CanvasRenderer
    // owns it. In case of unqueued commands (i.e. synchronous commands), the data is owned by
    // whoever owns the command object.
    QByteArray *data;

    CanvasGlCommandQueue::GlCommandId id;

    GLint i1;
    GLint i2;
    GLint i3;
    GLint i4;

    // Commands that use both ints and floats are limited to four ints.
    // In practice maximum of two ints are used by such commands.
    union {
        GLint i5;
        GLfloat f1;
    };
    union {
        GLint i6;
        GLfloat f2;
    };
    union {
        GLint i7;
        GLfloat f3;
    };
    union {
        GLint i8;
        GLfloat f4;
    };
};

class GlSyncCommand : public GlCommand
{
public:
    GlSyncCommand()
        : GlCommand(CanvasGlCommandQueue::internalNoCommand),
          returnValue(0),
          glError(false)
    {}
    GlSyncCommand(CanvasGlCommandQueue::GlCommandId command)
        : GlCommand(command),
          returnValue(0),
          glError(false)
    {}
    GlSyncCommand(CanvasGlCommandQueue::GlCommandId command,
              GLint p1, GLint p2 = 0, GLint p3 = 0, GLint p4 = 0, GLint p5 = 0, GLint p6 = 0,
              GLint p7 = 0, GLint p8 = 0)
        : GlCommand(command, p1, p2, p3, p4, p5, p6, p7, p8),
          returnValue(0),
          glError(false)
    {}
    GlSyncCommand(CanvasGlCommandQueue::GlCommandId command,
              GLfloat p1, GLfloat p2 = 0.0f, GLfloat p3 = 0.0f, GLfloat p4 = 0.0f)
        : GlCommand(command, p1, p2, p3, p4),
          returnValue(0),
          glError(false)
    {}
    ~GlSyncCommand() {}

    // Optional return value for synchronous commands. Not owned.
    void *returnValue;

    // Indicates if an OpenGL error occurred during command execution.
    bool glError;
};

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE

#endif // GLCOMMANDQUEUE_P_H
