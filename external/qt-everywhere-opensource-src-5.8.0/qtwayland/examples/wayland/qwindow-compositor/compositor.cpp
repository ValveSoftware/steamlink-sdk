/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Wayland module
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
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
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

#include "compositor.h"

#include <QMouseEvent>
#include <QKeyEvent>
#include <QTouchEvent>

#include <QtWaylandCompositor/QWaylandXdgShellV5>
#include <QtWaylandCompositor/QWaylandWlShellSurface>
#include <QtWaylandCompositor/qwaylandseat.h>
#include <QtWaylandCompositor/qwaylanddrag.h>

#include <QDebug>
#include <QOpenGLContext>

#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif

View::View()
    : m_textureTarget(GL_TEXTURE_2D)
    , m_texture(0)
    , m_wlShellSurface(nullptr)
    , m_xdgSurface(nullptr)
    , m_xdgPopup(nullptr)
    , m_parentView(nullptr)
{}

QOpenGLTexture *View::getTexture()
{
    if (advance()) {
        QWaylandBufferRef buf = currentBuffer();
        m_texture = buf.toOpenGLTexture();
    }

    return m_texture;
}

bool View::isCursor() const
{
    return surface()->isCursorSurface();
}

void View::onXdgSetMaximized()
{
    m_xdgSurface->sendMaximized(output()->geometry().size());

    // An improvement here, would have been to wait for the commit after the ack_configure for the
    // request above before moving the window. This would have prevented the window from being
    // moved until the contents of the window had actually updated. This improvement is left as an
    // exercise for the reader.
    setPosition(QPoint(0, 0));
}

void View::onXdgUnsetMaximized()
{
    m_xdgSurface->sendUnmaximized();
}

void View::onXdgSetFullscreen(QWaylandOutput* clientPreferredOutput)
{
    QWaylandOutput *outputToFullscreen = clientPreferredOutput
            ? clientPreferredOutput
            : output();

    m_xdgSurface->sendFullscreen(outputToFullscreen->geometry().size());

    // An improvement here, would have been to wait for the commit after the ack_configure for the
    // request above before moving the window. This would have prevented the window from being
    // moved until the contents of the window had actually updated. This improvement is left as an
    // exercise for the reader.
    setPosition(outputToFullscreen->position());
}

void View::onOffsetForNextFrame(const QPoint &offset)
{
    m_offset = offset;
    setPosition(position() + offset);
}

void View::onXdgUnsetFullscreen()
{
    onXdgUnsetMaximized();
}

Compositor::Compositor(QWindow *window)
    : QWaylandCompositor()
    , m_window(window)
    , m_wlShell(new QWaylandWlShell(this))
    , m_xdgShell(new QWaylandXdgShellV5(this))
{
    connect(m_wlShell, &QWaylandWlShell::wlShellSurfaceCreated, this, &Compositor::onWlShellSurfaceCreated);
    connect(m_xdgShell, &QWaylandXdgShellV5::xdgSurfaceCreated, this, &Compositor::onXdgSurfaceCreated);
    connect(m_xdgShell, &QWaylandXdgShellV5::xdgPopupRequested, this, &Compositor::onXdgPopupRequested);
}

Compositor::~Compositor()
{
}

void Compositor::create()
{
    QWaylandOutput *output = new QWaylandOutput(this, m_window);
    QWaylandOutputMode mode(QSize(800, 600), 60000);
    output->addMode(mode, true);
    QWaylandCompositor::create();
    output->setCurrentMode(mode);

    connect(this, &QWaylandCompositor::surfaceCreated, this, &Compositor::onSurfaceCreated);
    connect(defaultSeat(), &QWaylandSeat::cursorSurfaceRequest, this, &Compositor::adjustCursorSurface);
    connect(defaultSeat()->drag(), &QWaylandDrag::dragStarted, this, &Compositor::startDrag);

    connect(this, &QWaylandCompositor::subsurfaceChanged, this, &Compositor::onSubsurfaceChanged);
}

void Compositor::onSurfaceCreated(QWaylandSurface *surface)
{
    connect(surface, &QWaylandSurface::surfaceDestroyed, this, &Compositor::surfaceDestroyed);
    connect(surface, &QWaylandSurface::hasContentChanged, this, &Compositor::surfaceHasContentChanged);
    connect(surface, &QWaylandSurface::redraw, this, &Compositor::triggerRender);

    connect(surface, &QWaylandSurface::subsurfacePositionChanged, this, &Compositor::onSubsurfacePositionChanged);

    View *view = new View;
    view->setSurface(surface);
    view->setOutput(outputFor(m_window));
    m_views << view;
    connect(view, &QWaylandView::surfaceDestroyed, this, &Compositor::viewSurfaceDestroyed);
    connect(surface, &QWaylandSurface::offsetForNextFrame, view, &View::onOffsetForNextFrame);
}

void Compositor::surfaceHasContentChanged()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    if (surface->hasContent()) {
        if (surface->role() == QWaylandWlShellSurface::role()
                || surface->role() == QWaylandXdgSurfaceV5::role()
                || surface->role() == QWaylandXdgPopupV5::role()) {
            defaultSeat()->setKeyboardFocus(surface);
        }
    }
    triggerRender();
}

