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

#include "render_widget_host_view_qt_delegate_widget.h"

#include "qwebenginepage_p.h"
#include "qwebengineview.h"
#include <QGuiApplication>
#include <QLayout>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QResizeEvent>
#include <QSGAbstractRenderer>
#include <QSGNode>
#include <QWindow>
#include <private/qquickwindow_p.h>

#if (QT_VERSION < QT_VERSION_CHECK(5, 8, 0))
#include <QSGSimpleRectNode>
#include <QSGSimpleTextureNode>
#endif
#include <private/qwidget_p.h>

namespace QtWebEngineCore {

class RenderWidgetHostViewQuickItem : public QQuickItem {
public:
    RenderWidgetHostViewQuickItem(RenderWidgetHostViewQtDelegateClient *client) : m_client(client)
    {
        setFlag(ItemHasContents, true);
    }
protected:
    void focusInEvent(QFocusEvent *event) override
    {
        m_client->forwardEvent(event);
    }
    void focusOutEvent(QFocusEvent *event) override
    {
        m_client->forwardEvent(event);
    }
    void keyPressEvent(QKeyEvent *event) override
    {
        m_client->forwardEvent(event);
    }
    void keyReleaseEvent(QKeyEvent *event) override
    {
        m_client->forwardEvent(event);
    }
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override
    {
        return m_client->updatePaintNode(oldNode);
    }
private:
    RenderWidgetHostViewQtDelegateClient *m_client;
};

RenderWidgetHostViewQtDelegateWidget::RenderWidgetHostViewQtDelegateWidget(RenderWidgetHostViewQtDelegateClient *client, QWidget *parent)
    : QQuickWidget(parent)
    , m_client(client)
    , m_rootItem(new RenderWidgetHostViewQuickItem(client))
    , m_isPopup(false)
{
    setFocusPolicy(Qt::StrongFocus);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);

#ifndef QT_NO_OPENGL
    QOpenGLContext *globalSharedContext = QOpenGLContext::globalShareContext();
    if (globalSharedContext) {
        QSurfaceFormat sharedFormat = globalSharedContext->format();

#ifdef Q_OS_OSX
        // Check that the default QSurfaceFormat OpenGL profile is compatible with the global OpenGL
        // shared context profile, otherwise this could lead to a nasty crash.
        QSurfaceFormat defaultFormat = QSurfaceFormat::defaultFormat();

        if (defaultFormat.profile() != sharedFormat.profile()
            && defaultFormat.profile() == QSurfaceFormat::CoreProfile
            && defaultFormat.version() >= qMakePair(3, 2)) {
            qFatal("QWebEngine: Default QSurfaceFormat OpenGL profile is not compatible with the "
                   "global shared context OpenGL profile. Please make sure you set a compatible "
                   "QSurfaceFormat before the QtGui application instance is created.");
        }
#endif

        // Make sure the OpenGL profile of the QQuickWidget matches the shared context profile.
        if (sharedFormat.profile() == QSurfaceFormat::CoreProfile) {
            format.setMajorVersion(sharedFormat.majorVersion());
            format.setMinorVersion(sharedFormat.minorVersion());
            format.setProfile(sharedFormat.profile());
        }
    }

    setFormat(format);
#endif
#endif
    setMouseTracking(true);
    setAttribute(Qt::WA_AcceptTouchEvents);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_AlwaysShowToolTips);

    if (parent) {
        // Unset the popup parent if the parent is being destroyed, thus making sure a double
        // delete does not happen.
        // Also in case the delegate is destroyed before its parent (when a popup is simply
        // dismissed), this connection will automatically be removed by ~QObject(), preventing
        // a use-after-free.
        connect(parent, &QObject::destroyed,
                this, &RenderWidgetHostViewQtDelegateWidget::removeParentBeforeParentDelete);
    }
}

void RenderWidgetHostViewQtDelegateWidget::removeParentBeforeParentDelete()
{
    setParent(Q_NULLPTR);
}

void RenderWidgetHostViewQtDelegateWidget::initAsChild(WebContentsAdapterClient* container)
{
    setContent(QUrl(), nullptr, m_rootItem.data());

    QWebEnginePagePrivate *pagePrivate = static_cast<QWebEnginePagePrivate *>(container);
    if (pagePrivate->view) {
        pagePrivate->view->layout()->addWidget(this);
        pagePrivate->view->setFocusProxy(this);
        show();
    } else
        setParent(0);
}

