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

#ifndef CONTENT_BROWSER_CLIENT_QT_H
#define CONTENT_BROWSER_CLIENT_QT_H

#include "base/memory/ref_counted.h"
#include "content/public/browser/content_browser_client.h"

#include <QtGlobal>

namespace net {
class URLRequestContextGetter;
}

namespace content {
class BrowserContext;
class BrowserMainParts;

#if defined(ENABLE_PLUGINS)
class BrowserPpapiHost;
#endif

class DevToolsManagerDelegate;
class RenderFrameHost;
class RenderProcessHost;
class RenderViewHostDelegateView;
class ResourceContext;
class WebContentsViewPort;
class WebContents;
struct MainFunctionParams;
}

namespace gl {
class GLShareGroup;
}

namespace service_manager {
class InterfaceRegistry;
}

namespace QtWebEngineCore {
class BrowserContextQt;
class BrowserMainPartsQt;
class ResourceDispatcherHostDelegateQt;
class ShareGroupQtQuick;

class ContentBrowserClientQt : public content::ContentBrowserClient {

public:
    ContentBrowserClientQt();
    ~ContentBrowserClientQt();
    static ContentBrowserClientQt* Get();
    content::BrowserMainParts* CreateBrowserMainParts(const content::MainFunctionParams&) override;
    void RenderProcessWillLaunch(content::RenderProcessHost* host) override;
    void ResourceDispatcherHostCreated() override;
    gl::GLShareGroup* GetInProcessGpuShareGroup() override;
    content::MediaObserver* GetMediaObserver() override;
    content::QuotaPermissionContext *CreateQuotaPermissionContext() override;
    void OverrideWebkitPrefs(content::RenderViewHost *, content::WebPreferences *) override;
    void AllowCertificateError(content::WebContents* web_contents,
                                       int cert_error,
                                       const net::SSLInfo& ssl_info,
                                       const GURL& request_url,
                                       content::ResourceType resource_type,
                                       bool overridable,
                                       bool strict_enforcement,
                                       bool expired_previous_decision,
                                       const base::Callback<void(content::CertificateRequestResultType)>& callback) override;
    void SelectClientCertificate(content::WebContents* web_contents,
                                         net::SSLCertRequestInfo* cert_request_info,
                                         std::unique_ptr<content::ClientCertificateDelegate> delegate) override;
    content::DevToolsManagerDelegate *GetDevToolsManagerDelegate() override;

    std::string GetApplicationLocale() override;
    std::string GetAcceptLangs(content::BrowserContext* context) override;
    void AppendExtraCommandLineSwitches(base::CommandLine* command_line, int child_process_id) override;
    void GetAdditionalWebUISchemes(std::vector<std::string>* additional_schemes) override;

    void RegisterRenderFrameMojoInterfaces(service_manager::InterfaceRegistry* registry, content::RenderFrameHost* render_frame_host) override;

#if defined(Q_OS_LINUX)
    void GetAdditionalMappedFilesForChildProcess(const base::CommandLine& command_line, int child_process_id, content::FileDescriptorInfo* mappings) override;
#endif

#if defined(ENABLE_PLUGINS)
    void DidCreatePpapiPlugin(content::BrowserPpapiHost* browser_host) override;
#endif

private:
    BrowserMainPartsQt* m_browserMainParts;
    std::unique_ptr<ResourceDispatcherHostDelegateQt> m_resourceDispatcherHostDelegate;
    scoped_refptr<ShareGroupQtQuick> m_shareGroupQtQuick;
};

} // namespace QtWebEngineCore

#endif // CONTENT_BROWSER_CLIENT_QT_H
