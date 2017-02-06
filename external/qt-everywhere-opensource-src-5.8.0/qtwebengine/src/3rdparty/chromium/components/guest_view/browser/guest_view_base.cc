// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/guest_view_base.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/guest_view/browser/guest_view_event.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "components/guest_view/common/guest_view_messages.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_plugin_guest_mode.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/url_constants.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using content::WebContents;

namespace content {
struct FrameNavigateParams;
}

namespace guest_view {

namespace {

using WebContentsGuestViewMap = std::map<const WebContents*, GuestViewBase*>;
static base::LazyInstance<WebContentsGuestViewMap> webcontents_guestview_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

SetSizeParams::SetSizeParams() {
}
SetSizeParams::~SetSizeParams() {
}

// This observer ensures that the GuestViewBase destroys itself when its
// embedder goes away. It also tracks when the embedder's fullscreen is
// toggled or when its page scale factor changes so the guest can change
// itself accordingly.
class GuestViewBase::OwnerContentsObserver : public WebContentsObserver {
 public:
  OwnerContentsObserver(GuestViewBase* guest,
                        WebContents* embedder_web_contents)
      : WebContentsObserver(embedder_web_contents),
        is_fullscreen_(false),
        destroyed_(false),
        guest_(guest) {}

  ~OwnerContentsObserver() override {}

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override {
    // If the embedder is destroyed then destroy the guest.
    Destroy();
  }

  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override {
    // If the embedder navigates to a different page then destroy the guest.
    if (details.is_navigation_to_different_page())
      Destroy();
  }

  void RenderProcessGone(base::TerminationStatus status) override {
    if (destroyed_)
      return;
    // If the embedder process is destroyed, then destroy the guest.
    Destroy();
  }

  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override {
    if (destroyed_)
      return;

    is_fullscreen_ = entered_fullscreen;
    guest_->EmbedderFullscreenToggled(is_fullscreen_);
  }

  void MainFrameWasResized(bool width_changed) override {
    if (destroyed_ || !web_contents()->GetDelegate())
      return;

    bool current_fullscreen =
        web_contents()->GetDelegate()->IsFullscreenForTabOrPending(
            web_contents());
    if (is_fullscreen_ && !current_fullscreen) {
      is_fullscreen_ = false;
      guest_->EmbedderFullscreenToggled(is_fullscreen_);
    }
  }

  void OnPageScaleFactorChanged(float page_scale_factor) override {
    if (destroyed_)
      return;

    guest_->web_contents()->SetPageScale(page_scale_factor);
  }

  void DidUpdateAudioMutingState(bool muted) override {
    if (destroyed_)
      return;

    guest_->web_contents()->SetAudioMuted(muted);
  }

 private:
  bool is_fullscreen_;
  bool destroyed_;
  GuestViewBase* guest_;

  void Destroy() {
    if (destroyed_)
      return;
    destroyed_ = true;
    guest_->Destroy();
  }

  DISALLOW_COPY_AND_ASSIGN(OwnerContentsObserver);
};

// This observer ensures that the GuestViewBase destroys itself if its opener
// WebContents goes away before the GuestViewBase is attached.
class GuestViewBase::OpenerLifetimeObserver : public WebContentsObserver {
 public:
  OpenerLifetimeObserver(GuestViewBase* guest)
      : WebContentsObserver(guest->GetOpener()->web_contents()),
        guest_(guest) {}

  ~OpenerLifetimeObserver() override {}

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override {
    if (guest_->attached())
      return;

    // If the opener is destroyed then destroy the guest.
    guest_->Destroy();
  }

 private:
  GuestViewBase* guest_;

