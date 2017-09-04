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

    content::RenderWidgetHostViewBase *CreateViewForWidget(content::RenderWidgetHost* render_widget_host, bool is_guest_view_hack) override;

    void CreateView(const gfx::Size& initial_size, gfx::NativeView context) override;

    content::RenderWidgetHostViewBase* CreateViewForPopupWidget(content::RenderWidgetHost* render_widget_host) override;

    void SetPageTitle(const base::string16& title) override { }

    void RenderViewCreated(content::RenderViewHost* host) override;

    void RenderViewSwappedIn(content::RenderViewHost* host) override { QT_NOT_YET_IMPLEMENTED }

    void SetOverscrollControllerEnabled(bool enabled) override { QT_NOT_YET_IMPLEMENTED }

    gfx::NativeView GetNativeView() const override;

    gfx::NativeView GetContentNativeView() const override { QT_NOT_USED return 0; }

    gfx::NativeWindow GetTopLevelNativeWindow() const override { QT_NOT_USED return 0; }

    void GetContainerBounds(gfx::Rect* out) const override;

    void SizeContents(const gfx::Size& size) override { QT_NOT_YET_IMPLEMENTED }

    void Focus() override;

    void SetInitialFocus() override;

    void StoreFocus() override { QT_NOT_USED }

    void RestoreFocus() override { QT_NOT_USED }

    content::DropData* GetDropData() const override { QT_NOT_YET_IMPLEMENTED return 0; }

    gfx::Rect GetViewBounds() const override { QT_NOT_YET_IMPLEMENTED return gfx::Rect(); }

    void StartDragging(const content::DropData& drop_data, blink::WebDragOperationsMask allowed_ops,
                       const gfx::ImageSkia& image, const gfx::Vector2d& image_offset,
                       const content::DragEventSourceInfo& event_info,
                       content::RenderWidgetHostImpl* source_rwh) override;

    void UpdateDragCursor(blink::WebDragOperation dragOperation) override;

    void ShowContextMenu(content::RenderFrameHost *, const content::ContextMenuParams &params) override;

    void TakeFocus(bool reverse) override;

    void GetScreenInfo(content::ScreenInfo* results) const override;

#if defined(OS_MACOSX)
    void SetAllowOtherViews(bool allow) override { m_allowOtherViews = allow; }
    bool GetAllowOtherViews() const override { return m_allowOtherViews; }
    void CloseTabAfterEventTracking() override { QT_NOT_YET_IMPLEMENTED }
    bool IsEventTracking() const override { QT_NOT_YET_IMPLEMENTED; return false; }
#endif // defined(OS_MACOSX)

private:
    content::WebContents *m_webContents;
    WebContentsAdapterClient *m_client;
    WebContentsAdapterClient *m_factoryClient;
    bool m_allowOtherViews;
};

} // namespace QtWebEngineCore

#endif // WEB_CONTENTS_VIEW_QT_H
