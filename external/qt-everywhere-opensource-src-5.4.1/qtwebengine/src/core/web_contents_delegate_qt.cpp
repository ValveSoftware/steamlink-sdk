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

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_contents_delegate_qt.h"

#include "media_capture_devices_dispatcher.h"
#include "type_conversion.h"
#include "web_contents_adapter_client.h"
#include "web_contents_adapter_p.h"
#include "web_engine_context.h"
#include "web_engine_settings.h"
#include "web_engine_visited_links_manager.h"

#include "content/public/browser/favicon_status.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/favicon_url.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/url_constants.h"
#include "webkit/common/webpreferences.h"

// Maps the LogSeverity defines in base/logging.h to the web engines message levels.
static WebContentsAdapterClient::JavaScriptConsoleMessageLevel mapToJavascriptConsoleMessageLevel(int32 messageLevel) {
    if (messageLevel < 1)
        return WebContentsAdapterClient::Info;
    else if (messageLevel > 1)
        return WebContentsAdapterClient::Error;

    return WebContentsAdapterClient::Warning;
}

WebContentsDelegateQt::WebContentsDelegateQt(content::WebContents *webContents, WebContentsAdapterClient *adapterClient)
    : m_viewClient(adapterClient)
    , m_lastReceivedFindReply(0)
{
    webContents->SetDelegate(this);
    Observe(webContents);
}

content::WebContents *WebContentsDelegateQt::OpenURLFromTab(content::WebContents *source, const content::OpenURLParams &params)
{
    content::WebContents *target = source;
    if (params.disposition != CURRENT_TAB) {
        WebContentsAdapter *targetAdapter = createWindow(0, params.disposition, gfx::Rect(), params.user_gesture);
        if (targetAdapter)
            target = targetAdapter->d_func()->webContents.get();
    }

    content::NavigationController::LoadURLParams load_url_params(params.url);
    load_url_params.referrer = params.referrer;
    load_url_params.frame_tree_node_id = params.frame_tree_node_id;
    load_url_params.transition_type = params.transition;
    load_url_params.extra_headers = params.extra_headers;
    load_url_params.should_replace_current_entry = params.should_replace_current_entry;
    load_url_params.is_renderer_initiated = params.is_renderer_initiated;

    if (params.transferred_global_request_id != content::GlobalRequestID())
        load_url_params.transferred_global_request_id = params.transferred_global_request_id;

    target->GetController().LoadURLWithParams(load_url_params);
    return target;
}

void WebContentsDelegateQt::NavigationStateChanged(const content::WebContents* source, unsigned changed_flags)
{
    if (changed_flags & content::INVALIDATE_TYPE_URL)
        m_viewClient->urlChanged(toQt(source->GetVisibleURL()));
    if (changed_flags & content::INVALIDATE_TYPE_TITLE)
        m_viewClient->titleChanged(toQt(source->GetTitle()));
}

void WebContentsDelegateQt::AddNewContents(content::WebContents* source, content::WebContents* new_contents, WindowOpenDisposition disposition, const gfx::Rect& initial_pos, bool user_gesture, bool* was_blocked)
{
    WebContentsAdapter *newAdapter = createWindow(new_contents, disposition, initial_pos, user_gesture);
    if (was_blocked)
        *was_blocked = !newAdapter;
}

void WebContentsDelegateQt::CloseContents(content::WebContents *source)
{
    m_viewClient->close();
    GetJavaScriptDialogManager()->CancelActiveAndPendingDialogs(source);
}

void WebContentsDelegateQt::LoadProgressChanged(content::WebContents* source, double progress)
{
    if (!m_loadingErrorFrameList.isEmpty())
        return;
    m_viewClient->loadProgressChanged(qRound(progress * 100));
}

