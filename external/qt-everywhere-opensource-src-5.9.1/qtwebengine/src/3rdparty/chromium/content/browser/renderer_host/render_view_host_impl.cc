// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_impl.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/variations/variations_associated_data.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/content_switches_internal.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/speech_recognition_messages.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/file_chooser_file_info.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/touch/touch_device.h"
#include "ui/base/touch/touch_enabled.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/native_theme/native_theme_switches.h"
#include "url/url_constants.h"

#if defined(OS_WIN)
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/platform_font_win.h"
#endif

using base::TimeDelta;
using blink::WebConsoleMessage;
using blink::WebInputEvent;
using blink::WebMediaPlayerAction;
using blink::WebPluginAction;

namespace content {
namespace {

void GetPlatformSpecificPrefs(RendererPreferences* prefs) {
#if defined(OS_WIN)
  NONCLIENTMETRICS_XP metrics = {0};
  base::win::GetNonClientMetrics(&metrics);

  prefs->caption_font_family_name = metrics.lfCaptionFont.lfFaceName;
  prefs->caption_font_height = gfx::PlatformFontWin::GetFontSize(
      metrics.lfCaptionFont);

  prefs->small_caption_font_family_name = metrics.lfSmCaptionFont.lfFaceName;
  prefs->small_caption_font_height = gfx::PlatformFontWin::GetFontSize(
      metrics.lfSmCaptionFont);

  prefs->menu_font_family_name = metrics.lfMenuFont.lfFaceName;
  prefs->menu_font_height = gfx::PlatformFontWin::GetFontSize(
      metrics.lfMenuFont);

  prefs->status_font_family_name = metrics.lfStatusFont.lfFaceName;
  prefs->status_font_height = gfx::PlatformFontWin::GetFontSize(
      metrics.lfStatusFont);

  prefs->message_font_family_name = metrics.lfMessageFont.lfFaceName;
  prefs->message_font_height = gfx::PlatformFontWin::GetFontSize(
      metrics.lfMessageFont);

  prefs->vertical_scroll_bar_width_in_dips =
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXVSCROLL);
  prefs->horizontal_scroll_bar_height_in_dips =
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYHSCROLL);
  prefs->arrow_bitmap_height_vertical_scroll_bar_in_dips =
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYVSCROLL);
  prefs->arrow_bitmap_width_horizontal_scroll_bar_in_dips =
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXHSCROLL);
#elif defined(OS_LINUX)
  prefs->system_font_family_name = gfx::Font().GetFontName();
#endif
}

}  // namespace

// static
const int64_t RenderViewHostImpl::kUnloadTimeoutMS = 1000;

///////////////////////////////////////////////////////////////////////////////
// RenderViewHost, public:

// static
RenderViewHost* RenderViewHost::FromID(int render_process_id,
                                       int render_view_id) {
  return RenderViewHostImpl::FromID(render_process_id, render_view_id);
}

// static
RenderViewHost* RenderViewHost::From(RenderWidgetHost* rwh) {
  return RenderViewHostImpl::From(rwh);
}

///////////////////////////////////////////////////////////////////////////////
// RenderViewHostImpl, public:

// static
RenderViewHostImpl* RenderViewHostImpl::FromID(int render_process_id,
                                               int render_view_id) {
  RenderWidgetHost* widget =
      RenderWidgetHost::FromID(render_process_id, render_view_id);
  if (!widget)
    return nullptr;
  return From(widget);
}

// static
RenderViewHostImpl* RenderViewHostImpl::From(RenderWidgetHost* rwh) {
  DCHECK(rwh);
  RenderWidgetHostOwnerDelegate* owner_delegate =
      RenderWidgetHostImpl::From(rwh)->owner_delegate();
  if (!owner_delegate)
    return nullptr;
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(owner_delegate);
  DCHECK_EQ(rwh, rvh->GetWidget());
  return rvh;
}