void Compositor::surfaceDestroyed()
{
    triggerRender();
}

void Compositor::viewSurfaceDestroyed()
{
    View *view = qobject_cast<View*>(sender());
    m_views.removeAll(view);
    delete view;
}

View * Compositor::findView(const QWaylandSurface *s) const
{
    Q_FOREACH (View* view, m_views) {
        if (view->surface() == s)
            return view;
    }
    return Q_NULLPTR;
}

void Compositor::onWlShellSurfaceCreated(QWaylandWlShellSurface *wlShellSurface)
{
    connect(wlShellSurface, &QWaylandWlShellSurface::startMove, this, &Compositor::onStartMove);
    connect(wlShellSurface, &QWaylandWlShellSurface::startResize, this, &Compositor::onWlStartResize);
    connect(wlShellSurface, &QWaylandWlShellSurface::setTransient, this, &Compositor::onSetTransient);
    connect(wlShellSurface, &QWaylandWlShellSurface::setPopup, this, &Compositor::onSetPopup);

    View *view = findView(wlShellSurface->surface());
    Q_ASSERT(view);
    view->m_wlShellSurface = wlShellSurface;
}

void Compositor::onXdgSurfaceCreated(QWaylandXdgSurfaceV5 *xdgSurface)
{
    connect(xdgSurface, &QWaylandXdgSurfaceV5::startMove, this, &Compositor::onStartMove);
    connect(xdgSurface, &QWaylandXdgSurfaceV5::startResize, this, &Compositor::onXdgStartResize);

    View *view = findView(xdgSurface->surface());
    Q_ASSERT(view);
    view->m_xdgSurface = xdgSurface;

    connect(xdgSurface, &QWaylandXdgSurfaceV5::setMaximized, view, &View::onXdgSetMaximized);
    connect(xdgSurface, &QWaylandXdgSurfaceV5::setFullscreen, view, &View::onXdgSetFullscreen);
    connect(xdgSurface, &QWaylandXdgSurfaceV5::unsetMaximized, view, &View::onXdgUnsetMaximized);
    connect(xdgSurface, &QWaylandXdgSurfaceV5::unsetFullscreen, view, &View::onXdgUnsetFullscreen);
}

void Compositor::onXdgPopupRequested(QWaylandSurface *surface, QWaylandSurface *parent,
                                     QWaylandSeat *seat, const QPoint &position,
                                     const QWaylandResource &resource)
{
    Q_UNUSED(seat);

    QWaylandXdgPopupV5 *xdgPopup = new QWaylandXdgPopupV5(m_xdgShell, surface, parent, position, resource);

    View *view = findView(surface);
    Q_ASSERT(view);

    View *parentView = findView(parent);
    Q_ASSERT(parentView);

    view->setPosition(parentView->position() + position);
    view->m_xdgPopup = xdgPopup;
}

void Compositor::onStartMove()
{
    closePopups();
    emit startMove();
}

void Compositor::onWlStartResize(QWaylandSeat *, QWaylandWlShellSurface::ResizeEdge edges)
{
    closePopups();
    emit startResize(int(edges), false);
}

void Compositor::onXdgStartResize(QWaylandSeat *seat,
                                  QWaylandXdgSurfaceV5::ResizeEdge edges)
{
    Q_UNUSED(seat);
    emit startResize(int(edges), true);
}

void Compositor::onSetTransient(QWaylandSurface *parent, const QPoint &relativeToParent, bool inactive)
{
    Q_UNUSED(inactive);

    QWaylandWlShellSurface *wlShellSurface = qobject_cast<QWaylandWlShellSurface*>(sender());
    View *view = findView(wlShellSurface->surface());

    if (view) {
        raise(view);
        View *parentView = findView(parent);
        if (parentView)
            view->setPosition(parentView->position() + relativeToParent);
    }
}

void Compositor::onSetPopup(QWaylandSeat *seat, QWaylandSurface *parent, const QPoint &relativeToParent)
{
    Q_UNUSED(seat);
    QWaylandWlShellSurface *surface = qobject_cast<QWaylandWlShellSurface*>(sender());
    View *view = findView(surface->surface());
    if (view) {
        raise(view);
        View *parentView = findView(parent);
        if (parentView)
            view->setPosition(parentView->position() + relativeToParent);
    }
}

void Compositor::onSubsurfaceChanged(QWaylandSurface *child, QWaylandSurface *parent)
{
    View *view = findView(child);
    View *parentView = findView(parent);
    view->setParentView(parentView);
}

void Compositor::onSubsurfacePositionChanged(const QPoint &position)
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface*>(sender());
    if (!surface)
        return;
    View *view = findView(surface);
    view->setPosition(position);
    triggerRender();
}

void Compositor::triggerRender()
{
    m_window->requestUpdate();
}

void Compositor::startRender()
{
    QWaylandOutput *out = defaultOutput();
    if (out)
        out->frameStarted();
}