  DISALLOW_COPY_AND_ASSIGN(OpenerLifetimeObserver);
};

GuestViewBase::GuestViewBase(WebContents* owner_web_contents)
    : owner_web_contents_(owner_web_contents),
      browser_context_(owner_web_contents->GetBrowserContext()),
      guest_instance_id_(GuestViewManager::FromBrowserContext(browser_context_)
                             ->GetNextInstanceID()),
      view_instance_id_(kInstanceIDNone),
      element_instance_id_(kInstanceIDNone),
      initialized_(false),
      is_being_destroyed_(false),
      guest_host_(nullptr),
      auto_size_enabled_(false),
      is_full_page_plugin_(false),
      guest_proxy_routing_id_(MSG_ROUTING_NONE),
      weak_ptr_factory_(this) {
  owner_host_ = GuestViewManager::FromBrowserContext(browser_context_)->
      IsOwnedByExtension(this) ?
          owner_web_contents->GetLastCommittedURL().host() : std::string();
}

GuestViewBase::~GuestViewBase() {}

void GuestViewBase::Init(const base::DictionaryValue& create_params,
                         const WebContentsCreatedCallback& callback) {
  if (initialized_)
    return;
  initialized_ = true;

  if (!GuestViewManager::FromBrowserContext(browser_context_)->
          IsGuestAvailableToContext(this)) {
    // The derived class did not create a WebContents so this class serves no
    // purpose. Let's self-destruct.
    delete this;
    callback.Run(nullptr);
    return;
  }

  std::unique_ptr<base::DictionaryValue> params(create_params.DeepCopy());
  CreateWebContents(create_params,
                    base::Bind(&GuestViewBase::CompleteInit,
                               weak_ptr_factory_.GetWeakPtr(),
                               base::Passed(&params),
                               callback));
}

void GuestViewBase::InitWithWebContents(
    const base::DictionaryValue& create_params,
    WebContents* guest_web_contents) {
  DCHECK(guest_web_contents);

  // Create a ZoomController to allow the guest's contents to be zoomed.
  // Do this before adding the GuestView as a WebContents Observer so that
  // the GuestView and its derived classes can re-configure the ZoomController
  // after the latter has handled WebContentsObserver events (observers are
  // notified of events in the same order they are added as observers). For
  // example, GuestViewBase may wish to put its guest into isolated zoom mode
  // in DidNavigateMainFrame, but since ZoomController always resets to default
  // zoom mode on this event, GuestViewBase would need to do so after
  // ZoomController::DidNavigateMainFrame has completed.
  zoom::ZoomController::CreateForWebContents(guest_web_contents);

  // At this point, we have just created the guest WebContents, we need to add
  // an observer to the owner WebContents. This observer will be responsible
  // for destroying the guest WebContents if the owner goes away.
  owner_contents_observer_.reset(
      new OwnerContentsObserver(this, owner_web_contents_));

  WebContentsObserver::Observe(guest_web_contents);
  guest_web_contents->SetDelegate(this);
  webcontents_guestview_map.Get().insert(
      std::make_pair(guest_web_contents, this));
  GuestViewManager::FromBrowserContext(browser_context_)->
      AddGuest(guest_instance_id_, guest_web_contents);

  // Populate the view instance ID if we have it on creation.
  create_params.GetInteger(kParameterInstanceId, &view_instance_id_);

  if (CanRunInDetachedState())
    SetUpSizing(create_params);

  // Observe guest zoom changes.
  auto zoom_controller = zoom::ZoomController::FromWebContents(web_contents());
  zoom_controller->AddObserver(this);

  // Give the derived class an opportunity to perform additional initialization.
  DidInitialize(create_params);
}

void GuestViewBase::LoadURLWithParams(
      const content::NavigationController::LoadURLParams& load_params) {
  int guest_proxy_routing_id = host()->LoadURLWithParams(load_params);
  DCHECK(guest_proxy_routing_id_ == MSG_ROUTING_NONE ||
         guest_proxy_routing_id == guest_proxy_routing_id_);
  guest_proxy_routing_id_ = guest_proxy_routing_id;
}

void GuestViewBase::DispatchOnResizeEvent(const gfx::Size& old_size,
                                          const gfx::Size& new_size) {
  if (new_size == old_size)
    return;

  // Dispatch the onResize event.
  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(kOldWidth, old_size.width());
  args->SetInteger(kOldHeight, old_size.height());
  args->SetInteger(kNewWidth, new_size.width());
  args->SetInteger(kNewHeight, new_size.height());
  DispatchEventToGuestProxy(
      base::WrapUnique(new GuestViewEvent(kEventResize, std::move(args))));
}

gfx::Size GuestViewBase::GetDefaultSize() const {
  if (is_full_page_plugin()) {
    // Full page plugins default to the size of the owner's viewport.
    return owner_web_contents()
        ->GetRenderWidgetHostView()
        ->GetVisibleViewportSize();
  } else {
    return gfx::Size(kDefaultWidth, kDefaultHeight);
  }
}

void GuestViewBase::SetSize(const SetSizeParams& params) {
  bool enable_auto_size =
      params.enable_auto_size ? *params.enable_auto_size : auto_size_enabled_;
  gfx::Size min_size = params.min_size ? *params.min_size : min_auto_size_;
  gfx::Size max_size = params.max_size ? *params.max_size : max_auto_size_;

  if (params.normal_size)
    normal_size_ = *params.normal_size;

  min_auto_size_ = min_size;
  min_auto_size_.SetToMin(max_size);
  max_auto_size_ = max_size;
  max_auto_size_.SetToMax(min_size);

  enable_auto_size &= !min_auto_size_.IsEmpty() && !max_auto_size_.IsEmpty() &&
                      IsAutoSizeSupported();

  content::RenderViewHost* rvh = web_contents()->GetRenderViewHost();
  if (enable_auto_size) {
    // Autosize is being enabled.
    rvh->EnableAutoResize(min_auto_size_, max_auto_size_);
    normal_size_.SetSize(0, 0);
  } else {
    // Autosize is being disabled.
    // Use default width/height if missing from partially defined normal size.
    if (normal_size_.width() && !normal_size_.height())
      normal_size_.set_height(GetDefaultSize().height());
    if (!normal_size_.width() && normal_size_.height())
      normal_size_.set_width(GetDefaultSize().width());

    gfx::Size new_size;
    if (!normal_size_.IsEmpty()) {
      new_size = normal_size_;
    } else if (!guest_size_.IsEmpty()) {
      new_size = guest_size_;
    } else {
      new_size = GetDefaultSize();
    }

    bool changed_due_to_auto_resize = false;
    if (auto_size_enabled_) {
      // Autosize was previously enabled.
      rvh->DisableAutoResize(new_size);
      changed_due_to_auto_resize = true;
    } else {
      // Autosize was already disabled.
      guest_host_->SizeContents(new_size);
    }

    UpdateGuestSize(new_size, changed_due_to_auto_resize);
  }

  auto_size_enabled_ = enable_auto_size;
}

// static
void GuestViewBase::CleanUp(content::BrowserContext* browser_context,
                            int embedder_process_id,
                            int view_instance_id) {
  // TODO(paulmeyer): Add in any general GuestView cleanup work here.
}

// static
GuestViewBase* GuestViewBase::FromWebContents(const WebContents* web_contents) {
  WebContentsGuestViewMap* guest_map = webcontents_guestview_map.Pointer();
  auto it = guest_map->find(web_contents);
  return it == guest_map->end() ? nullptr : it->second;
}

// static
GuestViewBase* GuestViewBase::From(int owner_process_id,
                                   int guest_instance_id) {
  auto host = content::RenderProcessHost::FromID(owner_process_id);
  if (!host)
    return nullptr;

  WebContents* guest_web_contents =
      GuestViewManager::FromBrowserContext(host->GetBrowserContext())
          ->GetGuestByInstanceIDSafely(guest_instance_id, owner_process_id);
  if (!guest_web_contents)
    return nullptr;

  return GuestViewBase::FromWebContents(guest_web_contents);
}

// static
WebContents* GuestViewBase::GetTopLevelWebContents(WebContents* web_contents) {
  while (GuestViewBase* guest = FromWebContents(web_contents))
    web_contents = guest->owner_web_contents();
  return web_contents;
}

// static
bool GuestViewBase::IsGuest(WebContents* web_contents) {
  return !!GuestViewBase::FromWebContents(web_contents);
}

bool GuestViewBase::IsAutoSizeSupported() const {
  return false;
}

bool GuestViewBase::IsPreferredSizeModeEnabled() const {
  return false;
}

bool GuestViewBase::ZoomPropagatesFromEmbedderToGuest() const {
  return true;
}

void GuestViewBase::SetContextMenuPosition(const gfx::Point& position) {}

WebContents* GuestViewBase::CreateNewGuestWindow(
    const WebContents::CreateParams& create_params) {
  auto guest_manager = GuestViewManager::FromBrowserContext(browser_context());
  return guest_manager->CreateGuestWithWebContentsParams(
      GetViewType(),
      owner_web_contents(),
      create_params);
}

void GuestViewBase::DidAttach(int guest_proxy_routing_id) {
  DCHECK(guest_proxy_routing_id_ == MSG_ROUTING_NONE ||
         guest_proxy_routing_id == guest_proxy_routing_id_);
  guest_proxy_routing_id_ = guest_proxy_routing_id;

  opener_lifetime_observer_.reset();

  SetUpSizing(*attach_params());

  // The guest should have the same muting state as the owner.
  web_contents()->SetAudioMuted(owner_web_contents()->IsAudioMuted());

  // Give the derived class an opportunity to perform some actions.
  DidAttachToEmbedder();

  // Inform the associated GuestViewContainer that the contentWindow is ready.
  embedder_web_contents()->Send(new GuestViewMsg_GuestAttached(
      element_instance_id_,
      guest_proxy_routing_id));

  SendQueuedEvents();
}

void GuestViewBase::DidDetach() {
  GuestViewManager::FromBrowserContext(browser_context_)->DetachGuest(this);
  StopTrackingEmbedderZoomLevel();
  owner_web_contents()->Send(new GuestViewMsg_GuestDetached(
      element_instance_id_));
  element_instance_id_ = kInstanceIDNone;
  if (!CanRunInDetachedState())
    Destroy();
}

bool GuestViewBase::HandleFindForEmbedder(
    int request_id,
    const base::string16& search_text,
    const blink::WebFindOptions& options) {
  if (ShouldHandleFindRequestsForEmbedder()) {
    web_contents()->Find(request_id, search_text, options);
    return true;
  }
  return false;
}

bool GuestViewBase::HandleStopFindingForEmbedder(
    content::StopFindAction action) {
  if (ShouldHandleFindRequestsForEmbedder()) {
    web_contents()->StopFinding(action);
    return true;
  }
  return false;
}

WebContents* GuestViewBase::GetOwnerWebContents() const {
  return owner_web_contents_;
}

void GuestViewBase::GuestSizeChanged(const gfx::Size& new_size) {
  UpdateGuestSize(new_size, auto_size_enabled_);
}

const GURL& GuestViewBase::GetOwnerSiteURL() const {
  return owner_web_contents()->GetLastCommittedURL();
}

void GuestViewBase::Destroy() {
  if (is_being_destroyed_)
    return;

  is_being_destroyed_ = true;

  // It is important to clear owner_web_contents_ after the call to
  // StopTrackingEmbedderZoomLevel(), but before the rest of
  // the statements in this function.
  StopTrackingEmbedderZoomLevel();
  owner_web_contents_ = nullptr;

  DCHECK(web_contents());

  // Give the derived class an opportunity to perform some cleanup.
  WillDestroy();

  // Invalidate weak pointers now so that bound callbacks cannot be called late
  // into destruction. We must call this after WillDestroy because derived types
  // may wish to access their openers.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Give the content module an opportunity to perform some cleanup.
  guest_host_->WillDestroy();
  guest_host_ = nullptr;

  webcontents_guestview_map.Get().erase(web_contents());
  GuestViewManager::FromBrowserContext(browser_context_)->
      RemoveGuest(guest_instance_id_);
  pending_events_.clear();

  delete web_contents();
}

void GuestViewBase::SetAttachParams(const base::DictionaryValue& params) {
  attach_params_.reset(params.DeepCopy());
  attach_params_->GetInteger(kParameterInstanceId, &view_instance_id_);
}

void GuestViewBase::SetOpener(GuestViewBase* guest) {
  if (guest && guest->IsViewType(GetViewType())) {
    opener_ = guest->weak_ptr_factory_.GetWeakPtr();
    if (!attached())
      opener_lifetime_observer_.reset(new OpenerLifetimeObserver(this));
    return;
  }
  opener_ = base::WeakPtr<GuestViewBase>();
  opener_lifetime_observer_.reset();
}

void GuestViewBase::SetGuestHost(content::GuestHost* guest_host) {
  guest_host_ = guest_host;
}

void GuestViewBase::WillAttach(WebContents* embedder_web_contents,
                               int element_instance_id,
                               bool is_full_page_plugin,
                               const base::Closure& callback) {
  // Stop tracking the old embedder's zoom level.
  if (owner_web_contents())
    StopTrackingEmbedderZoomLevel();

  if (owner_web_contents_ != embedder_web_contents) {
    DCHECK_EQ(owner_contents_observer_->web_contents(), owner_web_contents_);
    owner_web_contents_ = embedder_web_contents;
    owner_contents_observer_.reset(
        new OwnerContentsObserver(this, embedder_web_contents));
    owner_host_ = GuestViewManager::FromBrowserContext(browser_context_)->
        IsOwnedByExtension(this) ?
            owner_web_contents()->GetLastCommittedURL().host() : std::string();
  }

  // Start tracking the new embedder's zoom level.
  StartTrackingEmbedderZoomLevel();
  element_instance_id_ = element_instance_id;
  is_full_page_plugin_ = is_full_page_plugin;

  WillAttachToEmbedder();

  // Completing attachment will resume suspended resource loads and then send
  // queued events.
  SignalWhenReady(callback);
}

void GuestViewBase::SignalWhenReady(const base::Closure& callback) {
  // The default behavior is to call the |callback| immediately. Derived classes
  // can implement an alternative signal for readiness.
  callback.Run();
}

bool GuestViewBase::ShouldHandleFindRequestsForEmbedder() const {
  return false;
}

int GuestViewBase::LogicalPixelsToPhysicalPixels(double logical_pixels) const {
  DCHECK(logical_pixels >= 0);
  double zoom_factor = GetEmbedderZoomFactor();
  return lround(logical_pixels * zoom_factor);
}

double GuestViewBase::PhysicalPixelsToLogicalPixels(int physical_pixels) const {
  DCHECK(physical_pixels >= 0);
  double zoom_factor = GetEmbedderZoomFactor();
  return physical_pixels / zoom_factor;
}

void GuestViewBase::DidStopLoading() {
  content::RenderViewHost* rvh = web_contents()->GetRenderViewHost();

  if (IsPreferredSizeModeEnabled())
    rvh->EnablePreferredSizeMode();
  GuestViewDidStopLoading();
}

void GuestViewBase::RenderViewReady() {
  GuestReady();
}

void GuestViewBase::WebContentsDestroyed() {
  // Let the derived class know that its WebContents is in the process of
  // being destroyed. web_contents() is still valid at this point.
  // TODO(fsamuel): This allows for reentrant code into WebContents during
  // destruction. This could potentially lead to bugs. Perhaps we should get rid
  // of this?
  GuestDestroyed();

  // Self-destruct.
  delete this;
}

void GuestViewBase::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (attached() && ZoomPropagatesFromEmbedderToGuest())
    SetGuestZoomLevelToMatchEmbedder();
}

void GuestViewBase::ActivateContents(WebContents* web_contents) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return;