RenderViewHostImpl::RenderViewHostImpl(
    SiteInstance* instance,
    std::unique_ptr<RenderWidgetHostImpl> widget,
    RenderViewHostDelegate* delegate,
    int32_t main_frame_routing_id,
    bool swapped_out,
    bool has_initialized_audio_host)
    : render_widget_host_(std::move(widget)),
      frames_ref_count_(0),
      delegate_(delegate),
      instance_(static_cast<SiteInstanceImpl*>(instance)),
      enabled_bindings_(0),
      is_active_(!swapped_out),
      is_swapped_out_(swapped_out),
      main_frame_routing_id_(main_frame_routing_id),
      is_waiting_for_close_ack_(false),
      sudden_termination_allowed_(false),
      render_view_termination_status_(base::TERMINATION_STATUS_STILL_RUNNING),
      updating_web_preferences_(false),
      render_view_ready_on_process_launch_(false),
      weak_factory_(this) {
  DCHECK(instance_.get());
  CHECK(delegate_);  // http://crbug.com/82827

  GetWidget()->set_owner_delegate(this);

  GetProcess()->AddObserver(this);

  // New views may be created during RenderProcessHost::ProcessDied(), within a
  // brief window where the internal ChannelProxy is null. This ensures that the
  // ChannelProxy is re-initialized in such cases so that subsequent messages
  // make their way to the new renderer once its restarted.
  GetProcess()->EnableSendQueue();

  if (ResourceDispatcherHostImpl::Get()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ResourceDispatcherHostImpl::OnRenderViewHostCreated,
                   base::Unretained(ResourceDispatcherHostImpl::Get()),
                   GetProcess()->GetID(), GetRoutingID()));
  }
}

RenderViewHostImpl::~RenderViewHostImpl() {
  if (ResourceDispatcherHostImpl::Get()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ResourceDispatcherHostImpl::OnRenderViewHostDeleted,
                   base::Unretained(ResourceDispatcherHostImpl::Get()),
                   GetProcess()->GetID(), GetRoutingID()));
  }

  delegate_->RenderViewDeleted(this);
  GetProcess()->RemoveObserver(this);
}

RenderViewHostDelegate* RenderViewHostImpl::GetDelegate() const {
  return delegate_;
}

SiteInstanceImpl* RenderViewHostImpl::GetSiteInstance() const {
  return instance_.get();
}

bool RenderViewHostImpl::CreateRenderView(
    int opener_frame_route_id,
    int proxy_route_id,
    const FrameReplicationState& replicated_frame_state,
    bool window_was_created_with_opener) {
  TRACE_EVENT0("renderer_host,navigation",
               "RenderViewHostImpl::CreateRenderView");
  DCHECK(!IsRenderViewLive()) << "Creating view twice";

  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init multiple times will be
  // ignored, so this is safe.
  if (!GetProcess()->Init())
    return false;
  DCHECK(GetProcess()->HasConnection());
  DCHECK(GetProcess()->GetBrowserContext());
  CHECK(main_frame_routing_id_ != MSG_ROUTING_NONE ||
        proxy_route_id != MSG_ROUTING_NONE);

  // We should not set both main_frame_routing_id_ and proxy_route_id.  Log
  // cases that this happens (without crashing) to track down
  // https://crbug.com/575245.
  // TODO(creis): Remove this once we've found the cause.
  if (main_frame_routing_id_ != MSG_ROUTING_NONE &&
      proxy_route_id != MSG_ROUTING_NONE)
    base::debug::DumpWithoutCrashing();

  GetWidget()->set_renderer_initialized(true);

  mojom::CreateViewParamsPtr params = mojom::CreateViewParams::New();
  params->renderer_preferences =
      delegate_->GetRendererPrefs(GetProcess()->GetBrowserContext());
  GetPlatformSpecificPrefs(&params->renderer_preferences);
  params->web_preferences = GetWebkitPreferences();
  params->view_id = GetRoutingID();
  params->main_frame_routing_id = main_frame_routing_id_;
  if (main_frame_routing_id_ != MSG_ROUTING_NONE) {
    RenderFrameHostImpl* main_rfh = RenderFrameHostImpl::FromID(
        GetProcess()->GetID(), main_frame_routing_id_);
    DCHECK(main_rfh);
    RenderWidgetHostImpl* main_rwh = main_rfh->GetRenderWidgetHost();
    params->main_frame_widget_routing_id = main_rwh->GetRoutingID();
  }
  params->session_storage_namespace_id =
      delegate_->GetSessionStorageNamespace(instance_.get())->id();
  // Ensure the RenderView sets its opener correctly.
  params->opener_frame_route_id = opener_frame_route_id;
  params->swapped_out = !is_active_;
  params->replicated_frame_state = replicated_frame_state;
  params->proxy_routing_id = proxy_route_id;
  params->hidden = GetWidget()->is_hidden();
  params->never_visible = delegate_->IsNeverVisible();
  params->window_was_created_with_opener = window_was_created_with_opener;
  params->enable_auto_resize = GetWidget()->auto_resize_enabled();
  params->min_size = GetWidget()->min_size_for_auto_resize();
  params->max_size = GetWidget()->max_size_for_auto_resize();
  params->page_zoom_level = delegate_->GetPendingPageZoomLevel();
  params->image_decode_color_space = gfx::ICCProfile::FromBestMonitor();

  GetWidget()->GetResizeParams(&params->initial_size);
  GetWidget()->SetInitialRenderSizeParams(params->initial_size);

  GetProcess()->GetRendererInterface()->CreateView(std::move(params));

  // If it's enabled, tell the renderer to set up the Javascript bindings for
  // sending messages back to the browser.
  if (GetProcess()->IsForGuestsOnly())
    DCHECK_EQ(0, enabled_bindings_);
  Send(new ViewMsg_AllowBindings(GetRoutingID(), enabled_bindings_));
  // Let our delegate know that we created a RenderView.
  delegate_->RenderViewCreated(this);

  // Since this method can create the main RenderFrame in the renderer process,
  // set the proper state on its corresponding RenderFrameHost.
  if (main_frame_routing_id_ != MSG_ROUTING_NONE) {
    RenderFrameHostImpl::FromID(GetProcess()->GetID(), main_frame_routing_id_)
        ->SetRenderFrameCreated(true);
  }
  GetWidget()->delegate()->SendScreenRects();
  PostRenderViewReady();

  return true;
}