void WebContentsDelegateQt::DidStartProvisionalLoadForFrame(int64 frame_id, int64 parent_frame_id, bool is_main_frame, const GURL &validated_url, bool isErrorPage, bool, content::RenderViewHost*)
{
    if (isErrorPage) {
        m_loadingErrorFrameList.append(frame_id);
        return;
    }

    if (!is_main_frame)
        return;

    m_loadingErrorFrameList.clear();
    m_viewClient->loadStarted(toQt(validated_url));
}

void WebContentsDelegateQt::DidCommitProvisionalLoadForFrame(int64 frame_id, const base::string16& frame_unique_name, bool is_main_frame, const GURL& url, content::PageTransition transition_type, content::RenderViewHost* render_view_host)
{
    // Make sure that we don't set the findNext WebFindOptions on a new frame.
    m_lastSearchedString = QString();

    // This is currently used for canGoBack/Forward values, which is flattened across frames. For other purposes we might have to pass is_main_frame.
    m_viewClient->loadCommitted();
}

void WebContentsDelegateQt::DidFailProvisionalLoad(int64 frame_id, const base::string16& frame_unique_name, bool is_main_frame, const GURL& validated_url, int error_code, const base::string16& error_description, content::RenderViewHost* render_view_host)
{
    DidFailLoad(frame_id, validated_url, is_main_frame, error_code, error_description, render_view_host);
}

void WebContentsDelegateQt::DidFailLoad(int64 frame_id, const GURL &validated_url, bool is_main_frame, int error_code, const base::string16 &error_description, content::RenderViewHost *rvh)
{
    if (m_loadingErrorFrameList.removeOne(frame_id) || !is_main_frame)
        return;

    m_viewClient->loadFinished(false, toQt(validated_url), error_code, toQt(error_description));
    m_viewClient->loadProgressChanged(0);
}

void WebContentsDelegateQt::DidFinishLoad(int64 frame_id, const GURL &url, bool is_main_frame, content::RenderViewHost*)
{
    if (m_loadingErrorFrameList.removeOne(frame_id)) {
        Q_ASSERT(url.is_valid() && url.spec() == content::kUnreachableWebDataURL);
        m_viewClient->iconChanged(QUrl());
        return;
    }

    if (!is_main_frame)
        return;

    m_viewClient->loadFinished(true, toQt(url));

    content::NavigationEntry *entry = web_contents()->GetController().GetActiveEntry();
    if (!entry)
        return;
    content::FaviconStatus &favicon = entry->GetFavicon();
    if (favicon.valid)
        m_viewClient->iconChanged(toQt(favicon.url));
    else
        m_viewClient->iconChanged(QUrl());
}

void WebContentsDelegateQt::DidUpdateFaviconURL(const std::vector<content::FaviconURL>& candidates)
{
    Q_FOREACH (content::FaviconURL candidate, candidates) {
        if (candidate.icon_type == content::FaviconURL::FAVICON && !candidate.icon_url.is_empty()) {
            content::NavigationEntry *entry = web_contents()->GetController().GetActiveEntry();
            if (!entry)
                continue;
            content::FaviconStatus &favicon = entry->GetFavicon();
            favicon.url = candidate.icon_url;
            favicon.valid = toQt(candidate.icon_url).isValid();
            break;
        }
    }
}

content::JavaScriptDialogManager *WebContentsDelegateQt::GetJavaScriptDialogManager()
{
    return JavaScriptDialogManagerQt::GetInstance();
}

void WebContentsDelegateQt::ToggleFullscreenModeForTab(content::WebContents* web_contents, bool enter_fullscreen)
{
    if (m_viewClient->isFullScreen() != enter_fullscreen) {
        m_viewClient->requestFullScreen(enter_fullscreen);
        web_contents->GetRenderViewHost()->WasResized();
    }
}

bool WebContentsDelegateQt::IsFullscreenForTabOrPending(const content::WebContents* web_contents) const
{
    return m_viewClient->isFullScreen();
}

