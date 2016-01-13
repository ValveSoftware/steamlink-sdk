// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_INTERSTITIAL_PAGE_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_INTERSTITIAL_PAGE_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/renderer_preferences.h"
#include "url/gurl.h"

namespace content {
class NavigationEntry;
class NavigationControllerImpl;
class RenderViewHostImpl;
class RenderWidgetHostView;
class WebContentsView;

enum ResourceRequestAction {
  BLOCK,
  RESUME,
  CANCEL
};

class CONTENT_EXPORT InterstitialPageImpl
    : public NON_EXPORTED_BASE(InterstitialPage),
      public NotificationObserver,
      public WebContentsObserver,
      public NON_EXPORTED_BASE(RenderFrameHostDelegate),
      public RenderViewHostDelegate,
      public RenderWidgetHostDelegate,
      public NON_EXPORTED_BASE(NavigatorDelegate) {
 public:
  // The different state of actions the user can take in an interstitial.
  enum ActionState {
    NO_ACTION,           // No action has been taken yet.
    PROCEED_ACTION,      // "Proceed" was selected.
    DONT_PROCEED_ACTION  // "Don't proceed" was selected.
  };

  InterstitialPageImpl(WebContents* web_contents,
                       RenderWidgetHostDelegate* render_widget_host_delegate,
                       bool new_navigation,
                       const GURL& url,
                       InterstitialPageDelegate* delegate);
  virtual ~InterstitialPageImpl();

  // InterstitialPage implementation:
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void DontProceed() OVERRIDE;
  virtual void Proceed() OVERRIDE;
  virtual RenderViewHost* GetRenderViewHostForTesting() const OVERRIDE;
  virtual InterstitialPageDelegate* GetDelegateForTesting() OVERRIDE;
  virtual void DontCreateViewForTesting() OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void Focus() OVERRIDE;

  // Allows the user to navigate away by disabling the interstitial, canceling
  // the pending request, and unblocking the hidden renderer.  The interstitial
  // will stay visible until the navigation completes.
  void CancelForNavigation();

  // Focus the first (last if reverse is true) element in the interstitial page.
  // Called when tab traversing.
  void FocusThroughTabTraversal(bool reverse);

  RenderWidgetHostView* GetView();

  // See description above field.
  void set_reload_on_dont_proceed(bool value) {
    reload_on_dont_proceed_ = value;
  }
  bool reload_on_dont_proceed() const { return reload_on_dont_proceed_; }

#if defined(OS_ANDROID)
  // Android shares a single platform window for all tabs, so we need to expose
  // the RenderViewHost to properly route gestures to the interstitial.
  RenderViewHost* GetRenderViewHost() const;
#endif

  // TODO(nasko): This should move to InterstitialPageNavigatorImpl, but in
  // the meantime make it public, so it can be called directly.
  void DidNavigate(
      RenderViewHost* render_view_host,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params);

 protected:
  // NotificationObserver method:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // WebContentsObserver implementation:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 RenderFrameHost* render_frame_host) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void WebContentsDestroyed() OVERRIDE;
  virtual void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) OVERRIDE;