bool RenderViewHostImpl::IsRenderViewLive() const {
  return GetProcess()->HasConnection() && GetWidget()->renderer_initialized();
}

void RenderViewHostImpl::SyncRendererPrefs() {
  RendererPreferences renderer_preferences =
      delegate_->GetRendererPrefs(GetProcess()->GetBrowserContext());
  GetPlatformSpecificPrefs(&renderer_preferences);
  Send(new ViewMsg_SetRendererPrefs(GetRoutingID(), renderer_preferences));
}

namespace {

void SetFloatParameterFromMap(
    const std::map<std::string, std::string>& settings,
    const std::string& setting_name,
    float* value) {
  const auto& find_it = settings.find(setting_name);
  if (find_it == settings.end())
    return;
  double parsed_value;
  if (!base::StringToDouble(find_it->second, &parsed_value))
    return;
  *value = parsed_value;
}

}  // namespace

WebPreferences RenderViewHostImpl::ComputeWebkitPrefs() {
  TRACE_EVENT0("browser", "RenderViewHostImpl::GetWebkitPrefs");
  WebPreferences prefs;

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  prefs.web_security_enabled =
      !command_line.HasSwitch(switches::kDisableWebSecurity);

  prefs.remote_fonts_enabled =
      !command_line.HasSwitch(switches::kDisableRemoteFonts);
  prefs.application_cache_enabled = true;
  prefs.xss_auditor_enabled =
      !command_line.HasSwitch(switches::kDisableXSSAuditor);
  prefs.local_storage_enabled =
      !command_line.HasSwitch(switches::kDisableLocalStorage);
  prefs.databases_enabled =
      !command_line.HasSwitch(switches::kDisableDatabases);

  prefs.experimental_webgl_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisable3DAPIs) &&
      !command_line.HasSwitch(switches::kDisableExperimentalWebGL);

  prefs.pepper_3d_enabled =
      !command_line.HasSwitch(switches::kDisablePepper3d);

  prefs.flash_3d_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableFlash3d);
  prefs.flash_stage3d_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableFlashStage3d);
  prefs.flash_stage3d_baseline_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableFlashStage3d);

  prefs.allow_file_access_from_file_urls =
      command_line.HasSwitch(switches::kAllowFileAccessFromFiles);

  prefs.accelerated_2d_canvas_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableAccelerated2dCanvas);
  prefs.antialiased_2d_canvas_disabled =
      command_line.HasSwitch(switches::kDisable2dCanvasAntialiasing);
  prefs.antialiased_clips_2d_canvas_enabled =
      !command_line.HasSwitch(switches::kDisable2dCanvasClipAntialiasing);
  prefs.accelerated_2d_canvas_msaa_sample_count =
      atoi(command_line.GetSwitchValueASCII(
      switches::kAcceleratedCanvas2dMSAASampleCount).c_str());

  prefs.inert_visual_viewport =
      command_line.HasSwitch(switches::kInertVisualViewport);

  prefs.use_solid_color_scrollbars = false;

  prefs.history_entry_requires_user_gesture =
      command_line.HasSwitch(switches::kHistoryEntryRequiresUserGesture);

