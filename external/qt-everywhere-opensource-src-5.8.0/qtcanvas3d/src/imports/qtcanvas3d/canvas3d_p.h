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

#ifndef CANVAS3D_P_H
#define CANVAS3D_P_H

#include "canvas3dcommon_p.h"
#include "context3d_p.h"

#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtGui/QOpenGLFramebufferObject>
#include <QtCore/QElapsedTimer>
#include <QtCore/QPointer>

QT_BEGIN_NAMESPACE

class QOffscreenSurface;

QT_CANVAS3D_BEGIN_NAMESPACE

class CanvasGlCommandQueue;
class CanvasRenderer;

// Debug: Logs on high level information about the OpenGL driver and context.
Q_DECLARE_LOGGING_CATEGORY(canvas3dinfo)

// Debug: Logs all the calls made in to Canvas3D and Context3D.
// Warning: Only logs warnings on failures in verifications.
Q_DECLARE_LOGGING_CATEGORY(canvas3drendering)

// Debug: Aggressive error checking. This means calling glGetError() after each OpenGL call.
// Since checking for errors is a synchronous call, this will cause a negative performance hit.
// Warning: All detected GL errors are logged as warnings on this category.
Q_DECLARE_LOGGING_CATEGORY(canvas3dglerrors)

class QT_CANVAS3D_EXPORT Canvas : public QQuickItem
{
    Q_OBJECT
    Q_DISABLE_COPY(Canvas)

    Q_ENUMS(RenderTarget)

    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(CanvasContext *context READ context NOTIFY contextChanged)
    Q_PROPERTY(float devicePixelRatio READ devicePixelRatio NOTIFY devicePixelRatioChanged)
    Q_PROPERTY(uint fps READ fps NOTIFY fpsChanged)
    Q_PROPERTY(QSize pixelSize READ pixelSize WRITE setPixelSize NOTIFY pixelSizeChanged)
    Q_PROPERTY(RenderTarget renderTarget READ renderTarget WRITE setRenderTarget NOTIFY renderTargetChanged REVISION 1)
    Q_PROPERTY(bool renderOnDemand READ renderOnDemand WRITE setRenderOnDemand NOTIFY renderOnDemandChanged REVISION 1)

public:
    enum RenderTarget {
        RenderTargetOffscreenBuffer,
        RenderTargetBackground,
        RenderTargetForeground
    };

    // internal
    enum ContextState {
        ContextNone,
        ContextLost,
        ContextRestoring,
        ContextAlive
    };

    Canvas(QQuickItem *parent = 0);
    ~Canvas();

    void handleWindowChanged(QQuickWindow *win);
    float devicePixelRatio();
    QSize pixelSize();
    void setPixelSize(QSize pixelSize);
    void setRenderTarget(RenderTarget target);
    RenderTarget renderTarget() const;
    void setRenderOnDemand(bool enable);
    bool renderOnDemand() const;

    uint fps();
    Q_INVOKABLE int frameTimeMs();
    Q_REVISION(1) Q_INVOKABLE int frameSetupTimeMs();

    Q_INVOKABLE QJSValue getContext(const QString &name);
    Q_INVOKABLE QJSValue getContext(const QString &name, const QVariantMap &options);
    CanvasContext *context();
    CanvasRenderer *renderer();

public slots:
    void requestRender();

private slots:
    void queueNextRender();
    void queueResizeGL();
    void emitNeedRender();
    void handleBeforeSynchronizing();
    void handleRendererFpsChange(uint fps);
    void handleContextLost();

signals:
    void needRender();
    void devicePixelRatioChanged(float ratio);
    void contextChanged(CanvasContext *context);
    void fpsChanged(uint fps);
    void pixelSizeChanged(QSize pixelSize);
    void renderTargetChanged();
    void renderOnDemandChanged();
    void contextLost();
    void contextRestored();

    void initializeGL();
    void paintGL();
    void resizeGL(int width, int height, float devicePixelRatio);

    void textureReady(int id, const QSize &size, float devicePixelRatio);

protected:
    virtual void geometryChanged(const QRectF & newGeometry, const QRectF & oldGeometry);
    virtual void itemChange(ItemChange change, const ItemChangeData &value);
    virtual QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *);

private:
    void setupAntialiasing();
    void updateWindowParameters();
    void sync();
    bool firstSync();

    bool m_isNeedRenderQueued;
    bool m_rendererReady;
    QPointer<CanvasContext> m_context3D;
    QSize m_fboSize;
    QSize m_maxSize;

    uint m_frameTimeMs;
    uint m_frameSetupTimeMs;
    QElapsedTimer m_frameTimer;
    int m_maxSamples;
    float m_devicePixelRatio;

    bool m_isOpenGLES2;
    bool m_isCombinedDepthStencilSupported;
    bool m_isSoftwareRendered;
    bool m_runningInDesigner;
    CanvasContextAttributes m_contextAttribs;
    bool m_isContextAttribsSet;
    bool m_alphaChanged;
    bool m_resizeGLQueued;
    bool m_allowRenderTargetChange;
    bool m_renderTargetSyncConnected;
    RenderTarget m_renderTarget;
    bool m_renderOnDemand;

    CanvasRenderer *m_renderer;

    GLint m_maxVertexAttribs;
    int m_contextVersion;
    QSet<QByteArray> m_extensions;

    uint m_fps;

    ContextState m_contextState;
    QPointer<QQuickWindow> m_contextWindow; // Not owned
};

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE

#endif // CANVAS3D_P_H

