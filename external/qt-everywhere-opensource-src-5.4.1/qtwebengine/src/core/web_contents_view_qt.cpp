/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "web_contents_view_qt.h"

#include "browser_context_qt.h"
#include "content_browser_client_qt.h"
#include "render_widget_host_view_qt_delegate.h"
#include "type_conversion.h"
#include "web_engine_context.h"

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/common/context_menu_params.h"

void WebContentsViewQt::initialize(WebContentsAdapterClient* client)
{
    m_client = client;

    // Check if a RWHV was created before the initialization.
    if (m_webContents->GetRenderWidgetHostView())
        static_cast<RenderWidgetHostViewQt *>(m_webContents->GetRenderWidgetHostView())->setAdapterClient(client);
}

content::RenderWidgetHostViewBase* WebContentsViewQt::CreateViewForWidget(content::RenderWidgetHost* render_widget_host)
{
    RenderWidgetHostViewQt *view = new RenderWidgetHostViewQt(render_widget_host);

    Q_ASSERT(m_factoryClient);
    view->setDelegate(m_factoryClient->CreateRenderWidgetHostViewQtDelegate(view));
    if (m_client)
        view->setAdapterClient(m_client);
    // Tell the RWHV delegate to attach itself to the native view container.
    view->InitAsChild(0);

    return view;
}

content::RenderWidgetHostViewBase* WebContentsViewQt::CreateViewForPopupWidget(content::RenderWidgetHost* render_widget_host)
{
    RenderWidgetHostViewQt *view = new RenderWidgetHostViewQt(render_widget_host);

    Q_ASSERT(m_client);
    view->setDelegate(m_client->CreateRenderWidgetHostViewQtDelegateForPopup(view));
    view->setAdapterClient(m_client);

    return view;
}

void WebContentsViewQt::CreateView(const gfx::Size& initial_size, gfx::NativeView context)
{
    // This is passed through content::WebContents::CreateParams::context either as the native view's client
    // directly or, in the case of a page-created new window, the client of the creating window's native view.
    m_factoryClient = reinterpret_cast<WebContentsAdapterClient *>(context);
}

gfx::NativeView WebContentsViewQt::GetNativeView() const
{
    // Hack to provide the client to WebContentsImpl::CreateNewWindow.
    return reinterpret_cast<gfx::NativeView>(m_client);
}

void WebContentsViewQt::GetContainerBounds(gfx::Rect* out) const
{
    const QRectF r(m_client->viewportRect());
    *out = gfx::Rect(r.x(), r.y(), r.width(), r.height());
}

void WebContentsViewQt::Focus()
{
    m_client->focusContainer();
}

void WebContentsViewQt::SetInitialFocus()
{
    Focus();
}

static WebEngineContextMenuData fromParams(const content::ContextMenuParams &params)
{
    WebEngineContextMenuData ret;
    ret.pos = QPoint(params.x, params.y);
    ret.linkUrl = toQt(params.link_url);
    ret.linkText = toQt(params.link_text.data());
    ret.selectedText = toQt(params.selection_text.data());
    return ret;
}

void WebContentsViewQt::ShowContextMenu(content::RenderFrameHost *, const content::ContextMenuParams &params)
{
    WebEngineContextMenuData contextMenuData(fromParams(params));
    m_client->contextMenuRequested(contextMenuData);
}

void WebContentsViewQt::StartDragging(const content::DropData& drop_data, blink::WebDragOperationsMask allowed_ops, const gfx::ImageSkia& image, const gfx::Vector2d& image_offset, const content::DragEventSourceInfo& event_info)
{
    Q_UNUSED(&drop_data);
    Q_UNUSED(allowed_ops);
    Q_UNUSED(&image);
    Q_UNUSED(&image_offset);
    Q_UNUSED(&event_info);
    // Tell the renderer to cancel the drag, see StartDragging's declaration in render_view_host_delegate_view.h for info.
    m_webContents->SystemDragEnded();
}

void WebContentsViewQt::TakeFocus(bool reverse)
{
    m_client->passOnFocus(reverse);
}