#if defined(OS_ANDROID)
  // On Android, user gestures are normally required, unless that requirement
  // is disabled with a command-line switch or the equivalent field trial is
  // is set to "Enabled".
  const std::string autoplay_group_name = base::FieldTrialList::FindFullName(
      "MediaElementAutoplay");
  prefs.user_gesture_required_for_media_playback = !command_line.HasSwitch(
      switches::kDisableGestureRequirementForMediaPlayback) &&
          (autoplay_group_name.empty() || autoplay_group_name != "Enabled");

  prefs.progress_bar_completion = GetProgressBarCompletionPolicy();

  prefs.use_solid_color_scrollbars = true;
#endif

  // Handle autoplay gesture override experiment.
  // Note that anything but a well-formed string turns the experiment off.
  prefs.autoplay_experiment_mode = base::FieldTrialList::FindFullName(
      "MediaElementGestureOverrideExperiment");

  prefs.touch_enabled = ui::AreTouchEventsEnabled();
  prefs.device_supports_touch = prefs.touch_enabled &&
      ui::GetTouchScreensAvailability() ==
          ui::TouchScreensAvailability::ENABLED;
  std::tie(prefs.available_pointer_types, prefs.available_hover_types) =
      ui::GetAvailablePointerAndHoverTypes();
  prefs.primary_pointer_type =
      ui::GetPrimaryPointerType(prefs.available_pointer_types);
  prefs.primary_hover_type =
      ui::GetPrimaryHoverType(prefs.available_hover_types);

#if defined(OS_ANDROID)
  prefs.device_supports_mouse = false;
#endif

  prefs.pointer_events_max_touch_points = ui::MaxTouchPoints();

  prefs.touch_adjustment_enabled =
      !command_line.HasSwitch(switches::kDisableTouchAdjustment);

  prefs.enable_scroll_animator =
      command_line.HasSwitch(switches::kEnableSmoothScrolling) ||
      (!command_line.HasSwitch(switches::kDisableSmoothScrolling) &&
      gfx::Animation::ScrollAnimationsEnabledBySystem());

  // Certain GPU features might have been blacklisted.
  GpuDataManagerImpl::GetInstance()->UpdateRendererWebPrefs(&prefs);

  if (ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          GetProcess()->GetID())) {
    prefs.loads_images_automatically = true;
    prefs.javascript_enabled = true;
  }

  prefs.number_of_cpu_cores = base::SysInfo::NumberOfProcessors();

  prefs.viewport_enabled = command_line.HasSwitch(switches::kEnableViewport);

  if (delegate_ && delegate_->IsOverridingUserAgent())
    prefs.viewport_meta_enabled = false;

  prefs.main_frame_resizes_are_orientation_changes =
      command_line.HasSwitch(switches::kMainFrameResizesAreOrientationChanges);

  prefs.color_correct_rendering_enabled =
      command_line.HasSwitch(cc::switches::kEnableColorCorrectRendering);

  prefs.spatial_navigation_enabled = command_line.HasSwitch(
      switches::kEnableSpatialNavigation);

  prefs.disable_reading_from_canvas = command_line.HasSwitch(
      switches::kDisableReadingFromCanvas);

  prefs.strict_mixed_content_checking = command_line.HasSwitch(
      switches::kEnableStrictMixedContentChecking);

  prefs.strict_powerful_feature_restrictions = command_line.HasSwitch(
      switches::kEnableStrictPowerfulFeatureRestrictions);

  const std::string blockable_mixed_content_group =
      base::FieldTrialList::FindFullName("BlockableMixedContent");
  prefs.strictly_block_blockable_mixed_content =
      blockable_mixed_content_group == "StrictlyBlockBlockableMixedContent";

  const std::string plugin_mixed_content_status =
      base::FieldTrialList::FindFullName("PluginMixedContentStatus");
  prefs.block_mixed_plugin_content =
      plugin_mixed_content_status == "BlockableMixedContent";

  prefs.v8_cache_options = GetV8CacheOptions();

  prefs.user_gesture_required_for_presentation = !command_line.HasSwitch(
      switches::kDisableGestureRequirementForPresentation);

  if (delegate_ && delegate_->HideDownloadUI())
    prefs.hide_download_ui = true;

  std::map<std::string, std::string> expensive_background_throttling_prefs;
  variations::GetVariationParamsByFeature(
      features::kExpensiveBackgroundTimerThrottling,
      &expensive_background_throttling_prefs);
  SetFloatParameterFromMap(expensive_background_throttling_prefs, "cpu_budget",
                           &prefs.expensive_background_throttling_cpu_budget);
  SetFloatParameterFromMap(
      expensive_background_throttling_prefs, "initial_budget",
      &prefs.expensive_background_throttling_initial_budget);
  SetFloatParameterFromMap(expensive_background_throttling_prefs, "max_budget",
                           &prefs.expensive_background_throttling_max_budget);
  SetFloatParameterFromMap(expensive_background_throttling_prefs, "max_delay",
                           &prefs.expensive_background_throttling_max_delay);

  GetContentClient()->browser()->OverrideWebkitPrefs(this, &prefs);
  return prefs;
}

