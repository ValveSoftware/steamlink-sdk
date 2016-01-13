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

#ifndef WEB_CONTENTS_DELEGATE_QT_H
#define WEB_CONTENTS_DELEGATE_QT_H

#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

#include "javascript_dialog_manager_qt.h"
#include <QtCore/qcompilerdetection.h>

namespace content {
    class BrowserContext;
    class SiteInstance;
    class RenderViewHost;
    class JavaScriptDialogManager;
    class WebContents;
}

struct WebPreferences;
class WebContentsAdapterClient;
class CertificateErrorController;

class WebContentsDelegateQt : public content::WebContentsDelegate
                            , public content::WebContentsObserver
{
public:
    WebContentsDelegateQt(content::WebContents*, WebContentsAdapterClient *adapterClient);
    ~WebContentsDelegateQt() { Q_ASSERT(m_loadingErrorFrameList.isEmpty()); }
    QString lastSearchedString() const { return m_lastSearchedString; }
    void setLastSearchedString(const QString &s) { m_lastSearchedString = s; }
    int lastReceivedFindReply() const { return m_lastReceivedFindReply; }

    virtual content::WebContents *OpenURLFromTab(content::WebContents *source, const content::OpenURLParams &params) Q_DECL_OVERRIDE;
    virtual void NavigationStateChanged(const content::WebContents* source, unsigned changed_flags) Q_DECL_OVERRIDE;
    virtual void AddNewContents(content::WebContents* source, content::WebContents* new_contents, WindowOpenDisposition disposition, const gfx::Rect& initial_pos, bool user_gesture, bool* was_blocked) Q_DECL_OVERRIDE;
    virtual void CloseContents(content::WebContents *source) Q_DECL_OVERRIDE;
    virtual void LoadProgressChanged(content::WebContents* source, double progress) Q_DECL_OVERRIDE;
    virtual void DidStartProvisionalLoadForFrame(int64 frame_id, int64 parent_frame_id, bool is_main_frame, const GURL &validated_url, bool is_error_page, bool is_iframe_srcdoc, content::RenderViewHost *render_view_host) Q_DECL_OVERRIDE;
    virtual void DidCommitProvisionalLoadForFrame(int64 frame_id, const base::string16& frame_unique_name, bool is_main_frame, const GURL& url, content::PageTransition transition_type, content::RenderViewHost* render_view_host) Q_DECL_OVERRIDE;
    virtual void DidFailProvisionalLoad(int64 frame_id, const base::string16& frame_unique_name, bool is_main_frame, const GURL& validated_url, int error_code, const base::string16& error_description, content::RenderViewHost* render_view_host) Q_DECL_OVERRIDE;
    virtual void DidFailLoad(int64 frame_id, const GURL &validated_url, bool is_main_frame, int error_code, const base::string16 &error_description, content::RenderViewHost *render_view_host) Q_DECL_OVERRIDE;
    virtual void DidFinishLoad(int64 frame_id, const GURL &validated_url, bool is_main_frame, content::RenderViewHost *render_view_host) Q_DECL_OVERRIDE;
    virtual void DidUpdateFaviconURL(const std::vector<content::FaviconURL>& candidates) Q_DECL_OVERRIDE;
    virtual content::JavaScriptDialogManager *GetJavaScriptDialogManager() Q_DECL_OVERRIDE;
    virtual void ToggleFullscreenModeForTab(content::WebContents* web_contents, bool enter_fullscreen) Q_DECL_OVERRIDE;
    virtual bool IsFullscreenForTabOrPending(const content::WebContents* web_contents) const Q_DECL_OVERRIDE;
    virtual void RunFileChooser(content::WebContents *, const content::FileChooserParams &params) Q_DECL_OVERRIDE;
    virtual bool AddMessageToConsole(content::WebContents* source, int32 level, const base::string16& message, int32 line_no, const base::string16& source_id) Q_DECL_OVERRIDE;
    virtual void FindReply(content::WebContents *source, int request_id, int number_of_matches, const gfx::Rect& selection_rect, int active_match_ordinal, bool final_update) Q_DECL_OVERRIDE;
    virtual void RequestMediaAccessPermission(content::WebContents* web_contents, const content::MediaStreamRequest& request, const content::MediaResponseCallback& callback) Q_DECL_OVERRIDE;
    virtual void UpdateTargetURL(content::WebContents *source, int32 page_id, const GURL &url) Q_DECL_OVERRIDE;
    virtual void DidNavigateAnyFrame(const content::LoadCommittedDetails&, const content::FrameNavigateParams& params) Q_DECL_OVERRIDE;

    void overrideWebPreferences(content::WebContents *, WebPreferences*);
    void allowCertificateError(const QExplicitlySharedDataPointer<CertificateErrorController> &) ;

private:
    WebContentsAdapter *createWindow(content::WebContents *new_contents, WindowOpenDisposition disposition, const gfx::Rect& initial_pos, bool user_gesture);

    WebContentsAdapterClient *m_viewClient;
    QString m_lastSearchedString;
    int m_lastReceivedFindReply;
    QList<int64> m_loadingErrorFrameList;
};

#endif // WEB_CONTENTS_DELEGATE_QT_H
