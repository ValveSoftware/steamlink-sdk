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

#ifndef CANVASRENDERER_P_H
#define CANVASRENDERER_P_H

#include "canvas3dcommon_p.h"
#include "contextattributes_p.h"
#include "glcommandqueue_p.h"
#include "glstatestore_p.h"
#include "canvas3d_p.h"

#include <QtCore/QElapsedTimer>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLFramebufferObject>
#include <QtQuick/QQuickItem>
#include <QtCore/QMutex>

QT_BEGIN_NAMESPACE

class QOffscreenSurface;
class QQuickWindow;
class QOpenGLShaderProgram;
class QOpenGLShader;

QT_CANVAS3D_BEGIN_NAMESPACE

class QT_CANVAS3D_EXPORT CanvasRenderer : public QObject, QOpenGLFunctions
{
    Q_OBJECT
    Q_DISABLE_COPY(CanvasRenderer)

public:
    CanvasRenderer(QObject *parent = 0);
    ~CanvasRenderer();

    void resolveQtContext(QQuickWindow *window, const QSize &initializedSize,
                          Canvas::RenderTarget renderTarget);
    void createContextShare();
    void getQtContextAttributes(CanvasContextAttributes &contextAttributes);
    void init(QQuickWindow *window, const CanvasContextAttributes &contextAttributes,
              GLint &maxVertexAttribs, QSize &maxSize, int &contextVersion,
              QSet<QByteArray> &extensions, bool &isCombinedDepthStencilSupported);
    bool createContext(QQuickWindow *window, const CanvasContextAttributes &contextAttributes,
                       GLint &maxVertexAttribs, QSize &maxSize, int &contextVersion,
                       QSet<QByteArray> &extensions, bool &isCombinedDepthStencilSupported);

    void createFBOs();
    void bindCurrentRenderTarget();
    void setFboSize(const QSize &fboSize);
    QSize fboSize() const { return m_fboSize; }

    uint fps() const { return m_fps; }
    CanvasGlCommandQueue *commandQueue() { return &m_commandQueue; }

    void transferCommands();
    void makeCanvasContextCurrent();
    void executeCommandQueue();
    void executeSyncCommand(GlSyncCommand &command);
    void finalizeTexture();
    void restoreCanvasOpenGLState();
    void resetQtOpenGLState();

    int maxSamples() const { return m_maxSamples; }
    bool isOpenGLES2() const { return m_isOpenGLES2; }
    bool contextCreated() const { return (m_glContext != 0); }
    bool qtContextResolved() const { return (m_glContextQt != 0); }
    bool usingQtContext() const { return m_renderTarget != Canvas::RenderTargetOffscreenBuffer; }

    void resolveMSAAFbo();
    void deleteCommandData();

    qint64 previousFrameTime();
    void destroy();

public slots:
    void shutDown();
    void render();
    void clearBackground();

signals:
    void fpsChanged(uint fps);
    void textureReady(int id, const QSize &size);
    void textureIdResolved(QQuickItem *item);

private:
    bool updateGlError(const char *funcName);
    void multiplyAlpha();

    QSize m_fboSize;
    QSize m_initializedSize;

    QOpenGLContext *m_glContext;
    QOpenGLContext *m_glContextQt;
    QOpenGLContext *m_glContextShare;
    QQuickWindow *m_contextWindow;
    Canvas::RenderTarget m_renderTarget;
    GLStateStore *m_stateStore;

    uint m_fps;
    int m_maxSamples;

    bool m_isOpenGLES2;
    bool m_antialias;
    bool m_preserveDrawingBuffer;
    bool m_multiplyAlpha;

    QOpenGLShaderProgram *m_alphaMultiplierProgram;
    QOpenGLShader *m_alphaMultiplierVertexShader;
    QOpenGLShader *m_alphaMultiplierFragmentShader;
    GLuint m_alphaMultiplierVertexBuffer;
    GLuint m_alphaMultiplierUVBuffer;
    GLint m_alphaMultiplierVertexAttribute;
    GLint m_alphaMultiplierUVAttribute;

    QOpenGLFramebufferObject *m_antialiasFbo;
    QOpenGLFramebufferObject *m_renderFbo;
    QOpenGLFramebufferObject *m_displayFbo;
    QOpenGLFramebufferObject *m_alphaMultiplierFbo;
    QOpenGLFramebufferObjectFormat m_fboFormat;
    QOpenGLFramebufferObjectFormat m_antialiasFboFormat;
    bool m_recreateFbos;
    bool m_verifyFboBinds;

    QOffscreenSurface *m_offscreenSurface;

    CanvasGlCommandQueue m_commandQueue;
    QVector<GlCommand> m_executeQueue;
    int m_executeQueueCount;
    int m_executeStartIndex;
    int m_executeEndIndex;
    GLuint m_currentFramebufferId;
    QRect m_forceViewportRect;

    int m_glError;
    QElapsedTimer m_fpsTimer;
    QElapsedTimer m_frameTimer;
    qint64 m_frameTimeMs;

    int m_fpsFrames;
    bool m_textureFinalized;

    GLbitfield m_clearMask;
    QMutex m_shutdownMutex;
};

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE

#endif // CANVASRENDERER_P_H