Q_STATIC_ASSERT_X(static_cast<int>(WebContentsAdapterClient::Open) == static_cast<int>(content::FileChooserParams::Open), "Enums out of sync");
Q_STATIC_ASSERT_X(static_cast<int>(WebContentsAdapterClient::Save) == static_cast<int>(content::FileChooserParams::Save), "Enums out of sync");

void WebContentsDelegateQt::RunFileChooser(content::WebContents *web_contents, const content::FileChooserParams &params)
{
    Q_UNUSED(web_contents)
    QStringList acceptedMimeTypes;
    acceptedMimeTypes.reserve(params.accept_types.size());
    for (std::vector<base::string16>::const_iterator it = params.accept_types.begin(); it < params.accept_types.end(); ++it)
        acceptedMimeTypes.append(toQt(*it));

    m_viewClient->runFileChooser(static_cast<WebContentsAdapterClient::FileChooserMode>(params.mode), toQt(params.default_file_name.value()), acceptedMimeTypes);
}

bool WebContentsDelegateQt::AddMessageToConsole(content::WebContents *source, int32 level, const base::string16 &message, int32 line_no, const base::string16 &source_id)
{
    Q_UNUSED(source)
    m_viewClient->javaScriptConsoleMessage(mapToJavascriptConsoleMessageLevel(level), toQt(message), static_cast<int>(line_no), toQt(source_id));
    return false;
}

void WebContentsDelegateQt::FindReply(content::WebContents *source, int request_id, int number_of_matches, const gfx::Rect& selection_rect, int active_match_ordinal, bool final_update)
{
    Q_UNUSED(source)
    Q_UNUSED(selection_rect)
    Q_UNUSED(active_match_ordinal)
    if (final_update) {
        m_lastReceivedFindReply = request_id;
        m_viewClient->didFindText(request_id, number_of_matches);
    }
}

void WebContentsDelegateQt::RequestMediaAccessPermission(content::WebContents *web_contents, const content::MediaStreamRequest &request, const content::MediaResponseCallback &callback)
{
    MediaCaptureDevicesDispatcher::GetInstance()->processMediaAccessRequest(m_viewClient, web_contents, request, callback);
}

void WebContentsDelegateQt::UpdateTargetURL(content::WebContents *source, int32 page_id, const GURL &url)
{
    Q_UNUSED(source)
    Q_UNUSED(page_id)
    m_viewClient->didUpdateTargetURL(toQt(url));
}

void WebContentsDelegateQt::DidNavigateAnyFrame(const content::LoadCommittedDetails &, const content::FrameNavigateParams &params)
{
    if (!params.should_update_history)
        return;
    WebEngineContext::current()->visitedLinksManager()->addUrl(params.url);
}


void WebContentsDelegateQt::overrideWebPreferences(content::WebContents *, WebPreferences *webPreferences)
{
    m_viewClient->webEngineSettings()->overrideWebPreferences(webPreferences);
}

WebContentsAdapter *WebContentsDelegateQt::createWindow(content::WebContents *new_contents, WindowOpenDisposition disposition, const gfx::Rect& initial_pos, bool user_gesture)
{
    WebContentsAdapter *newAdapter = new WebContentsAdapter(new_contents);
    // Do the first ref-count manually to be able to know if the application is handling adoptNewWindow through the public API.
    newAdapter->ref.ref();

    m_viewClient->adoptNewWindow(newAdapter, static_cast<WebContentsAdapterClient::WindowOpenDisposition>(disposition), user_gesture, toQt(initial_pos));

    if (!newAdapter->ref.deref()) {
        // adoptNewWindow didn't increase the ref-count, newAdapter and its new_contents (if non-null) need to be discarded.
        delete newAdapter;
        newAdapter = 0;
    }

    return newAdapter;
}

void WebContentsDelegateQt::allowCertificateError(const QExplicitlySharedDataPointer<CertificateErrorController> &errorController)
{
    m_viewClient->allowCertificateError(errorController);
}
