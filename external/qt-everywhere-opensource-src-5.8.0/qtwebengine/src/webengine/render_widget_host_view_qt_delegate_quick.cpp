/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include "render_widget_host_view_qt_delegate_quick.h"

#include "qquickwebengineview_p.h"
#include "qquickwebengineview_p_p.h"
#include <QGuiApplication>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QVariant>
#include <QWindow>
#include <private/qquickwindow_p.h>
#include <private/qsgcontext_p.h>

#if (QT_VERSION < QT_VERSION_CHECK(5, 8, 0))
#include <QSGSimpleRectNode>
#include <QSGSimpleTextureNode>
#endif

namespace QtWebEngineCore {

RenderWidgetHostViewQtDelegateQuick::RenderWidgetHostViewQtDelegateQuick(RenderWidgetHostViewQtDelegateClient *client, bool isPopup)
    : m_client(client)
    , m_isPopup(isPopup)
    , m_initialized(false)
{
    setFlag(ItemHasContents);
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    if (isPopup)
        return;
    setFocus(true);
    setActiveFocusOnTab(true);

#ifdef Q_OS_OSX
    // Check that the default QSurfaceFormat OpenGL profile is compatible with the global OpenGL
    // shared context profile, otherwise this could lead to a nasty crash.
    QOpenGLContext *globalSharedContext = QOpenGLContext::globalShareContext();
    if (globalSharedContext) {
        QSurfaceFormat sharedFormat = globalSharedContext->format();
        QSurfaceFormat defaultFormat = QSurfaceFormat::defaultFormat();

        if (defaultFormat.profile() != sharedFormat.profile()
            && defaultFormat.profile() == QSurfaceFormat::CoreProfile
            && defaultFormat.version() >= qMakePair(3, 2)) {
            qFatal("QWebEngine: Default QSurfaceFormat OpenGL profile is not compatible with the "
                   "global shared context OpenGL profile. Please make sure you set a compatible "
                   "QSurfaceFormat before the QtGui application instance is created.");
        }
    }
#endif

}

void RenderWidgetHostViewQtDelegateQuick::initAsChild(WebContentsAdapterClient* container)
{
    QQuickWebEngineView *view = static_cast<QQuickWebEngineViewPrivate *>(container)->q_func();
    setParentItem(view);
    setSize(view->boundingRect().size());
    // Focus on creation if the view accepts it
    if (view->activeFocusOnPress())
        setFocus(true);
    m_initialized = true;

}

void RenderWidgetHostViewQtDelegateQuick::initAsPopup(const QRect &r)
{
    Q_ASSERT(m_isPopup && parentItem());
    QRectF rect(parentItem()->mapRectFromScene(r));
    setX(rect.x());
    setY(rect.y());
    setWidth(rect.width());
    setHeight(rect.height());
    setVisible(true);
    m_initialized = true;
}

QRectF RenderWidgetHostViewQtDelegateQuick::screenRect() const
{
    QPointF pos = mapToScene(QPointF(0,0));
    return QRectF(pos.x(), pos.y(), width(), height());
}

QRectF RenderWidgetHostViewQtDelegateQuick::contentsRect() const
{
    QPointF scenePoint = mapToScene(QPointF(0, 0));
    QPointF screenPos;
    if (window())
        screenPos = window()->mapToGlobal(scenePoint.toPoint());
    return QRectF(screenPos.x(), screenPos.y(), width(), height());
}

void RenderWidgetHostViewQtDelegateQuick::setKeyboardFocus()
{
    setFocus(true);
}

bool RenderWidgetHostViewQtDelegateQuick::hasKeyboardFocus()
{
    return hasActiveFocus();
}

void RenderWidgetHostViewQtDelegateQuick::lockMouse()
{
    grabMouse();
}

void RenderWidgetHostViewQtDelegateQuick::unlockMouse()
{
    ungrabMouse();
}

void RenderWidgetHostViewQtDelegateQuick::show()
{
    setVisible(true);
    m_client->notifyShown();
}

void RenderWidgetHostViewQtDelegateQuick::hide()
{
    setVisible(false);
    m_client->notifyHidden();
}

bool RenderWidgetHostViewQtDelegateQuick::isVisible() const
{
    return QQuickItem::isVisible();
}

QWindow* RenderWidgetHostViewQtDelegateQuick::window() const
{
    return QQuickItem::window();
}

QSGTexture *RenderWidgetHostViewQtDelegateQuick::createTextureFromImage(const QImage &image)
{
    return QQuickItem::window()->createTextureFromImage(image, QQuickWindow::TextureCanUseAtlas);
}

QSGLayer *RenderWidgetHostViewQtDelegateQuick::createLayer()
{
    QSGRenderContext *renderContext = QQuickWindowPrivate::get(QQuickItem::window())->context;
    return renderContext->sceneGraphContext()->createLayer(renderContext);
}

QSGInternalImageNode *RenderWidgetHostViewQtDelegateQuick::createImageNode()
{
    QSGRenderContext *renderContext = QQuickWindowPrivate::get(QQuickItem::window())->context;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
    return renderContext->sceneGraphContext()->createInternalImageNode();
#else
    return renderContext->sceneGraphContext()->createImageNode();
#endif
}

QSGTextureNode *RenderWidgetHostViewQtDelegateQuick::createTextureNode()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
    return QQuickItem::window()->createImageNode();
#else
    return new QSGSimpleTextureNode();
#endif
}