  embedder_web_contents()->GetDelegate()->ActivateContents(
      embedder_web_contents());
}

void GuestViewBase::ContentsMouseEvent(WebContents* source,
                                       const gfx::Point& location,
                                       bool motion,
                                       bool exited) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return;

  embedder_web_contents()->GetDelegate()->ContentsMouseEvent(
      embedder_web_contents(), location, motion, exited);
}

void GuestViewBase::ContentsZoomChange(bool zoom_in) {
  zoom::PageZoom::Zoom(embedder_web_contents(), zoom_in
                                                    ? content::PAGE_ZOOM_IN
                                                    : content::PAGE_ZOOM_OUT);
}

void GuestViewBase::HandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (!attached())
    return;

  // Send the keyboard events back to the embedder to reprocess them.
  embedder_web_contents()->GetDelegate()->
      HandleKeyboardEvent(embedder_web_contents(), event);
}

void GuestViewBase::LoadingStateChanged(WebContents* source,
                                        bool to_different_document) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return;

  embedder_web_contents()->GetDelegate()->LoadingStateChanged(
      embedder_web_contents(), to_different_document);
}

content::ColorChooser* GuestViewBase::OpenColorChooser(
    WebContents* web_contents,
    SkColor color,
    const std::vector<content::ColorSuggestion>& suggestions) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return nullptr;

  return embedder_web_contents()->GetDelegate()->OpenColorChooser(
      web_contents, color, suggestions);
}

