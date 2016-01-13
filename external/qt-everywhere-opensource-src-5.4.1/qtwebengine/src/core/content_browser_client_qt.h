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

#ifndef CONTENT_BROWSER_CLIENT_QT_H
#define CONTENT_BROWSER_CLIENT_QT_H

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/content_browser_client.h"

#include <QtCore/qcompilerdetection.h> // Needed for Q_DECL_OVERRIDE

namespace net {
class URLRequestContextGetter;
}

namespace content {
class BrowserContext;
class BrowserMainParts;
class RenderProcessHost;
class RenderViewHostDelegateView;
class WebContentsViewPort;
class WebContents;
struct MainFunctionParams;
}

namespace gfx {
class GLShareGroup;
}

class BrowserContextQt;
class BrowserMainPartsQt;
class DevToolsHttpHandlerDelegateQt;
class ResourceDispatcherHostDelegateQt;
class ShareGroupQtQuick;

class ContentBrowserClientQt : public content::ContentBrowserClient {

public:
    ContentBrowserClientQt();
    ~ContentBrowserClientQt();
    static ContentBrowserClientQt* Get();
    virtual content::BrowserMainParts* CreateBrowserMainParts(const content::MainFunctionParams&) Q_DECL_OVERRIDE;
    virtual void RenderProcessWillLaunch(content::RenderProcessHost* host) Q_DECL_OVERRIDE;
    virtual void ResourceDispatcherHostCreated() Q_DECL_OVERRIDE;
    virtual gfx::GLShareGroup* GetInProcessGpuShareGroup() Q_DECL_OVERRIDE;
    virtual content::MediaObserver* GetMediaObserver() Q_DECL_OVERRIDE;
    virtual void OverrideWebkitPrefs(content::RenderViewHost *, const GURL &, WebPreferences *) Q_DECL_OVERRIDE;
    virtual content::AccessTokenStore *CreateAccessTokenStore() Q_DECL_OVERRIDE;
    virtual content::QuotaPermissionContext *CreateQuotaPermissionContext() Q_DECL_OVERRIDE;
    virtual void AllowCertificateError(
        int render_process_id,
        int render_frame_id,
        int cert_error,
        const net::SSLInfo& ssl_info,
        const GURL& request_url,
        ResourceType::Type resource_type,
        bool overridable,
        bool strict_enforcement,
        const base::Callback<void(bool)>& callback,
        content::CertificateRequestResultType* result) Q_DECL_OVERRIDE;
    virtual void RequestGeolocationPermission(
        content::WebContents *webContents,
        int bridge_id,
        const GURL &requesting_frame,
        bool user_gesture,
        base::Callback<void(bool)> result_callback,
        base::Closure *cancel_callback) Q_DECL_OVERRIDE;

    BrowserContextQt* browser_context();

    virtual net::URLRequestContextGetter *CreateRequestContext(content::BrowserContext *content_browser_context, content::ProtocolHandlerMap *protocol_handlers, content::URLRequestInterceptorScopedVector request_interceptorss) Q_DECL_OVERRIDE;

    void enableInspector(bool);

private:
    BrowserMainPartsQt* m_browserMainParts;
    scoped_ptr<DevToolsHttpHandlerDelegateQt> m_devtools;
    scoped_ptr<ResourceDispatcherHostDelegateQt> m_resourceDispatcherHostDelegate;
    scoped_refptr<ShareGroupQtQuick> m_shareGroupQtQuick;
};

#endif // CONTENT_BROWSER_CLIENT_QT_H