  // RenderFrameHostDelegate implementation:
  virtual bool OnMessageReceived(RenderFrameHost* render_frame_host,
                                 const IPC::Message& message) OVERRIDE;
  virtual void RenderFrameCreated(RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void UpdateTitle(RenderFrameHost* render_frame_host,
                           int32 page_id,
                           const base::string16& title,
                           base::i18n::TextDirection title_direction) OVERRIDE;

  // RenderViewHostDelegate implementation:
  virtual RenderViewHostDelegateView* GetDelegateView() OVERRIDE;
  virtual bool OnMessageReceived(RenderViewHost* render_view_host,
                                 const IPC::Message& message) OVERRIDE;
  virtual const GURL& GetMainFrameLastCommittedURL() const OVERRIDE;
  virtual void RenderViewTerminated(RenderViewHost* render_view_host,
                                    base::TerminationStatus status,
                                    int error_code) OVERRIDE;
  virtual RendererPreferences GetRendererPrefs(
      BrowserContext* browser_context) const OVERRIDE;
  virtual WebPreferences GetWebkitPrefs() OVERRIDE;
  virtual gfx::Rect GetRootWindowResizerRect() const OVERRIDE;
  virtual void CreateNewWindow(
      int render_process_id,
      int route_id,
      int main_frame_route_id,
      const ViewHostMsg_CreateWindow_Params& params,
      SessionStorageNamespace* session_storage_namespace) OVERRIDE;
  virtual void CreateNewWidget(int render_process_id,
                               int route_id,
                               blink::WebPopupType popup_type) OVERRIDE;
  virtual void CreateNewFullscreenWidget(int render_process_id,
                                         int route_id) OVERRIDE;
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) OVERRIDE;
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) OVERRIDE;
  virtual void ShowCreatedFullscreenWidget(int route_id) OVERRIDE;

  virtual SessionStorageNamespace* GetSessionStorageNamespace(
      SiteInstance* instance) OVERRIDE;

  virtual FrameTree* GetFrameTree() OVERRIDE;

  // RenderWidgetHostDelegate implementation:
  virtual void RenderWidgetDeleted(
      RenderWidgetHostImpl* render_widget_host) OVERRIDE;
  virtual bool PreHandleKeyboardEvent(
      const NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;
#if defined(OS_WIN)
  virtual gfx::NativeViewAccessible GetParentNativeViewAccessible() OVERRIDE;
#endif

  bool enabled() const { return enabled_; }
  WebContents* web_contents() const;
  const GURL& url() const { return url_; }

  // Creates the RenderViewHost containing the interstitial content.
  // Overriden in unit tests.
  virtual RenderViewHost* CreateRenderViewHost();

  // Creates the WebContentsView that shows the interstitial RVH.
  // Overriden in unit tests.
  virtual WebContentsView* CreateWebContentsView();

  // Notification magic.
  NotificationRegistrar notification_registrar_;

 private:
  class InterstitialPageRVHDelegateView;

  // Disable the interstitial:
  // - if it is not yet showing, then it won't be shown.
  // - any command sent by the RenderViewHost will be ignored.
  void Disable();

  // Delete ourselves, causing Shutdown on the RVH to be called.
  void Shutdown();

  void OnNavigatingAwayOrTabClosing();

  // Executes the passed action on the ResourceDispatcher (on the IO thread).
  // Used to block/resume/cancel requests for the RenderViewHost hidden by this
  // interstitial.
  void TakeActionOnResourceDispatcher(ResourceRequestAction action);

  // IPC message handlers.
  void OnDomOperationResponse(const std::string& json_string,
                              int automation_id);

  // The contents in which we are displayed.  This is valid until Hide is
  // called, at which point it will be set to NULL because the WebContents
  // itself may be deleted.
  WebContents* web_contents_;

  // The NavigationController for the content this page is being displayed over.
  NavigationControllerImpl* controller_;

  // Delegate for dispatching keyboard events and accessing the native view.
  RenderWidgetHostDelegate* render_widget_host_delegate_;

  // The URL that is shown when the interstitial is showing.
  GURL url_;

  // Whether this interstitial is shown as a result of a new navigation (in
  // which case a transient navigation entry is created).
  bool new_navigation_;

  // Whether we should discard the pending navigation entry when not proceeding.
  // This is to deal with cases where |new_navigation_| is true but a new
  // pending entry was created since this interstitial was shown and we should
  // not discard it.
  bool should_discard_pending_nav_entry_;

  // If true and the user chooses not to proceed the target NavigationController
  // is reloaded. This is used when two NavigationControllers are merged
  // (CopyStateFromAndPrune).
  // The default is false.
  bool reload_on_dont_proceed_;

  // Whether this interstitial is enabled.  See Disable() for more info.
  bool enabled_;

  // Whether the Proceed or DontProceed methods have been called yet.
  ActionState action_taken_;

  // The RenderViewHost displaying the interstitial contents.  This is valid
  // until Hide is called, at which point it will be set to NULL, signifying
  // that shutdown has started.
  // TODO(creis): This is now owned by the FrameTree.  We should route things
  // through the tree's root RenderFrameHost instead.
  RenderViewHostImpl* render_view_host_;

  // The frame tree structure of the current page.
  FrameTree frame_tree_;

  // The IDs for the Render[View|Process]Host hidden by this interstitial.
  int original_child_id_;
  int original_rvh_id_;

  // Whether or not we should change the title of the contents when hidden (to
  // revert it to its original value).
  bool should_revert_web_contents_title_;

  // Whether or not the contents was loading resources when the interstitial was
  // shown.  We restore this state if the user proceeds from the interstitial.
  bool web_contents_was_loading_;

  // Whether the ResourceDispatcherHost has been notified to cancel/resume the
  // resource requests blocked for the RenderViewHost.
  bool resource_dispatcher_host_notified_;

  // The original title of the contents that should be reverted to when the
  // interstitial is hidden.
  base::string16 original_web_contents_title_;

  // Our RenderViewHostViewDelegate, necessary for accelerators to work.
  scoped_ptr<InterstitialPageRVHDelegateView> rvh_delegate_view_;

  // Settings passed to the renderer.
  mutable RendererPreferences renderer_preferences_;

  bool create_view_;

  scoped_ptr<InterstitialPageDelegate> delegate_;

  base::WeakPtrFactory<InterstitialPageImpl> weak_ptr_factory_;

  scoped_refptr<SessionStorageNamespace> session_storage_namespace_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialPageImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_INTERSTITIAL_PAGE_IMPL_H_
