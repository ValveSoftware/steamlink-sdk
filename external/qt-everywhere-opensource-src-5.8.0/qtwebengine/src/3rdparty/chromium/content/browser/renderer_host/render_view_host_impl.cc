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
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/content_switches_internal.h"
#include "content/common/drag_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/speech_recognition_messages.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/focused_node_details.h"
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
#include "content/public/common/drop_data.h"
#include "content/public/common/file_chooser_file_info.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "net/base/filename_util.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/fileapi/isolated_context.h"
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
#include "ui/display/win/dpi.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/platform_font_win.h"
#endif

using base::TimeDelta;
using blink::WebConsoleMessage;
using blink::WebDragOperation;
using blink::WebDragOperationNone;
using blink::WebDragOperationsMask;
using blink::WebInputEvent;
using blink::WebMediaPlayerAction;
using blink::WebPluginAction;

namespace content {
namespace {

#if defined(OS_WIN)
void GetWindowsSpecificPrefs(RendererPreferences* prefs) {
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
      display::win::GetSystemMetricsInDIP(SM_CXVSCROLL);
  prefs->horizontal_scroll_bar_height_in_dips =
      display::win::GetSystemMetricsInDIP(SM_CYHSCROLL);
  prefs->arrow_bitmap_height_vertical_scroll_bar_in_dips =
      display::win::GetSystemMetricsInDIP(SM_CYVSCROLL);
  prefs->arrow_bitmap_width_horizontal_scroll_bar_in_dips =
      display::win::GetSystemMetricsInDIP(SM_CXHSCROLL);
}
#endif

std::vector<DropData::Metadata> DropDataToMetaData(const DropData& drop_data) {
  std::vector<DropData::Metadata> metadata;
  if (!drop_data.text.is_null()) {
    metadata.push_back(DropData::Metadata::CreateForMimeType(
        DropData::Kind::STRING,
        base::ASCIIToUTF16(ui::Clipboard::kMimeTypeText)));
  }

  if (drop_data.url.is_valid()) {
    metadata.push_back(DropData::Metadata::CreateForMimeType(
        DropData::Kind::STRING,
        base::ASCIIToUTF16(ui::Clipboard::kMimeTypeURIList)));
  }

  if (!drop_data.html.is_null()) {
    metadata.push_back(DropData::Metadata::CreateForMimeType(
        DropData::Kind::STRING,
        base::ASCIIToUTF16(ui::Clipboard::kMimeTypeHTML)));
  }

  // On Aura, filenames are available before drop.
  for (const auto& file_info : drop_data.filenames) {
    if (!file_info.path.empty()) {
      metadata.push_back(DropData::Metadata::CreateForFilePath(file_info.path));
    }
  }

  // On Android, only files' mime types are available before drop.
  for (const auto& mime_type : drop_data.file_mime_types) {
    if (!mime_type.empty()) {
      metadata.push_back(DropData::Metadata::CreateForMimeType(
          DropData::Kind::FILENAME, mime_type));
    }
  }

  for (const auto& file_system_file : drop_data.file_system_files) {
    if (!file_system_file.url.is_empty()) {
      metadata.push_back(
          DropData::Metadata::CreateForFileSystemUrl(file_system_file.url));
    }
  }

  for (const auto& custom_data_item : drop_data.custom_data) {
    metadata.push_back(DropData::Metadata::CreateForMimeType(
        DropData::Kind::STRING, custom_data_item.first));
  }

  return metadata;
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
      page_id_(-1),
      is_active_(!swapped_out),
      is_swapped_out_(swapped_out),
      main_frame_routing_id_(main_frame_routing_id),
      is_waiting_for_close_ack_(false),
      sudden_termination_allowed_(false),
      render_view_termination_status_(base::TERMINATION_STATUS_STILL_RUNNING),
      is_focused_element_editable_(false),
      updating_web_preferences_(false),
      render_view_ready_on_process_launch_(false),
      weak_factory_(this) {
  DCHECK(instance_.get());
  CHECK(delegate_);  // http://crbug.com/82827

  GetWidget()->set_owner_delegate(this);

  GetProcess()->AddObserver(this);
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
    int32_t max_page_id,
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
  // https://crbug.com/574245.
  // TODO(creis): Remove this once we've found the cause.
  if (main_frame_routing_id_ != MSG_ROUTING_NONE &&
      proxy_route_id != MSG_ROUTING_NONE)
    base::debug::DumpWithoutCrashing();

  GetWidget()->set_renderer_initialized(true);

  // Ensure the RenderView starts with a next_page_id larger than any existing
  // page ID it might be asked to render.
  int32_t next_page_id = 1;
  if (max_page_id > -1)
    next_page_id = max_page_id + 1;

  ViewMsg_New_Params params;
  params.renderer_preferences =
      delegate_->GetRendererPrefs(GetProcess()->GetBrowserContext());
#if defined(OS_WIN)
  GetWindowsSpecificPrefs(&params.renderer_preferences);
#endif
  params.web_preferences = GetWebkitPreferences();
  params.view_id = GetRoutingID();
  params.main_frame_routing_id = main_frame_routing_id_;
  if (main_frame_routing_id_ != MSG_ROUTING_NONE) {
    RenderFrameHostImpl* main_rfh = RenderFrameHostImpl::FromID(
        GetProcess()->GetID(), main_frame_routing_id_);
    DCHECK(main_rfh);
    RenderWidgetHostImpl* main_rwh = main_rfh->GetRenderWidgetHost();
    params.main_frame_widget_routing_id = main_rwh->GetRoutingID();
  }
  params.session_storage_namespace_id =
      delegate_->GetSessionStorageNamespace(instance_.get())->id();
  // Ensure the RenderView sets its opener correctly.
  params.opener_frame_route_id = opener_frame_route_id;
  params.swapped_out = !is_active_;
  params.replicated_frame_state = replicated_frame_state;
  params.proxy_routing_id = proxy_route_id;
  params.hidden = GetWidget()->is_hidden();
  params.never_visible = delegate_->IsNeverVisible();
  params.window_was_created_with_opener = window_was_created_with_opener;
  params.next_page_id = next_page_id;
  params.enable_auto_resize = GetWidget()->auto_resize_enabled();
  params.min_size = GetWidget()->min_size_for_auto_resize();
  params.max_size = GetWidget()->max_size_for_auto_resize();
  params.page_zoom_level = delegate_->GetPendingPageZoomLevel();
  params.image_decode_color_profile =
      gfx::ColorSpace::FromBestMonitor().GetICCProfile();
  GetWidget()->GetResizeParams(&params.initial_size);

  if (!Send(new ViewMsg_New(params)))
    return false;
  GetWidget()->SetInitialRenderSizeParams(params.initial_size);

  // If the RWHV has not yet been set, the surface ID namespace will get
  // passed down by the call to SetView().
  if (GetWidget()->GetView()) {
    Send(new ViewMsg_SetSurfaceIdNamespace(
        GetRoutingID(), GetWidget()->GetView()->GetSurfaceIdNamespace()));
  }

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
#if defined(OS_WIN)
  GetWindowsSpecificPrefs(&renderer_preferences);
#endif
  Send(new ViewMsg_SetRendererPrefs(GetRoutingID(), renderer_preferences));
}

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
      command_line.HasSwitch(switches::kEnable2dCanvasClipAntialiasing);
  prefs.accelerated_2d_canvas_msaa_sample_count =
      atoi(command_line.GetSwitchValueASCII(
      switches::kAcceleratedCanvas2dMSAASampleCount).c_str());

  prefs.inert_visual_viewport =
      command_line.HasSwitch(switches::kInertVisualViewport);

  prefs.pinch_overlay_scrollbar_thickness = 10;
  prefs.use_solid_color_scrollbars = ui::IsOverlayScrollbarEnabled();

#if defined(OS_ANDROID)
  // On Android, user gestures are normally required, unless that requirement
  // is disabled with a command-line switch or the equivalent field trial is
  // is set to "Enabled".
  const std::string autoplay_group_name = base::FieldTrialList::FindFullName(
      "MediaElementAutoplay");
  prefs.user_gesture_required_for_media_playback = !command_line.HasSwitch(
      switches::kDisableGestureRequirementForMediaPlayback) &&
          (autoplay_group_name.empty() || autoplay_group_name != "Enabled");
  prefs.autoplay_muted_videos_enabled =
      base::FeatureList::IsEnabled(features::kAutoplayMutedVideos);

  prefs.progress_bar_completion = GetProgressBarCompletionPolicy();
#endif

  // Handle autoplay gesture override experiment.
  // Note that anything but a well-formed string turns the experiment off.
  prefs.autoplay_experiment_mode = base::FieldTrialList::FindFullName(
      "MediaElementGestureOverrideExperiment");

  prefs.touch_enabled = ui::AreTouchEventsEnabled();
  prefs.device_supports_touch = prefs.touch_enabled &&
      ui::GetTouchScreensAvailability() ==
          ui::TouchScreensAvailability::ENABLED;
  prefs.available_pointer_types = ui::GetAvailablePointerTypes();
  prefs.primary_pointer_type = ui::GetPrimaryPointerType();
  prefs.available_hover_types = ui::GetAvailableHoverTypes();
  prefs.primary_hover_type = ui::GetPrimaryHoverType();

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

  prefs.image_color_profiles_enabled =
      command_line.HasSwitch(switches::kEnableImageColorProfiles);

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

  GetContentClient()->browser()->OverrideWebkitPrefs(this, &prefs);
  return prefs;
}

void RenderViewHostImpl::ClosePage() {
  is_waiting_for_close_ack_ = true;
  GetWidget()->StartHangMonitorTimeout(
      TimeDelta::FromMilliseconds(kUnloadTimeoutMS),
      RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_CLOSE_PAGE);

  if (IsRenderViewLive()) {
    // Since we are sending an IPC message to the renderer, increase the event
    // count to prevent the hang monitor timeout from being stopped by input
    // event acknowledgements.
    GetWidget()->increment_in_flight_event_count();

    // TODO(creis): Should this be moved to Shutdown?  It may not be called for
    // RenderViewHosts that have been swapped out.
    NotificationService::current()->Notify(
        NOTIFICATION_RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW,
        Source<RenderViewHost>(this),
        NotificationService::NoDetails());

    Send(new ViewMsg_ClosePage(GetRoutingID()));
  } else {
    // This RenderViewHost doesn't have a live renderer, so just skip the unload
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

void RenderViewHostImpl::DragTargetDragEnter(
    const DropData& drop_data,
    const gfx::Point& client_pt,
    const gfx::Point& screen_pt,
    WebDragOperationsMask operations_allowed,
    int key_modifiers) {
  DragTargetDragEnterWithMetaData(DropDataToMetaData(drop_data), client_pt,
                                  screen_pt, operations_allowed, key_modifiers);
}

void RenderViewHostImpl::DragTargetDragEnterWithMetaData(
    const std::vector<DropData::Metadata>& metadata,
    const gfx::Point& client_pt,
    const gfx::Point& screen_pt,
    WebDragOperationsMask operations_allowed,
    int key_modifiers) {
  Send(new DragMsg_TargetDragEnter(GetRoutingID(), metadata, client_pt,
                                   screen_pt, operations_allowed,
                                   key_modifiers));
}

void RenderViewHostImpl::DragTargetDragOver(
    const gfx::Point& client_pt,
    const gfx::Point& screen_pt,
    WebDragOperationsMask operations_allowed,
    int key_modifiers) {
  Send(new DragMsg_TargetDragOver(GetRoutingID(), client_pt, screen_pt,
                                  operations_allowed, key_modifiers));
}

void RenderViewHostImpl::DragTargetDragLeave() {
  Send(new DragMsg_TargetDragLeave(GetRoutingID()));
}

void RenderViewHostImpl::DragTargetDrop(const DropData& drop_data,
                                        const gfx::Point& client_pt,
                                        const gfx::Point& screen_pt,
                                        int key_modifiers) {
  DropData drop_data_with_permissions(drop_data);
  GrantFileAccessFromDropData(&drop_data_with_permissions);
  Send(new DragMsg_TargetDrop(GetRoutingID(), drop_data_with_permissions,
                              client_pt, screen_pt, key_modifiers));
}

void RenderViewHostImpl::FilterDropData(DropData* drop_data) {
#if DCHECK_IS_ON()
  drop_data->view_id = GetRoutingID();
#endif  // DCHECK_IS_ON()

  GetProcess()->FilterURL(true, &drop_data->url);
  if (drop_data->did_originate_from_renderer) {
    drop_data->filenames.clear();
  }
}

void RenderViewHostImpl::DragSourceEndedAt(
    int client_x, int client_y, int screen_x, int screen_y,
    WebDragOperation operation) {
  Send(new DragMsg_SourceEnded(GetRoutingID(),
                               gfx::Point(client_x, client_y),
                               gfx::Point(screen_x, screen_y),
                               operation));
}

void RenderViewHostImpl::DragSourceSystemDragEnded() {
  Send(new DragMsg_SourceSystemDragEnded(GetRoutingID()));
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

void RenderViewHostImpl::LoadStateChanged(
    const GURL& url,
    const net::LoadStateWithParam& load_state,
    uint64_t upload_position,
    uint64_t upload_size) {
  delegate_->LoadStateChanged(url, load_state, upload_position, upload_size);
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
    IPC_MESSAGE_HANDLER(DragHostMsg_StartDragging, OnStartDragging)
    IPC_MESSAGE_HANDLER(DragHostMsg_UpdateDragCursor, OnUpdateDragCursor)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FocusedNodeChanged, OnFocusedNodeChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClosePage_ACK, OnClosePageACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidZoomURL, OnDidZoomURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Focus, OnFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FocusedNodeTouched, OnFocusedNodeTouched)
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
    const ViewHostMsg_CreateWindow_Params& params,
    SessionStorageNamespace* session_storage_namespace) {
  ViewHostMsg_CreateWindow_Params validated_params(params);
  GetProcess()->FilterURL(false, &validated_params.target_url);
  GetProcess()->FilterURL(false, &validated_params.opener_url);
  GetProcess()->FilterURL(true, &validated_params.opener_security_origin);

  delegate_->CreateNewWindow(GetSiteInstance(), route_id, main_frame_route_id,
                             main_frame_widget_route_id, validated_params,
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

void RenderViewHostImpl::OnUpdateState(int32_t page_id,
                                       const PageState& state) {
  // If the following DCHECK fails, you have encountered a tricky edge-case that
  // has evaded reproduction for a very long time. Please report what you were
  // doing on http://crbug.com/407376, whether or not you can reproduce the
  // failure.
  DCHECK_EQ(page_id, page_id_);

  // Without this check, the renderer can trick the browser into using
  // filenames it can't access in a future session restore.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  int child_id = GetProcess()->GetID();
  if (!policy->CanReadAllFiles(child_id, state.GetReferencedFiles())) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RVH_CAN_ACCESS_FILES_OF_PAGE_STATE);
    return;
  }

  delegate_->UpdateState(this, page_id, state);
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

void RenderViewHostImpl::OnStartDragging(
    const DropData& drop_data,
    WebDragOperationsMask drag_operations_mask,
    const SkBitmap& bitmap,
    const gfx::Vector2d& bitmap_offset_in_dip,
    const DragEventSourceInfo& event_info) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (!view)
    return;

  DropData filtered_data(drop_data);
  RenderProcessHost* process = GetProcess();
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Allow drag of Javascript URLs to enable bookmarklet drag to bookmark bar.
  if (!filtered_data.url.SchemeIs(url::kJavaScriptScheme))
    process->FilterURL(true, &filtered_data.url);
  process->FilterURL(false, &filtered_data.html_base_url);
  // Filter out any paths that the renderer didn't have access to. This prevents
  // the following attack on a malicious renderer:
  // 1. StartDragging IPC sent with renderer-specified filesystem paths that it
  //    doesn't have read permissions for.
  // 2. We initiate a native DnD operation.
  // 3. DnD operation immediately ends since mouse is not held down. DnD events
  //    still fire though, which causes read permissions to be granted to the
  //    renderer for any file paths in the drop.
  filtered_data.filenames.clear();
  for (std::vector<ui::FileInfo>::const_iterator it =
           drop_data.filenames.begin();
       it != drop_data.filenames.end();
       ++it) {
    if (policy->CanReadFile(GetProcess()->GetID(), it->path))
      filtered_data.filenames.push_back(*it);
  }

  storage::FileSystemContext* file_system_context =
      BrowserContext::GetStoragePartition(GetProcess()->GetBrowserContext(),
                                          GetSiteInstance())
          ->GetFileSystemContext();
  filtered_data.file_system_files.clear();
  for (size_t i = 0; i < drop_data.file_system_files.size(); ++i) {
    storage::FileSystemURL file_system_url =
        file_system_context->CrackURL(drop_data.file_system_files[i].url);
    if (policy->CanReadFileSystemFile(GetProcess()->GetID(), file_system_url))
      filtered_data.file_system_files.push_back(drop_data.file_system_files[i]);
  }

  float scale = GetScaleFactorForView(GetWidget()->GetView());
  gfx::ImageSkia image(gfx::ImageSkiaRep(bitmap, scale));
  view->StartDragging(filtered_data, drag_operations_mask, image,
      bitmap_offset_in_dip, event_info);
}

void RenderViewHostImpl::OnUpdateDragCursor(WebDragOperation current_op) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->UpdateDragCursor(current_op);
}

void RenderViewHostImpl::OnTakeFocus(bool reverse) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->TakeFocus(reverse);
}

void RenderViewHostImpl::OnFocusedNodeChanged(
    bool is_editable_node,
    const gfx::Rect& node_bounds_in_viewport) {
  is_focused_element_editable_ = is_editable_node;
  if (GetWidget()->GetView())
    GetWidget()->GetView()->FocusedNodeChanged(is_editable_node);

  // None of the rest makes sense without a view.
  if (!GetWidget()->GetView())
    return;

  // Convert node_bounds to screen coordinates.
  gfx::Rect view_bounds_in_screen = GetWidget()->GetView()->GetViewBounds();
  gfx::Point origin = node_bounds_in_viewport.origin();
  origin.Offset(view_bounds_in_screen.x(), view_bounds_in_screen.y());
  gfx::Rect node_bounds_in_screen(origin.x(), origin.y(),
                                  node_bounds_in_viewport.width(),
                                  node_bounds_in_viewport.height());
  FocusedNodeDetails details = {is_editable_node, node_bounds_in_screen};
  NotificationService::current()->Notify(NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                                         Source<RenderViewHost>(this),
                                         Details<FocusedNodeDetails>(&details));
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

void RenderViewHostImpl::OnTextSurroundingSelectionResponse(
    const base::string16& content,
    size_t start_offset,
    size_t end_offset) {
  if (!GetWidget()->GetView())
    return;
  GetWidget()->GetView()->OnTextSurroundingSelectionResponse(
      content, start_offset, end_offset);
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
  is_focused_element_editable_ = false;
  Send(new ViewMsg_ClearFocusedElement(GetRoutingID()));
}

bool RenderViewHostImpl::IsFocusedElementEditable() {
  return is_focused_element_editable_;
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

void RenderViewHostImpl::OnFocusedNodeTouched(bool editable) {
#if defined(OS_WIN)
  // We use the cursor position to determine where the touch occurred.
  // TODO(ananta)
  // Pass this information from blink.
  // In site isolation mode, we may not have a RenderViewHostImpl instance
  // which means that displaying the OSK is not going to work. We should
  // probably move this to RenderWidgetHostImpl and call the view from there.
  // https://bugs.chromium.org/p/chromium/issues/detail?id=613326
  POINT cursor_pos = {};
  ::GetCursorPos(&cursor_pos);
  float scale = GetScaleFactorForView(GetWidget()->GetView());
  gfx::Point location_dips_screen =
      gfx::ConvertPointToDIP(scale, gfx::Point(cursor_pos));
  if (GetWidget()->GetView())
    GetWidget()->GetView()->FocusedNodeTouched(location_dips_screen, editable);
#endif
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

void RenderViewHostImpl::GrantFileAccessFromDropData(DropData* drop_data) {
  DCHECK_EQ(GetRoutingID(), drop_data->view_id);
  const int renderer_id = GetProcess()->GetID();
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

#if defined(OS_CHROMEOS)
  // The externalfile:// scheme is used in Chrome OS to open external files in a
  // browser tab.
  if (drop_data->url.SchemeIs(content::kExternalFileScheme))
    policy->GrantRequestURL(renderer_id, drop_data->url);
#endif

  // The filenames vector represents a capability to access the given files.
  storage::IsolatedContext::FileInfoSet files;
  for (auto& filename : drop_data->filenames) {
    // Make sure we have the same display_name as the one we register.
    if (filename.display_name.empty()) {
      std::string name;
      files.AddPath(filename.path, &name);
      filename.display_name = base::FilePath::FromUTF8Unsafe(name);
    } else {
      files.AddPathWithName(filename.path,
                            filename.display_name.AsUTF8Unsafe());
    }
    // A dragged file may wind up as the value of an input element, or it
    // may be used as the target of a navigation instead.  We don't know
    // which will happen at this point, so generously grant both access
    // and request permissions to the specific file to cover both cases.
    // We do not give it the permission to request all file:// URLs.
    policy->GrantRequestSpecificFileURL(renderer_id,
                                        net::FilePathToFileURL(filename.path));

    // If the renderer already has permission to read these paths, we don't need
    // to re-grant them. This prevents problems with DnD for files in the CrOS
    // file manager--the file manager already had read/write access to those
    // directories, but dragging a file would cause the read/write access to be
    // overwritten with read-only access, making them impossible to delete or
    // rename until the renderer was killed.
    if (!policy->CanReadFile(renderer_id, filename.path))
      policy->GrantReadFile(renderer_id, filename.path);
  }

  storage::IsolatedContext* isolated_context =
      storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  if (!files.fileset().empty()) {
    std::string filesystem_id =
        isolated_context->RegisterDraggedFileSystem(files);
    if (!filesystem_id.empty()) {
      // Grant the permission iff the ID is valid.
      policy->GrantReadFileSystem(renderer_id, filesystem_id);
    }
    drop_data->filesystem_id = base::UTF8ToUTF16(filesystem_id);
  }

  storage::FileSystemContext* file_system_context =
      BrowserContext::GetStoragePartition(GetProcess()->GetBrowserContext(),
                                          GetSiteInstance())
          ->GetFileSystemContext();
  for (auto& file_system_file : drop_data->file_system_files) {
    storage::FileSystemURL file_system_url =
        file_system_context->CrackURL(file_system_file.url);

    std::string register_name;
    std::string filesystem_id = isolated_context->RegisterFileSystemForPath(
        file_system_url.type(), file_system_url.filesystem_id(),
        file_system_url.path(), &register_name);

    if (!filesystem_id.empty()) {
      // Grant the permission iff the ID is valid.
      policy->GrantReadFileSystem(renderer_id, filesystem_id);
    }

    // Note: We are using the origin URL provided by the sender here. It may be
    // different from the receiver's.
    file_system_file.url =
        GURL(storage::GetIsolatedFileSystemRootURIString(
                 file_system_url.origin(), filesystem_id, std::string())
                 .append(register_name));
  }
}

}  // namespace content