void RenderViewHostImpl::ClosePage() {
  is_waiting_for_close_ack_ = true;
  GetWidget()->StartHangMonitorTimeout(
      TimeDelta::FromMilliseconds(kUnloadTimeoutMS),
      blink::WebInputEvent::Undefined,
      RendererUnresponsiveType::RENDERER_UNRESPONSIVE_CLOSE_PAGE);

  bool is_javascript_dialog_showing = delegate_->IsJavaScriptDialogShowing();

  // If there is a JavaScript dialog up, don't bother sending the renderer the
  // close event because it is known unresponsive, waiting for the reply from
  // the dialog.
  if (IsRenderViewLive() && !is_javascript_dialog_showing) {
    // Since we are sending an IPC message to the renderer, increase the event
    // count to prevent the hang monitor timeout from being stopped by input
    // event acknowledgments.
    GetWidget()->increment_in_flight_event_count();

    // TODO(creis): Should this be moved to Shutdown?  It may not be called for
    // RenderViewHosts that have been swapped out.
    NotificationService::current()->Notify(
        NOTIFICATION_RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW,
        Source<RenderViewHost>(this),
        NotificationService::NoDetails());

    Send(new ViewMsg_ClosePage(GetRoutingID()));
  } else {
    // This RenderViewHost doesn't have a live renderer, so just skip the close
    // event and close the page.
    ClosePageIgnoringUnloadEvents();
  }
}

void RenderViewHostImpl::ClosePageIgnoringUnloadEvents() {
  GetWidget()->StopHangMonitorTimeout();
  is_waiting_for_close_ack_ = false;

  sudden_termination_allowed_ = true;
  delegate_->Close(this);
}

void RenderViewHostImpl::RenderProcessReady(RenderProcessHost* host) {
  if (render_view_ready_on_process_launch_) {
    render_view_ready_on_process_launch_ = false;
    RenderViewReady();
  }
}

void RenderViewHostImpl::RenderProcessExited(RenderProcessHost* host,
                                             base::TerminationStatus status,
                                             int exit_code) {
  if (!GetWidget()->renderer_initialized())
    return;

  GetWidget()->RendererExited(status, exit_code);
  delegate_->RenderViewTerminated(this, status, exit_code);
}

bool RenderViewHostImpl::Send(IPC::Message* msg) {
  return GetWidget()->Send(msg);
}

RenderWidgetHostImpl* RenderViewHostImpl::GetWidget() const {
  return render_widget_host_.get();
}

RenderProcessHost* RenderViewHostImpl::GetProcess() const {
  return GetWidget()->GetProcess();
}

int RenderViewHostImpl::GetRoutingID() const {
  return GetWidget()->GetRoutingID();
}

RenderFrameHost* RenderViewHostImpl::GetMainFrame() {
  return RenderFrameHost::FromID(GetProcess()->GetID(), main_frame_routing_id_);
}

void RenderViewHostImpl::AllowBindings(int bindings_flags) {
  // Never grant any bindings to browser plugin guests.
  if (GetProcess()->IsForGuestsOnly()) {
    NOTREACHED() << "Never grant bindings to a guest process.";
    return;
  }

  // Ensure we aren't granting WebUI bindings to a process that has already
  // been used for non-privileged views.
  if (bindings_flags & BINDINGS_POLICY_WEB_UI &&
      GetProcess()->HasConnection() &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          GetProcess()->GetID())) {
    // This process has no bindings yet. Make sure it does not have more
    // than this single active view.
    // --single-process only has one renderer.
    if (GetProcess()->GetActiveViewCount() > 1 &&
        !base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSingleProcess))
      return;
  }

  if (bindings_flags & BINDINGS_POLICY_WEB_UI) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantWebUIBindings(
        GetProcess()->GetID());
  }

  enabled_bindings_ |= bindings_flags;
  if (GetWidget()->renderer_initialized())
    Send(new ViewMsg_AllowBindings(GetRoutingID(), enabled_bindings_));
}

int RenderViewHostImpl::GetEnabledBindings() const {
  return enabled_bindings_;
}

