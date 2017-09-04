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

#ifndef NETWORK_DELEGATE_QT_H
#define NETWORK_DELEGATE_QT_H

#include "net/base/network_delegate.h"
#include "net/base/net_errors.h"

#include <QUrl>
#include <QSet>

namespace QtWebEngineCore {

class URLRequestContextGetterQt;

class NetworkDelegateQt : public net::NetworkDelegate {
    QSet<net::URLRequest *> m_activeRequests;
    URLRequestContextGetterQt *m_requestContextGetter;
public:
    NetworkDelegateQt(URLRequestContextGetterQt *requestContext);

    struct RequestParams {
        QUrl url;
        bool isMainFrameRequest;
        int navigationType;
        int renderProcessId;
        int renderFrameId;
    };

    void NotifyNavigationRequestedOnUIThread(net::URLRequest *request,
                                             RequestParams params,
                                             const net::CompletionCallback &callback);

    void CompleteURLRequestOnIOThread(net::URLRequest *request,
                                      int navigationRequestAction,
                                      const net::CompletionCallback &callback);

    // net::NetworkDelegate implementation
    virtual int OnBeforeURLRequest(net::URLRequest* request, const net::CompletionCallback& callback, GURL* newUrl) override;
    virtual void OnURLRequestDestroyed(net::URLRequest* request) override;
    virtual bool OnCanSetCookie(const net::URLRequest&, const std::string&, net::CookieOptions*) override;
    virtual int OnBeforeStartTransaction(net::URLRequest *request, const net::CompletionCallback &callback, net::HttpRequestHeaders *headers) override;
    virtual void OnBeforeSendHeaders(net::URLRequest* request, const net::ProxyInfo& proxy_info,
                                     const net::ProxyRetryInfoMap& proxy_retry_info, net::HttpRequestHeaders* headers) override;
    virtual void OnStartTransaction(net::URLRequest *request, const net::HttpRequestHeaders &headers) override;
    virtual int OnHeadersReceived(net::URLRequest*, const net::CompletionCallback&, const net::HttpResponseHeaders*, scoped_refptr<net::HttpResponseHeaders>*, GURL*) override;
    virtual void OnBeforeRedirect(net::URLRequest*, const GURL&) override;
    virtual void OnResponseStarted(net::URLRequest*) override;
    virtual void OnNetworkBytesReceived(net::URLRequest*, int64_t) override;
    virtual void OnNetworkBytesSent(net::URLRequest *, int64_t) override;
    virtual void OnCompleted(net::URLRequest*, bool) override;
    virtual void OnPACScriptError(int, const base::string16&) override;
    virtual net::NetworkDelegate::AuthRequiredResponse OnAuthRequired(net::URLRequest*, const net::AuthChallengeInfo&, const AuthCallback&, net::AuthCredentials*) override;
    virtual bool OnCanGetCookies(const net::URLRequest&, const net::CookieList&) override;
    virtual bool OnCanAccessFile(const net::URLRequest& request, const base::FilePath& path) const override;
    virtual bool OnCanEnablePrivacyMode(const GURL&, const GURL&) const override;
    virtual bool OnAreExperimentalCookieFeaturesEnabled() const override;
    virtual bool OnAreStrictSecureCookiesEnabled() const override;
    virtual bool OnCancelURLRequestWithPolicyViolatingReferrerHeader(const net::URLRequest&, const GURL&, const GURL&) const override;
};

} // namespace QtWebEngineCore

#endif // NETWORK_DELEGATE_QT_H
