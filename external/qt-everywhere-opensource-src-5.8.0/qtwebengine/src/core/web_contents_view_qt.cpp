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

#include "web_contents_view_qt.h"

#include "browser_context_adapter.h"
#include "content_browser_client_qt.h"
#include "render_widget_host_view_qt_delegate.h"
#include "type_conversion.h"
#include "web_contents_adapter.h"
#include "web_engine_context.h"

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/common/context_menu_params.h"
#include <ui/gfx/image/image_skia.h>

#include <QtGui/qpixmap.h>

namespace QtWebEngineCore {

void WebContentsViewQt::initialize(WebContentsAdapterClient* client)
{
    m_client = client;
    m_factoryClient = client;

    // Check if a RWHV was created before the initialization.
    if (auto rwhv = static_cast<RenderWidgetHostViewQt *>(m_webContents->GetRenderWidgetHostView())) {
        rwhv->setAdapterClient(client);
        rwhv->SetBackgroundColor(toSk(client->backgroundColor()));
    }
}

content::RenderWidgetHostViewBase* WebContentsViewQt::CreateViewForWidget(content::RenderWidgetHost* render_widget_host, bool is_guest_view_hack)
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

void WebContentsViewQt::RenderViewCreated(content::RenderViewHost* host)
{
    // The render process is done creating the RenderView and it's ready to be routed
    // messages at this point.
    if (m_client)
        m_webContents->GetRenderWidgetHostView()->SetBackgroundColor(toSk(m_client->backgroundColor()));
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
    if (!m_client->isEnabled())
        return;
    if (content::RenderWidgetHostView *rwhv = m_webContents->GetRenderWidgetHostView())
        rwhv->Focus();
    m_client->focusContainer();
}

void WebContentsViewQt::SetInitialFocus()
{
    Focus();
}

ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaTypeNone, blink::WebContextMenuData::MediaTypeNone)
ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaTypeImage, blink::WebContextMenuData::MediaTypeImage)
ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaTypeVideo, blink::WebContextMenuData::MediaTypeVideo)
ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaTypeAudio, blink::WebContextMenuData::MediaTypeAudio)
ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaTypeCanvas, blink::WebContextMenuData::MediaTypeCanvas)
ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaTypeFile, blink::WebContextMenuData::MediaTypeFile)
ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaTypePlugin, blink::WebContextMenuData::MediaTypePlugin)

ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaNone, blink::WebContextMenuData::MediaNone)
ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaInError, blink::WebContextMenuData::MediaInError)
ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaPaused, blink::WebContextMenuData::MediaPaused)
ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaMuted, blink::WebContextMenuData::MediaMuted)
ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaLoop, blink::WebContextMenuData::MediaLoop)
ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaCanSave, blink::WebContextMenuData::MediaCanSave)
ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaHasAudio, blink::WebContextMenuData::MediaHasAudio)
ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaCanToggleControls, blink::WebContextMenuData::MediaCanToggleControls)
ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaControls, blink::WebContextMenuData::MediaControls)
ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaCanPrint, blink::WebContextMenuData::MediaCanPrint)
ASSERT_ENUMS_MATCH(WebEngineContextMenuData::MediaCanRotate, blink::WebContextMenuData::MediaCanRotate)

static inline WebEngineContextMenuData fromParams(const content::ContextMenuParams &params)
{
    WebEngineContextMenuData ret;
    ret.setPosition(QPoint(params.x, params.y));
    ret.setLinkUrl(toQt(params.link_url));
    ret.setLinkText(toQt(params.link_text.data()));
    ret.setSelectedText(toQt(params.selection_text.data()));
    ret.setMediaUrl(toQt(params.src_url));
    ret.setMediaType((WebEngineContextMenuData::MediaType)params.media_type);
    ret.setHasImageContent(params.has_image_contents);
    ret.setMediaFlags((WebEngineContextMenuData::MediaFlags)params.media_flags);
    ret.setSuggestedFileName(toQt(params.suggested_filename.data()));
    ret.setIsEditable(params.is_editable);
#if defined(ENABLE_SPELLCHECK)
    ret.setMisspelledWord(toQt(params.misspelled_word));
    ret.setSpellCheckerSuggestions(fromVector(params.dictionary_suggestions));
#endif
    return ret;
}

void WebContentsViewQt::ShowContextMenu(content::RenderFrameHost *, const content::ContextMenuParams &params)
{
    WebEngineContextMenuData contextMenuData(fromParams(params));
#if defined(ENABLE_SPELLCHECK)
    // Do not use params.spellcheck_enabled, since it is never
    // correctly initialized for chrome asynchronous spellchecking.
    // Even fixing the initialization in ContextMenuClientImpl::showContextMenu
    // will not work. By default SpellCheck::spellcheck_enabled_
    // must be initialized to true due to the way how the initialization sequence
    // in SpellCheck works ie. typing the first word triggers the creation
    // of the SpellcheckService. Use user preference store instead.
    contextMenuData.setIsSpellCheckerEnabled(m_client->browserContextAdapter()->isSpellCheckEnabled());
#endif
    m_client->contextMenuRequested(contextMenuData);
}

Qt::DropActions toQtDropActions(blink::WebDragOperationsMask ops)
{
    Qt::DropActions result;
    if (ops & blink::WebDragOperationCopy)
        result |= Qt::CopyAction;
    if (ops & blink::WebDragOperationLink)
        result |= Qt::LinkAction;
    if (ops & blink::WebDragOperationMove || ops & blink::WebDragOperationDelete)
        result |= Qt::MoveAction;
    return result;
}

Qt::DropAction toQt(blink::WebDragOperation op)
{
    if (op == blink::WebDragOperationCopy)
        return Qt::CopyAction;
    if (op == blink::WebDragOperationLink)
        return Qt::LinkAction;
    if (op == blink::WebDragOperationMove || op == blink::WebDragOperationDelete)
        return Qt::MoveAction;
    return Qt::IgnoreAction;
}

void WebContentsViewQt::StartDragging(const content::DropData &drop_data,
                                      blink::WebDragOperationsMask allowed_ops,
                                      const gfx::ImageSkia &image,
                                      const gfx::Vector2d &image_offset,
                                      const content::DragEventSourceInfo &event_info)
{
    Q_UNUSED(event_info);

    QPixmap pixmap;
    QPoint hotspot;
    pixmap = QPixmap::fromImage(toQImage(image.GetRepresentation(m_client->dpiScale())));
    if (!pixmap.isNull()) {
        hotspot.setX(image_offset.x());
        hotspot.setY(image_offset.y());
    }

    m_client->startDragging(drop_data, toQtDropActions(allowed_ops), pixmap, hotspot);
}

void WebContentsViewQt::UpdateDragCursor(blink::WebDragOperation dragOperation)
{
    m_client->webContentsAdapter()->updateDragAction(toQt(dragOperation));
}

void WebContentsViewQt::TakeFocus(bool reverse)
{
    m_client->passOnFocus(reverse);
}

} // namespace QtWebEngineCore