void RenderWidgetHostViewQtDelegateWidget::initAsPopup(const QRect& screenRect)
{
    m_isPopup = true;
    setContent(QUrl(), nullptr, m_rootItem.data());

    // The keyboard events are supposed to go to the parent RenderHostView
    // so the WebUI popups should never have focus. Besides, if the parent view
    // loses focus, WebKit will cause its associated popups (including this one)
    // to be destroyed.
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);

    // macOS doesn't like Qt::ToolTip when QWebEngineView is inside a modal dialog, specifically by
    // not forwarding click events to the popup. So we use Qt::Tool which behaves the same way, but
    // works on macOS too.
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);

    setGeometry(screenRect);
    show();
}

QRectF RenderWidgetHostViewQtDelegateWidget::screenRect() const
{
    return QRectF(x(), y(), width(), height());
}

QRectF RenderWidgetHostViewQtDelegateWidget::contentsRect() const
{
    QPointF pos = mapToGlobal(QPoint(0, 0));
    return QRectF(pos.x(), pos.y(), width(), height());
}

void RenderWidgetHostViewQtDelegateWidget::setKeyboardFocus()
{
    m_rootItem->forceActiveFocus();
}

bool RenderWidgetHostViewQtDelegateWidget::hasKeyboardFocus()
{
    return m_rootItem->hasActiveFocus();
}

void RenderWidgetHostViewQtDelegateWidget::lockMouse()
{
    grabMouse();
}

void RenderWidgetHostViewQtDelegateWidget::unlockMouse()
{
    releaseMouse();
}

void RenderWidgetHostViewQtDelegateWidget::show()
{
    // Check if we're attached to a QWebEngineView, we don't
    // want to show anything else than popups as top-level.
    if (parent() || m_isPopup) {
        QQuickWidget::show();
    }
}

void RenderWidgetHostViewQtDelegateWidget::hide()
{
    QQuickWidget::hide();
}

bool RenderWidgetHostViewQtDelegateWidget::isVisible() const
{
    return QQuickWidget::isVisible();
}

QWindow* RenderWidgetHostViewQtDelegateWidget::window() const
{
    const QWidget* root = QQuickWidget::window();
    return root ? root->windowHandle() : 0;
}

QSGTexture *RenderWidgetHostViewQtDelegateWidget::createTextureFromImage(const QImage &image)
{
    return quickWindow()->createTextureFromImage(image, QQuickWindow::TextureCanUseAtlas);
}

QSGLayer *RenderWidgetHostViewQtDelegateWidget::createLayer()
{
    QSGRenderContext *renderContext = QQuickWindowPrivate::get(quickWindow())->context;
    return renderContext->sceneGraphContext()->createLayer(renderContext);
}

QSGInternalImageNode *RenderWidgetHostViewQtDelegateWidget::createImageNode()
{
    QSGRenderContext *renderContext = QQuickWindowPrivate::get(quickWindow())->context;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
    return renderContext->sceneGraphContext()->createInternalImageNode();
#else
    return renderContext->sceneGraphContext()->createImageNode();
#endif
}

QSGTextureNode *RenderWidgetHostViewQtDelegateWidget::createTextureNode()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
    return quickWindow()->createImageNode();
#else
    return new QSGSimpleTextureNode();
#endif
}

QSGRectangleNode *RenderWidgetHostViewQtDelegateWidget::createRectangleNode()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
    return quickWindow()->createRectangleNode();
#else
    QSGRenderContext *renderContext = QQuickWindowPrivate::get(quickWindow())->context;
    return renderContext->sceneGraphContext()->createRectangleNode();
#endif
}

void RenderWidgetHostViewQtDelegateWidget::update()
{
    m_rootItem->update();
}

void RenderWidgetHostViewQtDelegateWidget::updateCursor(const QCursor &cursor)
{
    QQuickWidget::setCursor(cursor);
}

void RenderWidgetHostViewQtDelegateWidget::resize(int width, int height)
{
    QQuickWidget::resize(width, height);
}

void RenderWidgetHostViewQtDelegateWidget::move(const QPoint &screenPos)
{
    Q_ASSERT(m_isPopup);
    QQuickWidget::move(screenPos);
}

void RenderWidgetHostViewQtDelegateWidget::inputMethodStateChanged(bool editorVisible)
{
    if (qApp->inputMethod()->isVisible() == editorVisible)
        return;

    QQuickWidget::setAttribute(Qt::WA_InputMethodEnabled, editorVisible);
    qApp->inputMethod()->update(Qt::ImQueryInput | Qt::ImEnabled | Qt::ImHints);
    qApp->inputMethod()->setVisible(editorVisible);
}