void GuestViewBase::ResizeDueToAutoResize(WebContents* web_contents,
                                          const gfx::Size& new_size) {
  guest_host_->GuestResizeDueToAutoResize(new_size);
}

void GuestViewBase::RunFileChooser(content::RenderFrameHost* render_frame_host,
                                   const content::FileChooserParams& params) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return;

  embedder_web_contents()->GetDelegate()->RunFileChooser(render_frame_host,
                                                         params);
}

bool GuestViewBase::ShouldFocusPageAfterCrash() {
  // Focus is managed elsewhere.
  return false;
}

bool GuestViewBase::PreHandleGestureEvent(WebContents* source,
                                          const blink::WebGestureEvent& event) {
  return event.type == blink::WebGestureEvent::GesturePinchBegin ||
      event.type == blink::WebGestureEvent::GesturePinchUpdate ||
      event.type == blink::WebGestureEvent::GesturePinchEnd;
}

void GuestViewBase::UpdatePreferredSize(WebContents* target_web_contents,
                                        const gfx::Size& pref_size) {
  // In theory it's not necessary to check IsPreferredSizeModeEnabled() because
  // there will only be events if it was enabled in the first place. However,
  // something else may have turned on preferred size mode, so double check.
  DCHECK_EQ(web_contents(), target_web_contents);
  if (IsPreferredSizeModeEnabled()) {
    OnPreferredSizeChanged(pref_size);
  }
}

