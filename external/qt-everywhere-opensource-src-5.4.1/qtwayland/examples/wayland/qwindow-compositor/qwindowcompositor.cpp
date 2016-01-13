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

#include "qwindowcompositor.h"

#include <QMouseEvent>
#include <QKeyEvent>
#include <QTouchEvent>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QGuiApplication>
#include <QCursor>
#include <QPixmap>
#include <QLinkedList>
#include <QScreen>
#include <QPainter>

#include <QtCompositor/qwaylandinput.h>
#include <QtCompositor/qwaylandbufferref.h>
#include <QtCompositor/qwaylandsurfaceview.h>

QT_BEGIN_NAMESPACE

class BufferAttacher : public QWaylandBufferAttacher
{
public:
    BufferAttacher()
        : QWaylandBufferAttacher()
        , shmTex(0)
    {
    }

    ~BufferAttacher()
    {
        delete shmTex;
    }

    void attach(const QWaylandBufferRef &ref) Q_DECL_OVERRIDE
    {
        if (bufferRef) {
            if (bufferRef.isShm()) {
                delete shmTex;
                shmTex = 0;
            } else {
                bufferRef.destroyTexture();
            }
        }

        bufferRef = ref;

        if (bufferRef) {
            if (bufferRef.isShm()) {
                shmTex = new QOpenGLTexture(bufferRef.image(), QOpenGLTexture::DontGenerateMipMaps);
                texture = shmTex->textureId();
            } else {
                texture = bufferRef.createTexture();
            }
        }
    }

    QImage image() const
    {
        if (!bufferRef || !bufferRef.isShm())
            return QImage();
        return bufferRef.image();
    }

    QOpenGLTexture *shmTex;
    QWaylandBufferRef bufferRef;
    GLuint texture;
};

QWindowCompositor::QWindowCompositor(CompositorWindow *window)
    : QWaylandCompositor(window, 0, DefaultExtensions | SubSurfaceExtension)
    , m_window(window)
    , m_backgroundTexture(0)
    , m_textureBlitter(0)
    , m_renderScheduler(this)
    , m_draggingWindow(0)
    , m_dragKeyIsPressed(false)
    , m_cursorSurface(0)
    , m_cursorHotspotX(0)
    , m_cursorHotspotY(0)
    , m_modifiers(Qt::NoModifier)
{
    m_window->makeCurrent();

    m_textureBlitter = new TextureBlitter();
    m_backgroundImage = makeBackgroundImage(QLatin1String(":/background.jpg"));
    m_renderScheduler.setSingleShot(true);
    connect(&m_renderScheduler,SIGNAL(timeout()),this,SLOT(render()));

    QOpenGLFunctions *functions = m_window->context()->functions();
    functions->glGenFramebuffers(1, &m_surface_fbo);

    window->installEventFilter(this);

    setRetainedSelectionEnabled(true);

    setOutputGeometry(QRect(QPoint(0, 0), window->size()));
    setOutputRefreshRate(qRound(qGuiApp->primaryScreen()->refreshRate() * 1000.0));
    addDefaultShell();
}

QWindowCompositor::~QWindowCompositor()
{
    delete m_textureBlitter;
}


QImage QWindowCompositor::makeBackgroundImage(const QString &fileName)
{
    Q_ASSERT(m_window);

    int width = m_window->width();
    int height = m_window->height();
    QImage baseImage(fileName);
    QImage patternedBackground(width, height, baseImage.format());
    QPainter painter(&patternedBackground);

    QSize imageSize = baseImage.size();
    for (int y = 0; y < height; y += imageSize.height()) {
        for (int x = 0; x < width; x += imageSize.width()) {
            painter.drawImage(x, y, baseImage);
        }
    }

    return patternedBackground;
}

void QWindowCompositor::ensureKeyboardFocusSurface(QWaylandSurface *oldSurface)
{
    QWaylandSurface *kbdFocus = defaultInputDevice()->keyboardFocus();
    if (kbdFocus == oldSurface || !kbdFocus)
        defaultInputDevice()->setKeyboardFocus(m_surfaces.isEmpty() ? 0 : m_surfaces.last());
}

void QWindowCompositor::surfaceDestroyed()
{
    QWaylandSurface *surface = static_cast<QWaylandSurface *>(sender());
    m_surfaces.removeOne(surface);
    ensureKeyboardFocusSurface(surface);
    m_renderScheduler.start(0);
}

