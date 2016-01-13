// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_CONTROLLER_DELEGATE_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_CONTROLLER_DELEGATE_H_

#include <string>
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"

namespace content {

struct LoadCommittedDetails;
struct LoadNotificationDetails;
struct NativeWebKeyboardEvent;
class InterstitialPage;
class InterstitialPageImpl;
class RenderFrameHost;
class RenderViewHost;
class SiteInstance;
class WebContents;
class WebContentsDelegate;

// Interface for objects embedding a NavigationController to provide the
// functionality NavigationController needs.
// TODO(nasko): This interface should exist for short amount of time, while
// we transition navigation code from WebContents to Navigator.
class NavigationControllerDelegate {
 public:
  virtual ~NavigationControllerDelegate() {}

  // Duplicates of WebContents methods.
  virtual RenderViewHost* GetRenderViewHost() const = 0;
  virtual InterstitialPage* GetInterstitialPage() const = 0;
  virtual const std::string& GetContentsMimeType() const = 0;
  virtual void NotifyNavigationStateChanged(unsigned changed_flags) = 0;
  virtual void Stop() = 0;
  virtual SiteInstance* GetPendingSiteInstance() const = 0;
  virtual int32 GetMaxPageID() = 0;
  virtual int32 GetMaxPageIDForSiteInstance(SiteInstance* site_instance) = 0;
  virtual bool IsLoading() const = 0;
  virtual bool IsBeingDestroyed() const = 0;
  virtual bool CanOverscrollContent() const = 0;

  // Methods from WebContentsImpl that NavigationControllerImpl needs to
  // call.
  virtual void NotifyBeforeFormRepostWarningShow() = 0;
  virtual void NotifyNavigationEntryCommitted(
      const LoadCommittedDetails& load_details) = 0;
  virtual bool NavigateToPendingEntry(
      NavigationController::ReloadType reload_type) = 0;
  virtual void SetHistoryLengthAndPrune(
      const SiteInstance* site_instance,
      int merge_history_length,
      int32 minimum_page_id) = 0;
  virtual void CopyMaxPageIDsFrom(WebContents* web_contents) = 0;
  virtual void UpdateMaxPageID(int32 page_id) = 0;
  virtual void UpdateMaxPageIDForSiteInstance(SiteInstance* site_instance,
                                              int32 page_id) = 0;
  virtual void ActivateAndShowRepostFormWarningDialog() = 0;
  virtual bool HasAccessedInitialDocument() = 0;

  // This method is needed, since we are no longer guaranteed that the
  // embedder for NavigationController will be a WebContents object.
  virtual WebContents* GetWebContents() = 0;

  // Methods needed by InterstitialPageImpl.
  virtual bool IsHidden() = 0;
  virtual void RenderFrameForInterstitialPageCreated(
      RenderFrameHost* render_frame_host) = 0;
  virtual void AttachInterstitialPage(
      InterstitialPageImpl* interstitial_page) = 0;
  virtual void DetachInterstitialPage() = 0;
  virtual void SetIsLoading(RenderViewHost* render_view_host,
                            bool is_loading,
                            bool to_different_document,
                            LoadNotificationDetails* details) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_CONTROLLER_DELEGATE_H_