void GuestViewBase::UpdateTargetURL(WebContents* source, const GURL& url) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return;

  embedder_web_contents()->GetDelegate()->UpdateTargetURL(
      embedder_web_contents(), url);
}

bool GuestViewBase::ShouldResumeRequestsForCreatedWindow() {
  return false;
}

void GuestViewBase::FindReply(WebContents* source,
                              int request_id,
                              int number_of_matches,
                              const gfx::Rect& selection_rect,
                              int active_match_ordinal,
                              bool final_update) {
  if (ShouldHandleFindRequestsForEmbedder() &&
      attached() && embedder_web_contents()->GetDelegate()) {
    embedder_web_contents()->GetDelegate()->FindReply(embedder_web_contents(),
                                                      request_id,
                                                      number_of_matches,
                                                      selection_rect,
                                                      active_match_ordinal,
                                                      final_update);
  }
}

void GuestViewBase::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  if (data.web_contents == embedder_web_contents()) {
    // The embedder's zoom level has changed.
    auto guest_zoom_controller =
        zoom::ZoomController::FromWebContents(web_contents());
    if (content::ZoomValuesEqual(data.new_zoom_level,
                                 guest_zoom_controller->GetZoomLevel())) {
      return;
    }
    // When the embedder's zoom level doesn't match the guest's, then update the
    // guest's zoom level to match.
    guest_zoom_controller->SetZoomLevel(data.new_zoom_level);
    return;
  }

  if (data.web_contents == web_contents()) {
    // The guest's zoom level has changed.
    GuestZoomChanged(data.old_zoom_level, data.new_zoom_level);
  }
}

