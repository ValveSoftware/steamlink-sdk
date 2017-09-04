// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_VISIBLE_PASSWORD_OBSERVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_VISIBLE_PASSWORD_OBSERVER_H_

#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace password_manager {

// This class tracks password visibility notifications for the
// RenderFrameHosts in a WebContents. There is one
// VisiblePasswordObserver per WebContents. When a RenderFrameHost has a
// visible password field and the top-level URL is HTTP, the
// VisiblePasswordObserver notifies the WebContents, which allows the
// content embedder to adjust security UI. Similarly, when all
// RenderFrameHosts have hidden their password fields (either because
// the renderer sent a message reporting that all password fields are
// gone, or because the renderer crashed), the WebContents is notified
// so that security warnings can be removed by the embedder.
//
// The user of this class is responsible for listening for messages from
// the renderer about password visibility and notifying the
// VisiblePasswordObserver about them. The VisiblePasswordObserver
// itself observes renderer process crashes. (Note that listening for
// navigations explicitly is not required because on navigation the
// renderer will send messages as password fields are removed.)
class VisiblePasswordObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<VisiblePasswordObserver> {
 public:
  ~VisiblePasswordObserver() override;

  // This method should be called when there is a message notifying
  // the browser process that the renderer has a visible password
  // field.
  void RenderFrameHasVisiblePasswordField(
      content::RenderFrameHost* render_frame_host);

  // This method should be called when there is a message notifying the browser
  // process that the renderer has no visible password fields anymore.
  void RenderFrameHasNoVisiblePasswordFields(
      content::RenderFrameHost* render_frame_host);

  // WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

 private:
  explicit VisiblePasswordObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<VisiblePasswordObserver>;

  void MaybeNotifyPasswordInputShownOnHttp();

  void MaybeNotifyAllFieldsInvisible();

  content::WebContents* web_contents_;
  std::set<content::RenderFrameHost*> frames_with_visible_password_fields_;

  DISALLOW_COPY_AND_ASSIGN(VisiblePasswordObserver);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_VISIBLE_PASSWORD_OBSERVER_H_