void RenderViewHostImpl::SetWebUIProperty(const std::string& name,
                                          const std::string& value) {
  // This is a sanity check before telling the renderer to enable the property.
  // It could lie and send the corresponding IPC messages anyway, but we will
  // not act on them if enabled_bindings_ doesn't agree. If we get here without
  // WebUI bindings, kill the renderer process.
  if (enabled_bindings_ & BINDINGS_POLICY_WEB_UI) {
    Send(new ViewMsg_SetWebUIProperty(GetRoutingID(), name, value));
  } else {
    RecordAction(
        base::UserMetricsAction("BindingsMismatchTerminate_RVH_WebUI"));
    GetProcess()->Shutdown(content::RESULT_CODE_KILLED, false);
  }
}

void RenderViewHostImpl::RenderWidgetGotFocus() {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->GotFocus();
}

void RenderViewHostImpl::SetInitialFocus(bool reverse) {
  Send(new ViewMsg_SetInitialFocus(GetRoutingID(), reverse));
}

void RenderViewHostImpl::DirectoryEnumerationFinished(
    int request_id,
    const std::vector<base::FilePath>& files) {
  // Grant the security access requested to the given files.
  for (std::vector<base::FilePath>::const_iterator file = files.begin();
       file != files.end(); ++file) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
        GetProcess()->GetID(), *file);
  }
  Send(new ViewMsg_EnumerateDirectoryResponse(GetRoutingID(),
                                              request_id,
                                              files));
}

void RenderViewHostImpl::RenderWidgetWillSetIsLoading(bool is_loading) {
  if (ResourceDispatcherHostImpl::Get()) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ResourceDispatcherHostImpl::OnRenderViewHostSetIsLoading,
                   base::Unretained(ResourceDispatcherHostImpl::Get()),
                   GetProcess()->GetID(),
                   GetRoutingID(),
                   is_loading));
  }
}

bool RenderViewHostImpl::SuddenTerminationAllowed() const {
  return sudden_termination_allowed_ ||
      GetProcess()->SuddenTerminationAllowed();
}

///////////////////////////////////////////////////////////////////////////////
// RenderViewHostImpl, IPC message handlers:

bool RenderViewHostImpl::OnMessageReceived(const IPC::Message& msg) {
  // Filter out most IPC messages if this renderer is swapped out.
  // We still want to handle certain ACKs to keep our state consistent.
  if (is_swapped_out_) {
    if (!SwappedOutMessages::CanHandleWhileSwappedOut(msg)) {
      // If this is a synchronous message and we decided not to handle it,
      // we must send an error reply, or else the renderer will be stuck
      // and won't respond to future requests.
      if (msg.is_sync()) {
        IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
        reply->set_reply_error();
        Send(reply);
      }
      // Don't continue looking for someone to handle it.
      return true;
    }
  }

  if (delegate_->OnMessageReceived(this, msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderViewHostImpl, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_RenderProcessGone, OnRenderProcessGone)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowView, OnShowView)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnShowWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowFullscreenWidget,
                        OnShowFullscreenWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateState, OnUpdateState)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateTargetURL, OnUpdateTargetURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Close, OnClose)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestMove, OnRequestMove)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentAvailableInMainFrame,
                        OnDocumentAvailableInMainFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidContentsPreferredSizeChange,
                        OnDidContentsPreferredSizeChange)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RouteCloseEvent,
                        OnRouteCloseEvent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClosePage_ACK, OnClosePageACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidZoomURL, OnDidZoomURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Focus, OnFocus)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void RenderViewHostImpl::RenderWidgetDidInit() {
  PostRenderViewReady();
}

void RenderViewHostImpl::ShutdownAndDestroy() {
  // We can't release the SessionStorageNamespace until our peer
  // in the renderer has wound down.
  if (GetProcess()->HasConnection()) {
    RenderProcessHostImpl::ReleaseOnCloseACK(
        GetProcess(),
        delegate_->GetSessionStorageNamespaceMap(),
        GetRoutingID());
  }

  GetWidget()->ShutdownAndDestroyWidget(false);
  delete this;
}

void RenderViewHostImpl::CreateNewWindow(
    int32_t route_id,
    int32_t main_frame_route_id,
    int32_t main_frame_widget_route_id,
    const mojom::CreateNewWindowParams& params,
    SessionStorageNamespace* session_storage_namespace) {
  mojom::CreateNewWindowParamsPtr validated_params(params.Clone());
  GetProcess()->FilterURL(false, &validated_params->target_url);
  GetProcess()->FilterURL(false, &validated_params->opener_url);
  GetProcess()->FilterURL(true, &validated_params->opener_security_origin);

  delegate_->CreateNewWindow(GetSiteInstance(), route_id, main_frame_route_id,
                             main_frame_widget_route_id, *validated_params,
                             session_storage_namespace);
}