void GuestViewBase::DispatchEventToGuestProxy(
    std::unique_ptr<GuestViewEvent> event) {
  event->Dispatch(this, guest_instance_id_);
}

void GuestViewBase::DispatchEventToView(std::unique_ptr<GuestViewEvent> event) {
  if (!attached() &&
      (!CanRunInDetachedState() || !can_owner_receive_events())) {
    pending_events_.push_back(std::move(event));
    return;
  }

  event->Dispatch(this, view_instance_id_);
}

void GuestViewBase::SendQueuedEvents() {
  if (!attached())
    return;
  while (!pending_events_.empty()) {
    std::unique_ptr<GuestViewEvent> event_ptr =
        std::move(pending_events_.front());
    pending_events_.pop_front();
    event_ptr->Dispatch(this, view_instance_id_);
  }
}

void GuestViewBase::CompleteInit(
    std::unique_ptr<base::DictionaryValue> create_params,
    const WebContentsCreatedCallback& callback,
    WebContents* guest_web_contents) {
  if (!guest_web_contents) {
    // The derived class did not create a WebContents so this class serves no
    // purpose. Let's self-destruct.
    delete this;
    callback.Run(nullptr);
    return;
  }
  InitWithWebContents(*create_params, guest_web_contents);
  callback.Run(guest_web_contents);
}