QSGRectangleNode *RenderWidgetHostViewQtDelegateQuick::createRectangleNode()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
    return QQuickItem::window()->createRectangleNode();
#else
    QSGRenderContext *renderContext = QQuickWindowPrivate::get(QQuickItem::window())->context;
    return renderContext->sceneGraphContext()->createRectangleNode();
#endif
}

void RenderWidgetHostViewQtDelegateQuick::update()
{
    QQuickItem::update();
}

void RenderWidgetHostViewQtDelegateQuick::updateCursor(const QCursor &cursor)
{
    setCursor(cursor);
}

void RenderWidgetHostViewQtDelegateQuick::resize(int width, int height)
{
    setSize(QSizeF(width, height));
}

void RenderWidgetHostViewQtDelegateQuick::inputMethodStateChanged(bool editorVisible)
{
    if (qApp->inputMethod()->isVisible() == editorVisible)
        return;

    if (parentItem() && parentItem()->flags() & QQuickItem::ItemAcceptsInputMethod) {
        qApp->inputMethod()->update(Qt::ImQueryInput | Qt::ImEnabled | Qt::ImHints);
        qApp->inputMethod()->setVisible(editorVisible);
    }

}

bool RenderWidgetHostViewQtDelegateQuick::event(QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride) {
        if (editorActionForKeyEvent(static_cast<QKeyEvent*>(event)) != QQuickWebEngineView::NoWebAction) {
            event->accept();
            return true;
        }
    }

    return QQuickItem::event(event);
}

void RenderWidgetHostViewQtDelegateQuick::focusInEvent(QFocusEvent *event)
{
    m_client->forwardEvent(event);
}

void RenderWidgetHostViewQtDelegateQuick::focusOutEvent(QFocusEvent *event)
{
    m_client->forwardEvent(event);
}

void RenderWidgetHostViewQtDelegateQuick::mousePressEvent(QMouseEvent *event)
{
    QQuickItem *parent = parentItem();
    if (!m_isPopup && (parent && parent->property("activeFocusOnPress").toBool()))
        forceActiveFocus();
    if (!m_isPopup && parent && !parent->property("activeFocusOnPress").toBool() && !parent->hasActiveFocus()) {
        event->ignore();
        return;
    }
    m_client->forwardEvent(event);
}

void RenderWidgetHostViewQtDelegateQuick::mouseMoveEvent(QMouseEvent *event)
{
    QQuickItem *parent = parentItem();
    if (parent && !parent->property("activeFocusOnPress").toBool() && !parent->hasActiveFocus()) {
        event->ignore();
        return;
    }
    m_client->forwardEvent(event);
}

