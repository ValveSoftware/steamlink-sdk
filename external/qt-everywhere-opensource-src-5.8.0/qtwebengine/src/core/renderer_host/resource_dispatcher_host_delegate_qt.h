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

#ifndef RESOURCE_DISPATCHER_HOST_DELEGATE_QT_H
#define RESOURCE_DISPATCHER_HOST_DELEGATE_QT_H

#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_dispatcher_host_login_delegate.h"

#include "web_contents_adapter_client.h"

namespace QtWebEngineCore {

class AuthenticationDialogController;

class ResourceDispatcherHostLoginDelegateQt : public content::ResourceDispatcherHostLoginDelegate {
public:
    ResourceDispatcherHostLoginDelegateQt(net::AuthChallengeInfo *authInfo, net::URLRequest *request);
    ~ResourceDispatcherHostLoginDelegateQt();

    // ResourceDispatcherHostLoginDelegate implementation
    virtual void OnRequestCancelled();

    QUrl url() const;
    QString realm() const;
    QString host() const;
    bool isProxy() const;

    void sendAuthToRequester(bool success, const QString &user, const QString &password);

private:
    void triggerDialog();
    void destroy();

    QUrl m_url;
    QString m_realm;
    bool m_isProxy;
    QString m_host;

    int m_renderProcessId;
    int m_renderFrameId;

    scoped_refptr<net::AuthChallengeInfo> m_authInfo;

    // The request that wants login data.
    // Must only be accessed on the IO thread.
    net::URLRequest *m_request;

    // This member is used to keep authentication dialog controller alive until
    // authorization is sent or cancelled.
    QSharedPointer<AuthenticationDialogController> m_dialogController;
};

class ResourceDispatcherHostDelegateQt : public content::ResourceDispatcherHostDelegate {
public:
    bool HandleExternalProtocol(const GURL& url, int child_id,
                                const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
                                bool is_main_frame, ui::PageTransition page_transition, bool has_user_gesture,
                                content::ResourceContext* resource_context) override;

    virtual content::ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(net::AuthChallengeInfo *authInfo, net::URLRequest *request) Q_DECL_OVERRIDE;
};

} // namespace QtWebEngineCore

#endif // RESOURCE_DISPATCHER_HOST_DELEGATE_QT_H
