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

#ifndef WEB_CONTENTS_VIEW_QT_H
#define WEB_CONTENTS_VIEW_QT_H

#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"

#include "qtwebenginecoreglobal_p.h"
#include "render_widget_host_view_qt.h"
#include "web_contents_adapter_client.h"
#include "web_contents_delegate_qt.h"
#include "web_engine_context.h"

namespace QtWebEngineCore {

class WebContentsViewQt
    : public content::WebContentsView
    , public content::RenderViewHostDelegateView
{
public:
    static inline WebContentsViewQt *from(WebContentsView *view) { return static_cast<WebContentsViewQt*>(view); }

    WebContentsViewQt(content::WebContents* webContents)
        : m_webContents(webContents)
        , m_client(0)
        , m_factoryClient(0)
        , m_allowOtherViews(false)
    { }

    void initialize(WebContentsAdapterClient* client);
    WebContentsAdapterClient *client() { return m_client; }

    virtual content::RenderWidgetHostViewBase *CreateViewForWidget(content::RenderWidgetHost* render_widget_host, bool is_guest_view_hack) Q_DECL_OVERRIDE;

    virtual void CreateView(const gfx::Size& initial_size, gfx::NativeView context) Q_DECL_OVERRIDE;

    virtual content::RenderWidgetHostViewBase* CreateViewForPopupWidget(content::RenderWidgetHost* render_widget_host) Q_DECL_OVERRIDE;

    virtual void SetPageTitle(const base::string16& title) Q_DECL_OVERRIDE { }

    virtual void RenderViewCreated(content::RenderViewHost* host) Q_DECL_OVERRIDE;

    virtual void RenderViewSwappedIn(content::RenderViewHost* host) Q_DECL_OVERRIDE { QT_NOT_YET_IMPLEMENTED }

    virtual void SetOverscrollControllerEnabled(bool enabled) Q_DECL_OVERRIDE { QT_NOT_YET_IMPLEMENTED }

    virtual gfx::NativeView GetNativeView() const Q_DECL_OVERRIDE;

    virtual gfx::NativeView GetContentNativeView() const Q_DECL_OVERRIDE { QT_NOT_USED return 0; }

    virtual gfx::NativeWindow GetTopLevelNativeWindow() const Q_DECL_OVERRIDE { QT_NOT_USED return 0; }

    virtual void GetContainerBounds(gfx::Rect* out) const Q_DECL_OVERRIDE;

    virtual void SizeContents(const gfx::Size& size) Q_DECL_OVERRIDE { QT_NOT_YET_IMPLEMENTED }

    virtual void Focus() Q_DECL_OVERRIDE;

    virtual void SetInitialFocus() Q_DECL_OVERRIDE;

    virtual void StoreFocus() Q_DECL_OVERRIDE { QT_NOT_USED }

    virtual void RestoreFocus() Q_DECL_OVERRIDE { QT_NOT_USED }

    virtual content::DropData* GetDropData() const Q_DECL_OVERRIDE { QT_NOT_YET_IMPLEMENTED return 0; }

    virtual gfx::Rect GetViewBounds() const Q_DECL_OVERRIDE { QT_NOT_YET_IMPLEMENTED return gfx::Rect(); }

    void StartDragging(const content::DropData &drop_data, blink::WebDragOperationsMask allowed_ops,
                       const gfx::ImageSkia &image,  const gfx::Vector2d &image_offset,
                       const content::DragEventSourceInfo &event_info) Q_DECL_OVERRIDE;

    void UpdateDragCursor(blink::WebDragOperation dragOperation) Q_DECL_OVERRIDE;

    virtual void ShowContextMenu(content::RenderFrameHost *, const content::ContextMenuParams &params) Q_DECL_OVERRIDE;

    virtual void TakeFocus(bool reverse) Q_DECL_OVERRIDE;

#if defined(OS_MACOSX)
    virtual void SetAllowOtherViews(bool allow) Q_DECL_OVERRIDE { m_allowOtherViews = allow; }
    virtual bool GetAllowOtherViews() const Q_DECL_OVERRIDE { return m_allowOtherViews; }
    virtual void CloseTabAfterEventTracking() Q_DECL_OVERRIDE { QT_NOT_YET_IMPLEMENTED }
    virtual bool IsEventTracking() const Q_DECL_OVERRIDE { QT_NOT_YET_IMPLEMENTED; return false; }
    virtual void SetOverlayView(WebContentsView* overlay, const gfx::Point& offset) { QT_NOT_YET_IMPLEMENTED }
    virtual void RemoveOverlayView() { QT_NOT_YET_IMPLEMENTED }
#endif // defined(OS_MACOSX)

private:
    content::WebContents *m_webContents;
    WebContentsAdapterClient *m_client;
    WebContentsAdapterClient *m_factoryClient;
    bool m_allowOtherViews;
};

} // namespace QtWebEngineCore

#endif // WEB_CONTENTS_VIEW_QT_H
