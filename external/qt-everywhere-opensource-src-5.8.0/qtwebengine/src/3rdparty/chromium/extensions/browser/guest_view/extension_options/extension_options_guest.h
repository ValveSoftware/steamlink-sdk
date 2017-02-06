// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/guest_view/browser/guest_view.h"
#include "extensions/browser/guest_view/extension_options/extension_options_guest_delegate.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionOptionsGuest
    : public guest_view::GuestView<ExtensionOptionsGuest> {
 public:
  static const char Type[];
  static guest_view::GuestViewBase* Create(
      content::WebContents* owner_web_contents);

 private:
  explicit ExtensionOptionsGuest(content::WebContents* owner_web_contents);
  ~ExtensionOptionsGuest() override;

  // GuestViewBase implementation.
  bool CanRunInDetachedState() const final;
  void CreateWebContents(const base::DictionaryValue& create_params,
                         const WebContentsCreatedCallback& callback) final;
  void DidInitialize(const base::DictionaryValue& create_params) final;
  void GuestViewDidStopLoading() final;
  const char* GetAPINamespace() const final;
  int GetTaskPrefix() const final;
  bool IsPreferredSizeModeEnabled() const final;
  void OnPreferredSizeChanged(const gfx::Size& pref_size) final;
  bool ShouldHandleFindRequestsForEmbedder() const final;

  // content::WebContentsDelegate implementation.
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) final;
  void CloseContents(content::WebContents* source) final;
  bool HandleContextMenu(const content::ContextMenuParams& params) final;
  bool ShouldCreateWebContents(
      content::WebContents* web_contents,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      WindowContainerType window_container_type,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) final;

  // content::WebContentsObserver implementation.
  void DidNavigateMainFrame(const content::LoadCommittedDetails& details,
                            const content::FrameNavigateParams& params) final;

  std::unique_ptr<extensions::ExtensionOptionsGuestDelegate>
      extension_options_guest_delegate_;
  GURL options_page_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionOptionsGuest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_H_