void QWindowCompositor::surfaceMapped()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    QPoint pos;
    if (!m_surfaces.contains(surface)) {
        if (surface->windowType() != QWaylandSurface::Popup) {
            uint px = 0;
            uint py = 0;
            if (!QCoreApplication::arguments().contains(QLatin1String("-stickytopleft"))) {
                px = 1 + (qrand() % (m_window->width() - surface->size().width() - 2));
                py = 1 + (qrand() % (m_window->height() - surface->size().height() - 2));
            }
            pos = QPoint(px, py);
            QWaylandSurfaceView *view = surface->views().first();
            view->setPos(pos);
        }
    } else {
        m_surfaces.removeOne(surface);
    }

    if (surface->windowType() == QWaylandSurface::Popup) {
        QWaylandSurfaceView *view = surface->views().first();
        view->setPos(surface->transientParent()->views().first()->pos() + surface->transientOffset());
    }

    m_surfaces.append(surface);
    defaultInputDevice()->setKeyboardFocus(surface);

    m_renderScheduler.start(0);
}

void QWindowCompositor::surfaceUnmapped()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    if (m_surfaces.removeOne(surface))
        m_surfaces.insert(0, surface);

    ensureKeyboardFocusSurface(surface);
    m_renderScheduler.start(0);
}

void QWindowCompositor::surfaceCommitted()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    surfaceCommitted(surface);
}

void QWindowCompositor::surfacePosChanged()
{
    m_renderScheduler.start(0);
}

void QWindowCompositor::surfaceCommitted(QWaylandSurface *surface)
{
    Q_UNUSED(surface)
    m_renderScheduler.start(0);
}

void QWindowCompositor::surfaceCreated(QWaylandSurface *surface)
{
    connect(surface, SIGNAL(surfaceDestroyed()), this, SLOT(surfaceDestroyed()));
    connect(surface, SIGNAL(mapped()), this, SLOT(surfaceMapped()));
    connect(surface, SIGNAL(unmapped()), this, SLOT(surfaceUnmapped()));
    connect(surface, SIGNAL(redraw()), this, SLOT(surfaceCommitted()));
    connect(surface, SIGNAL(extendedSurfaceReady()), this, SLOT(sendExpose()));
    m_renderScheduler.start(0);

    surface->setBufferAttacher(new BufferAttacher);
}

void QWindowCompositor::sendExpose()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    surface->sendOnScreenVisibilityChange(true);
}

void QWindowCompositor::updateCursor(bool hasBuffer)
{
    Q_UNUSED(hasBuffer)
    if (!m_cursorSurface)
        return;

    QImage image = static_cast<BufferAttacher *>(m_cursorSurface->bufferAttacher())->image();

    QCursor cursor(QPixmap::fromImage(image), m_cursorHotspotX, m_cursorHotspotY);
    static bool cursorIsSet = false;
    if (cursorIsSet) {
        QGuiApplication::changeOverrideCursor(cursor);
    } else {
        QGuiApplication::setOverrideCursor(cursor);
        cursorIsSet = true;
    }
}

QPointF QWindowCompositor::toView(QWaylandSurfaceView *view, const QPointF &pos) const
{
    return pos - view->pos();
}

void QWindowCompositor::setCursorSurface(QWaylandSurface *surface, int hotspotX, int hotspotY)
{
    if ((m_cursorSurface != surface) && surface)
        connect(surface, SIGNAL(configure(bool)), this, SLOT(updateCursor(bool)));

    m_cursorSurface = surface;
    m_cursorHotspotX = hotspotX;
    m_cursorHotspotY = hotspotY;
    if (m_cursorSurface && !m_cursorSurface->bufferAttacher())
        m_cursorSurface->setBufferAttacher(new BufferAttacher);
}

QWaylandSurfaceView *QWindowCompositor::viewAt(const QPointF &point, QPointF *local)
{
    for (int i = m_surfaces.size() - 1; i >= 0; --i) {
        QWaylandSurface *surface = m_surfaces.at(i);
        foreach (QWaylandSurfaceView *view, surface->views()) {
            QRectF geo(view->pos(), surface->size());
            if (geo.contains(point)) {
                if (local)
                    *local = toView(view, point);
                return view;
            }
        }
    }
    return 0;
}

void QWindowCompositor::render()
{
    m_window->makeCurrent();
    frameStarted();

    cleanupGraphicsResources();

    if (!m_backgroundTexture)
        m_backgroundTexture = new QOpenGLTexture(m_backgroundImage, QOpenGLTexture::DontGenerateMipMaps);

    m_textureBlitter->bind();
    // Draw the background image texture
    m_textureBlitter->drawTexture(m_backgroundTexture->textureId(),
                                  QRect(QPoint(0, 0), m_backgroundImage.size()),
                                  window()->size(),
                                  0, false, true);

    foreach (QWaylandSurface *surface, m_surfaces) {
        if (!surface->visible())
            continue;
        GLuint texture = static_cast<BufferAttacher *>(surface->bufferAttacher())->texture;
        foreach (QWaylandSurfaceView *view, surface->views()) {
            QRect geo(view->pos().toPoint(),surface->size());
            m_textureBlitter->drawTexture(texture,geo,m_window->size(),0,false,surface->isYInverted());
            foreach (QWaylandSurface *child, surface->subSurfaces()) {
                drawSubSurface(view->pos().toPoint(), child);
            }
        }
    }

    m_textureBlitter->release();
    sendFrameCallbacks(surfaces());

    // N.B. Never call glFinish() here as the busylooping with vsync 'feature' of the nvidia binary driver is not desirable.
    m_window->swapBuffers();
}