void RenderViewHostImpl::CreateNewWidget(int32_t route_id,
                                         blink::WebPopupType popup_type) {
  delegate_->CreateNewWidget(GetProcess()->GetID(), route_id, popup_type);
}

void RenderViewHostImpl::CreateNewFullscreenWidget(int32_t route_id) {
  delegate_->CreateNewFullscreenWidget(GetProcess()->GetID(), route_id);
}

void RenderViewHostImpl::OnShowView(int route_id,
                                    WindowOpenDisposition disposition,
                                    const gfx::Rect& initial_rect,
                                    bool user_gesture) {
  delegate_->ShowCreatedWindow(GetProcess()->GetID(), route_id, disposition,
                               initial_rect, user_gesture);
  Send(new ViewMsg_Move_ACK(route_id));
}

void RenderViewHostImpl::OnShowWidget(int route_id,
                                      const gfx::Rect& initial_rect) {
  delegate_->ShowCreatedWidget(GetProcess()->GetID(), route_id, initial_rect);
  Send(new ViewMsg_Move_ACK(route_id));
}

void RenderViewHostImpl::OnShowFullscreenWidget(int route_id) {
  delegate_->ShowCreatedFullscreenWidget(GetProcess()->GetID(), route_id);
  Send(new ViewMsg_Move_ACK(route_id));
}

void RenderViewHostImpl::OnRenderProcessGone(int status, int exit_code) {
  // Do nothing, otherwise RenderWidgetHostImpl will assume it is not a
  // RenderViewHostImpl and destroy itself.
  // TODO(nasko): Remove this hack once RenderViewHost and RenderWidgetHost are
  // decoupled.
}

void RenderViewHostImpl::OnUpdateState(const PageState& state) {
  // Without this check, the renderer can trick the browser into using
  // filenames it can't access in a future session restore.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  int child_id = GetProcess()->GetID();
  if (!policy->CanReadAllFiles(child_id, state.GetReferencedFiles())) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RVH_CAN_ACCESS_FILES_OF_PAGE_STATE);
    return;
  }

  delegate_->UpdateState(this, state);
}

void RenderViewHostImpl::OnUpdateTargetURL(const GURL& url) {
  if (is_active_)
    delegate_->UpdateTargetURL(this, url);

  // Send a notification back to the renderer that we are ready to
  // receive more target urls.
  Send(new ViewMsg_UpdateTargetURL_ACK(GetRoutingID()));
}

void RenderViewHostImpl::OnClose() {
  // If the renderer is telling us to close, it has already run the unload
  // events, and we can take the fast path.
  ClosePageIgnoringUnloadEvents();
}

void RenderViewHostImpl::OnRequestMove(const gfx::Rect& pos) {
  if (is_active_)
    delegate_->RequestMove(pos);
  Send(new ViewMsg_Move_ACK(GetRoutingID()));
}

void RenderViewHostImpl::OnDocumentAvailableInMainFrame(
    bool uses_temporary_zoom_level) {
  delegate_->DocumentAvailableInMainFrame(this);

  if (!uses_temporary_zoom_level)
    return;

  HostZoomMapImpl* host_zoom_map =
      static_cast<HostZoomMapImpl*>(HostZoomMap::Get(GetSiteInstance()));
  host_zoom_map->SetTemporaryZoomLevel(GetProcess()->GetID(),
                                       GetRoutingID(),
                                       host_zoom_map->GetDefaultZoomLevel());
}

void RenderViewHostImpl::OnDidContentsPreferredSizeChange(
    const gfx::Size& new_size) {
  delegate_->UpdatePreferredSize(new_size);
}

void RenderViewHostImpl::OnRouteCloseEvent() {
  // Have the delegate route this to the active RenderViewHost.
  delegate_->RouteCloseEvent(this);
}

void RenderViewHostImpl::OnTakeFocus(bool reverse) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->TakeFocus(reverse);
}

void RenderViewHostImpl::OnClosePageACK() {
  GetWidget()->decrement_in_flight_event_count();
  ClosePageIgnoringUnloadEvents();
}

void RenderViewHostImpl::OnFocus() {
  // Note: We allow focus and blur from swapped out RenderViewHosts, even when
  // the active RenderViewHost is in a different BrowsingInstance (e.g., WebUI).
  delegate_->Activate();
}

void RenderViewHostImpl::RenderWidgetDidForwardMouseEvent(
    const blink::WebMouseEvent& mouse_event) {
  if (mouse_event.type == WebInputEvent::MouseWheel &&
      GetWidget()->ignore_input_events()) {
    delegate_->OnIgnoredUIEvent();
  }
}