void RenderWidgetHostViewQtDelegateWidget::setClearColor(const QColor &color)
{
    QQuickWidget::setClearColor(color);
    // QQuickWidget is usually blended by punching holes into widgets
    // above it to simulate the visual stacking order. If we want it to be
    // transparent we have to throw away the proper stacking order and always
    // blend the complete normal widgets backing store under it.
    bool isTranslucent = color.alpha() < 255;
    setAttribute(Qt::WA_AlwaysStackOnTop, isTranslucent);
    setAttribute(Qt::WA_OpaquePaintEvent, !isTranslucent);
    update();
}

QVariant RenderWidgetHostViewQtDelegateWidget::inputMethodQuery(Qt::InputMethodQuery query) const
{
    return m_client->inputMethodQuery(query);
}

void RenderWidgetHostViewQtDelegateWidget::resizeEvent(QResizeEvent *resizeEvent)
{
    QQuickWidget::resizeEvent(resizeEvent);

    const QPoint globalPos = mapToGlobal(pos());
    if (globalPos != m_lastGlobalPos) {
        m_lastGlobalPos = globalPos;
        m_client->windowBoundsChanged();
    }

    m_client->notifyResize();
}

void RenderWidgetHostViewQtDelegateWidget::showEvent(QShowEvent *event)
{
    QQuickWidget::showEvent(event);
    // We don't have a way to catch a top-level window change with QWidget
    // but a widget will most likely be shown again if it changes, so do
    // the reconnection at this point.
    foreach (const QMetaObject::Connection &c, m_windowConnections)
        disconnect(c);
    m_windowConnections.clear();
    if (QWindow *w = window()) {
        m_windowConnections.append(connect(w, SIGNAL(xChanged(int)), SLOT(onWindowPosChanged())));
        m_windowConnections.append(connect(w, SIGNAL(yChanged(int)), SLOT(onWindowPosChanged())));
    }
    m_client->windowChanged();
    m_client->notifyShown();
}

void RenderWidgetHostViewQtDelegateWidget::hideEvent(QHideEvent *event)
{
    QQuickWidget::hideEvent(event);
    m_client->notifyHidden();
}

bool RenderWidgetHostViewQtDelegateWidget::event(QEvent *event)
{
    bool handled = false;

    // Mimic QWidget::event() by ignoring mouse, keyboard, touch and tablet events if the widget is
    // disabled.
    if (!isEnabled()) {
        switch (event->type()) {
        case QEvent::TabletPress:
        case QEvent::TabletRelease:
        case QEvent::TabletMove:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
        case QEvent::ContextMenu:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
#ifndef QT_NO_WHEELEVENT
        case QEvent::Wheel:
#endif
            return false;
        default:
            break;
        }
    }

    switch (event->type()) {
    case QEvent::FocusIn:
    case QEvent::FocusOut:
        // We forward focus events later, once they have made it to the m_rootItem.
        return QQuickWidget::event(event);
    case QEvent::ShortcutOverride:
        if (editorActionForKeyEvent(static_cast<QKeyEvent*>(event)) != QWebEnginePage::NoWebAction) {
            event->accept();
            return true;
        }
        break;
    case QEvent::DragEnter:
    case QEvent::DragLeave:
    case QEvent::DragMove:
    case QEvent::Drop:
        // Let the parent handle these events.
        return false;
    default:
        break;
    }

    QEvent::Type type = event->type();
    if (type == QEvent::FocusIn) {
        QWidgetPrivate *d = QWidgetPrivate::get(this);
        d->updateWidgetTransform(event);
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        // QWidget keeps the Qt4 behavior where the DblClick event would replace the Press event.
        // QtQuick is different by sending both the Press and DblClick events for the second press
        // where we can simply ignore the DblClick event.
        QMouseEvent *dblClick = static_cast<QMouseEvent *>(event);
        QMouseEvent press(QEvent::MouseButtonPress, dblClick->localPos(), dblClick->windowPos(), dblClick->screenPos(),
            dblClick->button(), dblClick->buttons(), dblClick->modifiers());
        press.setTimestamp(dblClick->timestamp());
        handled = m_client->forwardEvent(&press);
    } else
        handled = m_client->forwardEvent(event);

    if (!handled)
        return QQuickWidget::event(event);
    return true;
}

void RenderWidgetHostViewQtDelegateWidget::onWindowPosChanged()
{
    m_lastGlobalPos = mapToGlobal(pos());
    m_client->windowBoundsChanged();
}

} // namespace QtWebEngineCore