void QWindowCompositor::drawSubSurface(const QPoint &offset, QWaylandSurface *surface)
{
    GLuint texture = static_cast<BufferAttacher *>(surface->bufferAttacher())->texture;
    QWaylandSurfaceView *view = surface->views().first();
    QPoint pos = view->pos().toPoint() + offset;
    QRect geo(pos, surface->size());
    m_textureBlitter->drawTexture(texture, geo, m_window->size(), 0, false, surface->isYInverted());
    foreach (QWaylandSurface *child, surface->subSurfaces()) {
        drawSubSurface(pos, child);
    }
}

bool QWindowCompositor::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != m_window)
        return false;

    QWaylandInputDevice *input = defaultInputDevice();

    switch (event->type()) {
    case QEvent::Expose:
        m_renderScheduler.start(0);
        if (m_window->isExposed()) {
            // Alt-tabbing away normally results in the alt remaining in
            // pressed state in the clients xkb state. Prevent this by sending
            // a release. This is not an issue in a "real" compositor but
            // is very annoying when running in a regular window on xcb.
            Qt::KeyboardModifiers mods = QGuiApplication::queryKeyboardModifiers();
            if (m_modifiers != mods && input->keyboardFocus()) {
                Qt::KeyboardModifiers stuckMods = m_modifiers ^ mods;
                if (stuckMods & Qt::AltModifier)
                    input->sendKeyReleaseEvent(64); // native scancode for left alt
                m_modifiers = mods;
            }
        }
        break;
    case QEvent::MouseButtonPress: {
        QPointF local;
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        QWaylandSurfaceView *target = viewAt(me->localPos(), &local);
        if (m_dragKeyIsPressed && target) {
            m_draggingWindow = target;
            m_drag_diff = local;
        } else {
            if (target && input->keyboardFocus() != target->surface()) {
                input->setKeyboardFocus(target->surface());
                m_surfaces.removeOne(target->surface());
                m_surfaces.append(target->surface());
                m_renderScheduler.start(0);
            }
            input->sendMousePressEvent(me->button(), local, me->localPos());
        }
        return true;
    }
    case QEvent::MouseButtonRelease: {
        QWaylandSurfaceView *target = input->mouseFocus();
        if (m_draggingWindow) {
            m_draggingWindow = 0;
            m_drag_diff = QPointF();
        } else {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            QPointF localPos;
            if (target)
                localPos = toView(target, me->localPos());
            input->sendMouseReleaseEvent(me->button(), localPos, me->localPos());
        }
        return true;
    }
    case QEvent::MouseMove: {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (m_draggingWindow) {
            m_draggingWindow->setPos(me->localPos() - m_drag_diff);
            m_renderScheduler.start(0);
        } else {
            QPointF local;
            QWaylandSurfaceView *target = viewAt(me->localPos(), &local);
            input->sendMouseMoveEvent(target, local, me->localPos());
        }
        break;
    }
    case QEvent::Wheel: {
        QWheelEvent *we = static_cast<QWheelEvent *>(event);
        input->sendMouseWheelEvent(we->orientation(), we->delta());
        break;
    }
    case QEvent::KeyPress: {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Meta || ke->key() == Qt::Key_Super_L) {
            m_dragKeyIsPressed = true;
        }
        m_modifiers = ke->modifiers();
        QWaylandSurface *targetSurface = input->keyboardFocus();
        if (targetSurface)
            input->sendKeyPressEvent(ke->nativeScanCode());
        break;
    }
    case QEvent::KeyRelease: {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Meta || ke->key() == Qt::Key_Super_L) {
            m_dragKeyIsPressed = false;
        }
        m_modifiers = ke->modifiers();
        QWaylandSurface *targetSurface = input->keyboardFocus();
        if (targetSurface)
            input->sendKeyReleaseEvent(ke->nativeScanCode());
        break;
    }
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    {
        QWaylandSurfaceView *target = 0;
        QTouchEvent *te = static_cast<QTouchEvent *>(event);
        QList<QTouchEvent::TouchPoint> points = te->touchPoints();
        QPoint pointPos;
        if (!points.isEmpty()) {
            pointPos = points.at(0).pos().toPoint();
            target = viewAt(pointPos);
        }
        if (target && target != input->mouseFocus())
            input->setMouseFocus(target, pointPos, pointPos);
        if (input->mouseFocus())
            input->sendFullTouchEvent(te);
        break;
    }
    default:
        break;
    }
    return false;
}

QT_END_NAMESPACE
