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

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "resource_dispatcher_host_delegate_qt.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request.h"

#include "authentication_dialog_controller.h"
#include "authentication_dialog_controller_p.h"
#include "type_conversion.h"
#include "web_contents_view_qt.h"

namespace QtWebEngineCore {

ResourceDispatcherHostLoginDelegateQt::ResourceDispatcherHostLoginDelegateQt(net::AuthChallengeInfo *authInfo, net::URLRequest *request)
    : m_authInfo(authInfo)
    , m_request(request)
{
    Q_ASSERT(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    content::ResourceRequestInfo::GetRenderFrameForRequest(request, &m_renderProcessId,  &m_renderFrameId);

    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&ResourceDispatcherHostLoginDelegateQt::triggerDialog, this));
}

ResourceDispatcherHostLoginDelegateQt::~ResourceDispatcherHostLoginDelegateQt()
{
    Q_ASSERT(m_dialogController.isNull());
    // We must have called ClearLoginDelegateForRequest if we didn't receive an OnRequestCancelled.
    Q_ASSERT(!m_request);
}

void ResourceDispatcherHostLoginDelegateQt::OnRequestCancelled()
{
    destroy();
}

QUrl ResourceDispatcherHostLoginDelegateQt::url() const
{
    return toQt(m_request->url());
}

QString ResourceDispatcherHostLoginDelegateQt::realm() const
{
    return QString::fromStdString(m_authInfo->realm);
}

QString ResourceDispatcherHostLoginDelegateQt::host() const
{
    return QString::fromStdString(m_authInfo->challenger.host());
}

bool ResourceDispatcherHostLoginDelegateQt::isProxy() const
{
    return m_authInfo->is_proxy;
}

void ResourceDispatcherHostLoginDelegateQt::triggerDialog()
{
    Q_ASSERT(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    content::RenderFrameHost *renderFrameHost = content::RenderFrameHost::FromID(m_renderProcessId, m_renderFrameId);
    if (!renderFrameHost)
        return;
    content::RenderViewHost *renderViewHost = renderFrameHost->GetRenderViewHost();
    content::WebContentsImpl *webContents = static_cast<content::WebContentsImpl *>(content::WebContents::FromRenderViewHost(renderViewHost));
    WebContentsAdapterClient *client = WebContentsViewQt::from(webContents->GetView())->client();

    AuthenticationDialogControllerPrivate *dialogControllerData = new AuthenticationDialogControllerPrivate(this);
    m_dialogController.reset(new AuthenticationDialogController(dialogControllerData));
    client->authenticationRequired(m_dialogController);
}

void ResourceDispatcherHostLoginDelegateQt::sendAuthToRequester(bool success, const QString &user, const QString &password)
{
    Q_ASSERT(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    if (!m_request)
        return;

    if (success)
        m_request->SetAuth(net::AuthCredentials(toString16(user), toString16(password)));
    else
        m_request->CancelAuth();
    content::ResourceDispatcherHost::Get()->ClearLoginDelegateForRequest(m_request);

    destroy();
}

void ResourceDispatcherHostLoginDelegateQt::destroy()
{
    m_dialogController.reset();
    m_request = 0;
}

static void LaunchURL(const GURL& url, int render_process_id,
                      const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
                      ui::PageTransition page_transition, bool is_main_frame)
{
    Q_UNUSED(render_process_id);
    Q_ASSERT(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    content::WebContents* webContents = web_contents_getter.Run();
    if (!webContents)
        return;
    WebContentsDelegateQt *contentsDelegate = static_cast<WebContentsDelegateQt*>(webContents->GetDelegate());
    contentsDelegate->launchExternalURL(toQt(url), page_transition, is_main_frame);
}

bool ResourceDispatcherHostDelegateQt::HandleExternalProtocol(const GURL& url, int child_id,
                                                              const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
                                                              bool is_main_frame, ui::PageTransition page_transition, bool has_user_gesture,
                                                              content::ResourceContext* /*resource_context*/)
{
    Q_ASSERT(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    // We don't want to launch external applications unless it is based on a user action
    if (!has_user_gesture)
        return false;

    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&LaunchURL, url, child_id, web_contents_getter, page_transition, is_main_frame));
    return true;
}

content::ResourceDispatcherHostLoginDelegate *ResourceDispatcherHostDelegateQt::CreateLoginDelegate(net::AuthChallengeInfo *authInfo, net::URLRequest *request)
{
    // ResourceDispatcherHostLoginDelegateQt is ref-counted and will be released after we called ClearLoginDelegateForRequest.
    return new ResourceDispatcherHostLoginDelegateQt(authInfo, request);
}

} // namespace QtWebEngineCore
