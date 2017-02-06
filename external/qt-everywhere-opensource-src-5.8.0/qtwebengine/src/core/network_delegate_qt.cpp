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

#include "network_delegate_qt.h"

#include "browser_context_adapter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/resource_request_info.h"
#include "cookie_monster_delegate_qt.h"
#include "ui/base/page_transition_types.h"
#include "url_request_context_getter_qt.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"
#include "qwebengineurlrequestinfo.h"
#include "qwebengineurlrequestinfo_p.h"
#include "qwebengineurlrequestinterceptor.h"
#include "type_conversion.h"
#include "web_contents_adapter_client.h"
#include "web_contents_view_qt.h"

namespace QtWebEngineCore {

WebContentsAdapterClient::NavigationType pageTransitionToNavigationType(ui::PageTransition transition)
{
    int32_t qualifier = ui::PageTransitionGetQualifier(transition);

    if (qualifier & ui::PAGE_TRANSITION_FORWARD_BACK)
        return WebContentsAdapterClient::BackForwardNavigation;

    ui::PageTransition strippedTransition = ui::PageTransitionStripQualifier(transition);

    switch (strippedTransition) {
    case ui::PAGE_TRANSITION_LINK:
        return WebContentsAdapterClient::LinkNavigation;
    case ui::PAGE_TRANSITION_TYPED:
        return WebContentsAdapterClient::TypedNavigation;
    case ui::PAGE_TRANSITION_FORM_SUBMIT:
        return WebContentsAdapterClient::FormSubmittedNavigation;
    case ui::PAGE_TRANSITION_RELOAD:
        return WebContentsAdapterClient::ReloadNavigation;
    default:
        return WebContentsAdapterClient::OtherNavigation;
    }
}

QWebEngineUrlRequestInfo::ResourceType toQt(content::ResourceType resourceType)
{
    if (resourceType >= 0 && resourceType < content::ResourceType(QWebEngineUrlRequestInfo::ResourceTypeLast))
        return static_cast<QWebEngineUrlRequestInfo::ResourceType>(resourceType);
    return QWebEngineUrlRequestInfo::ResourceTypeUnknown;
}

QWebEngineUrlRequestInfo::NavigationType toQt(WebContentsAdapterClient::NavigationType navigationType)
{
    return static_cast<QWebEngineUrlRequestInfo::NavigationType>(navigationType);
}

NetworkDelegateQt::NetworkDelegateQt(URLRequestContextGetterQt *requestContext)
    : m_requestContextGetter(requestContext)
{
}

int NetworkDelegateQt::OnBeforeURLRequest(net::URLRequest *request, const net::CompletionCallback &callback, GURL *newUrl)
{
    Q_ASSERT(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    Q_ASSERT(m_requestContextGetter);

    const content::ResourceRequestInfo *resourceInfo = content::ResourceRequestInfo::ForRequest(request);

    content::ResourceType resourceType = content::RESOURCE_TYPE_LAST_TYPE;
    WebContentsAdapterClient::NavigationType navigationType = WebContentsAdapterClient::OtherNavigation;

    if (resourceInfo) {
        resourceType = resourceInfo->GetResourceType();
        navigationType = pageTransitionToNavigationType(resourceInfo->GetPageTransition());
    }

    const QUrl qUrl = toQt(request->url());

    QWebEngineUrlRequestInterceptor* interceptor = m_requestContextGetter->m_requestInterceptor;
    if (interceptor) {
        QWebEngineUrlRequestInfoPrivate *infoPrivate = new QWebEngineUrlRequestInfoPrivate(toQt(resourceType),
                                                                                           toQt(navigationType),
                                                                                           qUrl,
                                                                                           toQt(request->first_party_for_cookies()),
                                                                                           QByteArray::fromStdString(request->method()));
        QWebEngineUrlRequestInfo requestInfo(infoPrivate);
        interceptor->interceptRequest(requestInfo);
        if (requestInfo.changed()) {
            int result = infoPrivate->shouldBlockRequest ? net::ERR_BLOCKED_BY_CLIENT : net::OK;

            if (qUrl != infoPrivate->url)
                *newUrl = toGurl(infoPrivate->url);

            if (!infoPrivate->extraHeaders.isEmpty()) {
                auto end = infoPrivate->extraHeaders.constEnd();
                for (auto header = infoPrivate->extraHeaders.constBegin(); header != end; ++header)
                    request->SetExtraRequestHeaderByName(header.key().toStdString(), header.value().toStdString(), /* overwrite */ true);
            }

            if (result != net::OK)
                return result;
        }
    }

    if (!resourceInfo)
        return net::OK;

    int renderProcessId;
    int renderFrameId;
    // Only intercept MAIN_FRAME and SUB_FRAME with an associated render frame.
    if (!content::IsResourceTypeFrame(resourceType) || !resourceInfo->GetRenderFrameForRequest(request, &renderProcessId, &renderFrameId))
        return net::OK;

    // Track active requests since |callback| and |new_url| are valid
    // only until OnURLRequestDestroyed is called for this request.
    m_activeRequests.insert(request);

    RequestParams params = {
        qUrl,
        resourceInfo->IsMainFrame(),
        navigationType,
        renderProcessId,
        renderFrameId
    };

    content::BrowserThread::PostTask(
                content::BrowserThread::UI,
                FROM_HERE,
                base::Bind(&NetworkDelegateQt::NotifyNavigationRequestedOnUIThread,
                           base::Unretained(this),
                           request,
                           params,
                           callback)
                );

    // We'll run the callback after we notified the UI thread.
    return net::ERR_IO_PENDING;
}

void NetworkDelegateQt::OnURLRequestDestroyed(net::URLRequest* request)
{
    m_activeRequests.remove(request);
}

void NetworkDelegateQt::CompleteURLRequestOnIOThread(net::URLRequest *request,
                                                     int navigationRequestAction,
                                                     const net::CompletionCallback &callback)
{
    Q_ASSERT(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    if (!m_activeRequests.contains(request))
        return;

    if (request->status().status() == net::URLRequestStatus::CANCELED)
        return;

    int error = net::OK;
    switch (navigationRequestAction) {
    case WebContentsAdapterClient::AcceptRequest:
        error = net::OK;
        break;
    case WebContentsAdapterClient::IgnoreRequest:
        error = net::ERR_ABORTED;
        break;
    default:
        error = net::ERR_FAILED;
        Q_UNREACHABLE();
    }
    callback.Run(error);
}

void NetworkDelegateQt::NotifyNavigationRequestedOnUIThread(net::URLRequest *request,
                                                            RequestParams params,
                                                            const net::CompletionCallback &callback)
{
    Q_ASSERT(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    int navigationRequestAction = WebContentsAdapterClient::AcceptRequest;
    content::RenderFrameHost *rfh = content::RenderFrameHost::FromID(params.renderProcessId, params.renderFrameId);

    if (rfh) {
        content::WebContents *webContents = content::WebContents::FromRenderViewHost(rfh->GetRenderViewHost());
        WebContentsAdapterClient *client = WebContentsViewQt::from(static_cast<content::WebContentsImpl*>(webContents)->GetView())->client();
        client->navigationRequested(params.navigationType, params.url, navigationRequestAction, params.isMainFrameRequest);
    }

    // Run the callback on the IO thread.
    content::BrowserThread::PostTask(
                content::BrowserThread::IO,
                FROM_HERE,
                base::Bind(&NetworkDelegateQt::CompleteURLRequestOnIOThread,
                           base::Unretained(this),
                           request,
                           navigationRequestAction,
                           callback)
                );
}

bool NetworkDelegateQt::OnCanSetCookie(const net::URLRequest& request,
                                       const std::string& cookie_line,
                                       net::CookieOptions*)
{
    Q_ASSERT(m_requestContextGetter);
    return m_requestContextGetter->m_cookieDelegate->canSetCookie(toQt(request.first_party_for_cookies()), QByteArray::fromStdString(cookie_line), toQt(request.url()));
}


int NetworkDelegateQt::OnBeforeStartTransaction(net::URLRequest *request, const net::CompletionCallback &callback, net::HttpRequestHeaders *headers)
{
    return net::OK;
}

void NetworkDelegateQt::OnBeforeSendHeaders(net::URLRequest* request, const net::ProxyInfo& proxy_info,
                                            const net::ProxyRetryInfoMap& proxy_retry_info, net::HttpRequestHeaders* headers)
{
}

void NetworkDelegateQt::OnStartTransaction(net::URLRequest *request, const net::HttpRequestHeaders &headers)
{
}

int NetworkDelegateQt::OnHeadersReceived(net::URLRequest*, const net::CompletionCallback&, const net::HttpResponseHeaders*, scoped_refptr<net::HttpResponseHeaders>*, GURL*)
{
    return net::OK;
}

void NetworkDelegateQt::OnBeforeRedirect(net::URLRequest*, const GURL&)
{
}

void NetworkDelegateQt::OnResponseStarted(net::URLRequest*)
{
}

void NetworkDelegateQt::OnNetworkBytesReceived(net::URLRequest*, int64_t)
{
}

void NetworkDelegateQt::OnNetworkBytesSent(net::URLRequest*, int64_t)
{
}

void NetworkDelegateQt::OnCompleted(net::URLRequest*, bool)
{
}

void NetworkDelegateQt::OnPACScriptError(int, const base::string16&)
{
}

net::NetworkDelegate::AuthRequiredResponse NetworkDelegateQt::OnAuthRequired(net::URLRequest*, const net::AuthChallengeInfo&, const AuthCallback&, net::AuthCredentials*)
{
    return AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool NetworkDelegateQt::OnCanGetCookies(const net::URLRequest&, const net::CookieList&)
{
    return true;
}

bool NetworkDelegateQt::OnCanAccessFile(const net::URLRequest& request, const base::FilePath& path) const
{
    return true;
}

bool NetworkDelegateQt::OnCanEnablePrivacyMode(const GURL&, const GURL&) const
{
    return false;
}

bool NetworkDelegateQt::OnAreExperimentalCookieFeaturesEnabled() const
{
    return false;
}

bool NetworkDelegateQt::OnAreStrictSecureCookiesEnabled() const
{
    return false;
}

bool NetworkDelegateQt::OnCancelURLRequestWithPolicyViolatingReferrerHeader(const net::URLRequest&, const GURL&, const GURL&) const
{
    return false;
}

} // namespace QtWebEngineCore