void Compositor::endRender()
{
    QWaylandOutput *out = defaultOutput();
    if (out)
        out->sendFrameCallbacks();
}

void Compositor::updateCursor()
{
    m_cursorView.advance();
    QImage image = m_cursorView.currentBuffer().image();
    if (!image.isNull())
        m_window->setCursor(QCursor(QPixmap::fromImage(image), m_cursorHotspotX, m_cursorHotspotY));
}

void Compositor::adjustCursorSurface(QWaylandSurface *surface, int hotspotX, int hotspotY)
{
    if ((m_cursorView.surface() != surface)) {
        if (m_cursorView.surface())
            disconnect(m_cursorView.surface(), &QWaylandSurface::redraw, this, &Compositor::updateCursor);
        if (surface)
            connect(surface, &QWaylandSurface::redraw, this, &Compositor::updateCursor);
    }

    m_cursorView.setSurface(surface);
    m_cursorHotspotX = hotspotX;
    m_cursorHotspotY = hotspotY;

    if (surface && surface->hasContent())
        updateCursor();
}

void Compositor::closePopups()
{
    m_wlShell->closeAllPopups();
    m_xdgShell->closeAllPopups();
}

void Compositor::handleMouseEvent(QWaylandView *target, QMouseEvent *me)
{
    auto popClient = popupClient();
    if (target && me->type() == QEvent::MouseButtonPress
            && popClient && popClient != target->surface()->client()) {
        closePopups();
    }

    QWaylandSeat *input = defaultSeat();
    QWaylandSurface *surface = target ? target->surface() : nullptr;
    switch (me->type()) {
        case QEvent::MouseButtonPress:
            input->sendMousePressEvent(me->button());
            if (surface != input->keyboardFocus()) {
                if (surface == nullptr
                        || surface->role() == QWaylandWlShellSurface::role()
                        || surface->role() == QWaylandXdgSurfaceV5::role()
                        || surface->role() == QWaylandXdgPopupV5::role()) {
                    input->setKeyboardFocus(surface);
                }
            }
            break;
    case QEvent::MouseButtonRelease:
         input->sendMouseReleaseEvent(me->button());
         break;
    case QEvent::MouseMove:
        input->sendMouseMoveEvent(target, me->localPos(), me->globalPos());
    default:
        break;
    }
}

void Compositor::handleResize(View *target, const QSize &initialSize, const QPoint &delta, int edge)
{
    QWaylandWlShellSurface *wlShellSurface = target->m_wlShellSurface;
    if (wlShellSurface) {
        QWaylandWlShellSurface::ResizeEdge edges = QWaylandWlShellSurface::ResizeEdge(edge);
        QSize newSize = wlShellSurface->sizeForResize(initialSize, delta, edges);
        wlShellSurface->sendConfigure(newSize, edges);
    }

    QWaylandXdgSurfaceV5 *xdgSurface = target->m_xdgSurface;
    if (xdgSurface) {
        QWaylandXdgSurfaceV5::ResizeEdge edges = static_cast<QWaylandXdgSurfaceV5::ResizeEdge>(edge);
        QSize newSize = xdgSurface->sizeForResize(initialSize, delta, edges);
        xdgSurface->sendResizing(newSize);
    }
}

void Compositor::startDrag()
{
    QWaylandDrag *currentDrag = defaultSeat()->drag();
    Q_ASSERT(currentDrag);
    View *iconView = findView(currentDrag->icon());
    iconView->setPosition(m_window->mapFromGlobal(QCursor::pos()));

    emit dragStarted(iconView);
}

void Compositor::handleDrag(View *target, QMouseEvent *me)
{
    QPointF pos = me->localPos();
    QWaylandSurface *surface = 0;
    if (target) {
        pos -= target->position();
        surface = target->surface();
    }
    QWaylandDrag *currentDrag = defaultSeat()->drag();
    currentDrag->dragMove(surface, pos);
    if (me->buttons() == Qt::NoButton) {
        m_views.removeOne(findView(currentDrag->icon()));
        currentDrag->drop();
    }
}

QWaylandClient *Compositor::popupClient() const
{
    auto client = m_wlShell->popupClient();
    return client ? client : m_xdgShell->popupClient();
}

// We only have a flat list of views, plus pointers from child to parent,
// so maintaining a stacking order gets a bit complex. A better data
// structure is left as an exercise for the reader.

static int findEndOfChildTree(const QList<View*> &list, int index)
{
    int n = list.count();
    View *parent = list.at(index);
    while (index + 1 < n) {
        if (list.at(index+1)->parentView() != parent)
            break;
        index = findEndOfChildTree(list, index + 1);
    }
    return index;
}

void Compositor::raise(View *view)
{
    int startPos = m_views.indexOf(view);
    int endPos = findEndOfChildTree(m_views, startPos);

    int n = m_views.count();
    int tail =  n - endPos - 1;

    //bubble sort: move the child tree to the end of the list
    for (int i = 0; i < tail; i++) {
        int source = endPos + 1 + i;
        int dest = startPos + i;
        for (int j = source; j > dest; j--)
            m_views.swap(j, j-1);
    }
}