double GuestViewBase::GetEmbedderZoomFactor() const {
  if (!embedder_web_contents())
    return 1.0;

  return content::ZoomLevelToZoomFactor(
      zoom::ZoomController::GetZoomLevelForWebContents(
          embedder_web_contents()));
}

void GuestViewBase::SetUpSizing(const base::DictionaryValue& params) {
  // Read the autosize parameters passed in from the embedder.
  bool auto_size_enabled = auto_size_enabled_;
  params.GetBoolean(kAttributeAutoSize, &auto_size_enabled);

  int max_height = max_auto_size_.height();
  int max_width = max_auto_size_.width();
  params.GetInteger(kAttributeMaxHeight, &max_height);
  params.GetInteger(kAttributeMaxWidth, &max_width);

  int min_height = min_auto_size_.height();
  int min_width = min_auto_size_.width();
  params.GetInteger(kAttributeMinHeight, &min_height);
  params.GetInteger(kAttributeMinWidth, &min_width);

  double element_height = 0.0;
  double element_width = 0.0;
  params.GetDouble(kElementHeight, &element_height);
  params.GetDouble(kElementWidth, &element_width);

  // Set the normal size to the element size so that the guestview will fit
  // the element initially if autosize is disabled.
  int normal_height = normal_size_.height();
  int normal_width = normal_size_.width();
  // If the element size was provided in logical units (versus physical), then
  // it will be converted to physical units.
  bool element_size_is_logical = false;
  params.GetBoolean(kElementSizeIsLogical, &element_size_is_logical);
  if (element_size_is_logical) {
    // Convert the element size from logical pixels to physical pixels.
    normal_height = LogicalPixelsToPhysicalPixels(element_height);
    normal_width = LogicalPixelsToPhysicalPixels(element_width);
  } else {
    normal_height = lround(element_height);
    normal_width = lround(element_width);
  }

  SetSizeParams set_size_params;
  set_size_params.enable_auto_size.reset(new bool(auto_size_enabled));
  set_size_params.min_size.reset(new gfx::Size(min_width, min_height));
  set_size_params.max_size.reset(new gfx::Size(max_width, max_height));
  set_size_params.normal_size.reset(new gfx::Size(normal_width, normal_height));

  // Call SetSize to apply all the appropriate validation and clipping of
  // values.
  SetSize(set_size_params);
}

void GuestViewBase::SetGuestZoomLevelToMatchEmbedder() {
  auto embedder_zoom_controller =
      zoom::ZoomController::FromWebContents(owner_web_contents());
  if (!embedder_zoom_controller)
    return;

  zoom::ZoomController::FromWebContents(web_contents())
      ->SetZoomLevel(embedder_zoom_controller->GetZoomLevel());
}

void GuestViewBase::StartTrackingEmbedderZoomLevel() {
  if (!ZoomPropagatesFromEmbedderToGuest())
    return;

  auto embedder_zoom_controller =
      zoom::ZoomController::FromWebContents(owner_web_contents());
  // Chrome Apps do not have a ZoomController.
  if (!embedder_zoom_controller)
    return;
  // Listen to the embedder's zoom changes.
  embedder_zoom_controller->AddObserver(this);

  // Set the guest's initial zoom level to be equal to the embedder's.
  SetGuestZoomLevelToMatchEmbedder();
}

void GuestViewBase::StopTrackingEmbedderZoomLevel() {
  if (!attached() || !ZoomPropagatesFromEmbedderToGuest())
    return;

  auto embedder_zoom_controller =
      zoom::ZoomController::FromWebContents(owner_web_contents());
  // Chrome Apps do not have a ZoomController.
  if (!embedder_zoom_controller)
    return;
  embedder_zoom_controller->RemoveObserver(this);
}

void GuestViewBase::UpdateGuestSize(const gfx::Size& new_size,
                                    bool due_to_auto_resize) {
  if (due_to_auto_resize)
    GuestSizeChangedDueToAutoSize(guest_size_, new_size);
  DispatchOnResizeEvent(guest_size_, new_size);
  guest_size_ = new_size;
}

}  // namespace guest_view
