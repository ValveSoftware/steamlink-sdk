/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Compositor.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QWINDOWCOMPOSITOR_H
#define QWINDOWCOMPOSITOR_H

#include "qwaylandcompositor.h"
#include "qwaylandsurface.h"
#include "textureblitter.h"
#include "compositorwindow.h"

#include <QtGui/private/qopengltexturecache_p.h>
#include <QObject>
#include <QTimer>

QT_BEGIN_NAMESPACE

class QWaylandSurfaceView;
class QOpenGLTexture;

class QWindowCompositor : public QObject, public QWaylandCompositor
{
    Q_OBJECT
public:
    QWindowCompositor(CompositorWindow *window);
    ~QWindowCompositor();

private slots:
    void surfaceDestroyed();
    void surfaceMapped();
    void surfaceUnmapped();
    void surfaceCommitted();
    void surfacePosChanged();

    void render();
protected:
    void surfaceCommitted(QWaylandSurface *surface);
    void surfaceCreated(QWaylandSurface *surface);

    QWaylandSurfaceView* viewAt(const QPointF &point, QPointF *local = 0);

    bool eventFilter(QObject *obj, QEvent *event);
    QPointF toView(QWaylandSurfaceView *view, const QPointF &pos) const;

    void setCursorSurface(QWaylandSurface *surface, int hotspotX, int hotspotY);

    void ensureKeyboardFocusSurface(QWaylandSurface *oldSurface);
    QImage makeBackgroundImage(const QString &fileName);

private slots:
    void sendExpose();
    void updateCursor(bool hasBuffer);

private:
    void drawSubSurface(const QPoint &offset, QWaylandSurface *surface);

    CompositorWindow *m_window;
    QImage m_backgroundImage;
    QOpenGLTexture *m_backgroundTexture;
    QList<QWaylandSurface *> m_surfaces;
    TextureBlitter *m_textureBlitter;
    GLuint m_surface_fbo;
    QTimer m_renderScheduler;

    //Dragging windows around
    QWaylandSurfaceView *m_draggingWindow;
    bool m_dragKeyIsPressed;
    QPointF m_drag_diff;

    //Cursor
    QWaylandSurface *m_cursorSurface;
    int m_cursorHotspotX;
    int m_cursorHotspotY;

    Qt::KeyboardModifiers m_modifiers;
};

QT_END_NAMESPACE

#endif // QWINDOWCOMPOSITOR_H
