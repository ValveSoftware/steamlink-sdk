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

#include <QtCore/qcompilerdetection.h> // Needed for Q_DECL_OVERRIDE

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
    virtual content::BrowserMainParts* CreateBrowserMainParts(const content::MainFunctionParams&) Q_DECL_OVERRIDE;
    virtual void RenderProcessWillLaunch(content::RenderProcessHost* host) Q_DECL_OVERRIDE;
    virtual void ResourceDispatcherHostCreated() Q_DECL_OVERRIDE;
    virtual gl::GLShareGroup* GetInProcessGpuShareGroup() Q_DECL_OVERRIDE;
    virtual content::MediaObserver* GetMediaObserver() Q_DECL_OVERRIDE;
    virtual content::GeolocationDelegate* CreateGeolocationDelegate() Q_DECL_OVERRIDE;
    virtual content::QuotaPermissionContext *CreateQuotaPermissionContext() Q_DECL_OVERRIDE;
    virtual void OverrideWebkitPrefs(content::RenderViewHost *, content::WebPreferences *) Q_DECL_OVERRIDE;
    virtual void AllowCertificateError(content::WebContents* web_contents,
                                       int cert_error,
                                       const net::SSLInfo& ssl_info,
                                       const GURL& request_url,
                                       content::ResourceType resource_type,
                                       bool overridable,
                                       bool strict_enforcement,
                                       bool expired_previous_decision,
                                       const base::Callback<void(bool)>& callback,
                                       content::CertificateRequestResultType* result) Q_DECL_OVERRIDE;
    virtual void SelectClientCertificate(content::WebContents* web_contents,
                                         net::SSLCertRequestInfo* cert_request_info,
                                         std::unique_ptr<content::ClientCertificateDelegate> delegate) Q_DECL_OVERRIDE;
    content::DevToolsManagerDelegate *GetDevToolsManagerDelegate() Q_DECL_OVERRIDE;

    virtual std::string GetApplicationLocale() Q_DECL_OVERRIDE;
    std::string GetAcceptLangs(content::BrowserContext* context) Q_DECL_OVERRIDE;
    virtual void AppendExtraCommandLineSwitches(base::CommandLine* command_line, int child_process_id) Q_DECL_OVERRIDE;
    virtual void GetAdditionalWebUISchemes(std::vector<std::string>* additional_schemes) Q_DECL_OVERRIDE;

#if defined(Q_OS_LINUX)
    virtual void GetAdditionalMappedFilesForChildProcess(const base::CommandLine& command_line, int child_process_id, content::FileDescriptorInfo* mappings) Q_DECL_OVERRIDE;
#endif

#if defined(ENABLE_PLUGINS)
    virtual void DidCreatePpapiPlugin(content::BrowserPpapiHost* browser_host) Q_DECL_OVERRIDE;
#endif

private:
    BrowserMainPartsQt* m_browserMainParts;
    std::unique_ptr<ResourceDispatcherHostDelegateQt> m_resourceDispatcherHostDelegate;
    scoped_refptr<ShareGroupQtQuick> m_shareGroupQtQuick;
};

} // namespace QtWebEngineCore

#endif // CONTENT_BROWSER_CLIENT_QT_H