void RenderWidgetHostViewQtDelegateQuick::mouseReleaseEvent(QMouseEvent *event)
{
    QQuickItem *parent = parentItem();
    if (!m_isPopup && parent && !parent->property("activeFocusOnPress").toBool() && !parent->hasActiveFocus()) {
        event->ignore();
        return;
    }
    m_client->forwardEvent(event);
}

void RenderWidgetHostViewQtDelegateQuick::keyPressEvent(QKeyEvent *event)
{
    m_client->forwardEvent(event);
}

void RenderWidgetHostViewQtDelegateQuick::keyReleaseEvent(QKeyEvent *event)
{
    m_client->forwardEvent(event);
}

void RenderWidgetHostViewQtDelegateQuick::wheelEvent(QWheelEvent *event)
{
    m_client->forwardEvent(event);
}

void RenderWidgetHostViewQtDelegateQuick::touchEvent(QTouchEvent *event)
{
    QQuickItem *parent = parentItem();
    if (event->type() == QEvent::TouchBegin && !m_isPopup
            && (parent && parent->property("activeFocusOnPress").toBool()))
        forceActiveFocus();
    if (parent && !parent->property("activeFocusOnPress").toBool() && !parent->hasActiveFocus()) {
        event->ignore();
        return;
    }
    m_client->forwardEvent(event);
}

void RenderWidgetHostViewQtDelegateQuick::hoverMoveEvent(QHoverEvent *event)
{
    QQuickItem *parent = parentItem();
    if (!m_isPopup && parent && !parent->property("activeFocusOnPress").toBool() && !parent->hasActiveFocus()) {
        event->ignore();
        return;
    }
    m_client->forwardEvent(event);
}

QVariant RenderWidgetHostViewQtDelegateQuick::inputMethodQuery(Qt::InputMethodQuery query) const
{
    return m_client->inputMethodQuery(query);
}

void RenderWidgetHostViewQtDelegateQuick::inputMethodEvent(QInputMethodEvent *event)
{
    m_client->forwardEvent(event);
}

void RenderWidgetHostViewQtDelegateQuick::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    if (window()) {
        // TODO(pvarga): Use QQuickItem::mapToGlobal from Qt 5.7
        const QPoint globalPos = window()->mapToGlobal(position().toPoint());
        if (globalPos != m_lastGlobalPos) {
            m_lastGlobalPos = globalPos;
            m_client->windowBoundsChanged();
        }
    }

    m_client->notifyResize();
}

void RenderWidgetHostViewQtDelegateQuick::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickItem::itemChange(change, value);
    if (change == QQuickItem::ItemSceneChange) {
        foreach (const QMetaObject::Connection &c, m_windowConnections)
            disconnect(c);
        m_windowConnections.clear();
        if (value.window) {
            m_windowConnections.append(connect(value.window, SIGNAL(xChanged(int)), SLOT(onWindowPosChanged())));
            m_windowConnections.append(connect(value.window, SIGNAL(yChanged(int)), SLOT(onWindowPosChanged())));
            if (!m_isPopup)
                m_windowConnections.append(connect(value.window, SIGNAL(closing(QQuickCloseEvent *)), SLOT(onHide())));
        }

        if (m_initialized)
            m_client->windowChanged();
    } else if (change == QQuickItem::ItemVisibleHasChanged) {
        if (!m_isPopup && !value.boolValue)
            onHide();
    }
}

QSGNode *RenderWidgetHostViewQtDelegateQuick::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    return m_client->updatePaintNode(oldNode);
}

void RenderWidgetHostViewQtDelegateQuick::onWindowPosChanged()
{
    if (window()) {
        // TODO(pvarga): Use QQuickItem::mapToGlobal from Qt 5.7
        m_lastGlobalPos = window()->mapToGlobal(position().toPoint());
    }
    m_client->windowBoundsChanged();
}

void RenderWidgetHostViewQtDelegateQuick::onHide()
{
    QFocusEvent event(QEvent::FocusOut, Qt::OtherFocusReason);
    m_client->forwardEvent(&event);
}

} // namespace QtWebEngineCore