bool RenderViewHostImpl::MayRenderWidgetForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event) {
  if (GetWidget()->ignore_input_events()) {
    if (key_event.type == WebInputEvent::RawKeyDown)
      delegate_->OnIgnoredUIEvent();
    return false;
  }
  return true;
}

WebPreferences RenderViewHostImpl::GetWebkitPreferences() {
  if (!web_preferences_.get()) {
    OnWebkitPreferencesChanged();
  }
  return *web_preferences_;
}

void RenderViewHostImpl::UpdateWebkitPreferences(const WebPreferences& prefs) {
  web_preferences_.reset(new WebPreferences(prefs));
  Send(new ViewMsg_UpdateWebPreferences(GetRoutingID(), prefs));
}

void RenderViewHostImpl::OnWebkitPreferencesChanged() {
  // This is defensive code to avoid infinite loops due to code run inside
  // UpdateWebkitPreferences() accidentally updating more preferences and thus
  // calling back into this code. See crbug.com/398751 for one past example.
  if (updating_web_preferences_)
    return;
  updating_web_preferences_ = true;
  UpdateWebkitPreferences(ComputeWebkitPrefs());
  updating_web_preferences_ = false;
}

void RenderViewHostImpl::ClearFocusedElement() {
  // TODO(ekaramad): We should move this to WebContents instead
  // (https://crbug.com/675975).
  if (delegate_)
    delegate_->ClearFocusedElement();
}

bool RenderViewHostImpl::IsFocusedElementEditable() {
  // TODO(ekaramad): We should move this to WebContents instead
  // (https://crbug.com/675975).
  return delegate_ && delegate_->IsFocusedElementEditable();
}

void RenderViewHostImpl::Zoom(PageZoom zoom) {
  Send(new ViewMsg_Zoom(GetRoutingID(), zoom));
}

void RenderViewHostImpl::DisableScrollbarsForThreshold(const gfx::Size& size) {
  Send(new ViewMsg_DisableScrollbarsForSmallWindows(GetRoutingID(), size));
}

void RenderViewHostImpl::EnablePreferredSizeMode() {
  Send(new ViewMsg_EnablePreferredSizeChangedMode(GetRoutingID()));
}

void RenderViewHostImpl::EnableAutoResize(const gfx::Size& min_size,
                                          const gfx::Size& max_size) {
  GetWidget()->SetAutoResize(true, min_size, max_size);
  Send(new ViewMsg_EnableAutoResize(GetRoutingID(), min_size, max_size));
}

void RenderViewHostImpl::DisableAutoResize(const gfx::Size& new_size) {
  GetWidget()->SetAutoResize(false, gfx::Size(), gfx::Size());
  Send(new ViewMsg_DisableAutoResize(GetRoutingID(), new_size));
  if (!new_size.IsEmpty())
    GetWidget()->GetView()->SetSize(new_size);
}

void RenderViewHostImpl::ExecuteMediaPlayerActionAtLocation(
  const gfx::Point& location, const blink::WebMediaPlayerAction& action) {
  Send(new ViewMsg_MediaPlayerActionAt(GetRoutingID(), location, action));
}

void RenderViewHostImpl::ExecutePluginActionAtLocation(
  const gfx::Point& location, const blink::WebPluginAction& action) {
  Send(new ViewMsg_PluginActionAt(GetRoutingID(), location, action));
}

void RenderViewHostImpl::NotifyMoveOrResizeStarted() {
  Send(new ViewMsg_MoveOrResizeStarted(GetRoutingID()));
}

void RenderViewHostImpl::OnDidZoomURL(double zoom_level,
                                      const GURL& url) {
  HostZoomMapImpl* host_zoom_map =
      static_cast<HostZoomMapImpl*>(HostZoomMap::Get(GetSiteInstance()));

  host_zoom_map->SetZoomLevelForView(GetProcess()->GetID(),
                                     GetRoutingID(),
                                     zoom_level,
                                     net::GetHostOrSpecFromURL(url));
}

void RenderViewHostImpl::SelectWordAroundCaret() {
  Send(new ViewMsg_SelectWordAroundCaret(GetRoutingID()));
}

void RenderViewHostImpl::PostRenderViewReady() {
  if (GetProcess()->IsReady()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&RenderViewHostImpl::RenderViewReady,
                   weak_factory_.GetWeakPtr()));
  } else {
    render_view_ready_on_process_launch_ = true;
  }
}

void RenderViewHostImpl::RenderViewReady() {
  delegate_->RenderViewReady(this);
}

}  // namespace content
