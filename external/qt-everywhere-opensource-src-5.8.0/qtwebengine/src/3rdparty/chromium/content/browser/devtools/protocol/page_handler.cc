// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/page_handler.h"

#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/threading/worker_pool.h"
#include "content/browser/devtools/protocol/color_picker.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/referrer.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/snapshot/snapshot.h"
#include "url/gurl.h"

namespace content {
namespace devtools {
namespace page {

namespace {

static const char kPng[] = "png";
static const char kJpeg[] = "jpeg";
static int kDefaultScreenshotQuality = 80;
static int kFrameRetryDelayMs = 100;
static int kCaptureRetryLimit = 2;
static int kMaxScreencastFramesInFlight = 2;

std::string EncodeScreencastFrame(const SkBitmap& bitmap,
                                  const std::string& format,
                                  int quality) {
  std::vector<unsigned char> data;
  SkAutoLockPixels lock_image(bitmap);
  bool encoded;
  if (format == kPng) {
    encoded = gfx::PNGCodec::Encode(
        reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
        gfx::PNGCodec::FORMAT_SkBitmap,
        gfx::Size(bitmap.width(), bitmap.height()),
        bitmap.width() * bitmap.bytesPerPixel(),
        false, std::vector<gfx::PNGCodec::Comment>(), &data);
  } else if (format == kJpeg) {
    encoded = gfx::JPEGCodec::Encode(
        reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
        gfx::JPEGCodec::FORMAT_SkBitmap,
        bitmap.width(),
        bitmap.height(),
        bitmap.width() * bitmap.bytesPerPixel(),
        quality, &data);
  } else {
    encoded = false;
  }

  if (!encoded)
    return std::string();

  std::string base_64_data;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<char*>(&data[0]), data.size()),
      &base_64_data);

  return base_64_data;
}

}  // namespace

typedef DevToolsProtocolClient::Response Response;

PageHandler::PageHandler()
    : enabled_(false),
      screencast_enabled_(false),
      screencast_quality_(kDefaultScreenshotQuality),
      screencast_max_width_(-1),
      screencast_max_height_(-1),
      capture_every_nth_frame_(1),
      capture_retry_count_(0),
      has_compositor_frame_metadata_(false),
      session_id_(0),
      frame_counter_(0),
      frames_in_flight_(0),
      color_picker_(new ColorPicker(base::Bind(
          &PageHandler::OnColorPicked, base::Unretained(this)))),
      host_(nullptr),
      weak_factory_(this) {
}

PageHandler::~PageHandler() {
}

void PageHandler::SetRenderFrameHost(RenderFrameHostImpl* host) {
  if (host_ == host)
    return;

  RenderWidgetHostImpl* widget_host =
      host_ ? host_->GetRenderWidgetHost() : nullptr;
  if (widget_host) {
    registrar_.Remove(
        this,
        content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
        content::Source<RenderWidgetHost>(widget_host));
  }

  host_ = host;
  widget_host = host_ ? host_->GetRenderWidgetHost() : nullptr;
  color_picker_->SetRenderWidgetHost(widget_host);

  if (widget_host) {
    registrar_.Add(
        this,
        content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
        content::Source<RenderWidgetHost>(widget_host));
  }
}

void PageHandler::SetClient(std::unique_ptr<Client> client) {
  client_.swap(client);
}

void PageHandler::Detached() {
  Disable();
}

void PageHandler::OnSwapCompositorFrame(
    cc::CompositorFrameMetadata frame_metadata) {
  last_compositor_frame_metadata_ = std::move(frame_metadata);
  has_compositor_frame_metadata_ = true;

  if (screencast_enabled_)
    InnerSwapCompositorFrame();
  color_picker_->OnSwapCompositorFrame();
}

void PageHandler::OnSynchronousSwapCompositorFrame(
    cc::CompositorFrameMetadata frame_metadata) {
  if (has_compositor_frame_metadata_) {
    last_compositor_frame_metadata_ =
        std::move(next_compositor_frame_metadata_);
  } else {
    last_compositor_frame_metadata_ = frame_metadata.Clone();
  }
  next_compositor_frame_metadata_ = std::move(frame_metadata);

  has_compositor_frame_metadata_ = true;

  if (screencast_enabled_)
    InnerSwapCompositorFrame();
  color_picker_->OnSwapCompositorFrame();
}

void PageHandler::Observe(int type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  if (!screencast_enabled_)
    return;
  DCHECK(type == content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED);
  bool visible = *Details<bool>(details).ptr();
  NotifyScreencastVisibility(visible);
}

void PageHandler::DidAttachInterstitialPage() {
  if (!enabled_)
    return;
  client_->InterstitialShown(InterstitialShownParams::Create());
}

void PageHandler::DidDetachInterstitialPage() {
  if (!enabled_)
    return;
  client_->InterstitialHidden(InterstitialHiddenParams::Create());
}

Response PageHandler::Enable() {
  enabled_ = true;
  return Response::FallThrough();
}

Response PageHandler::Disable() {
  enabled_ = false;
  screencast_enabled_ = false;
  color_picker_->SetEnabled(false);
  return Response::FallThrough();
}

Response PageHandler::Reload(const bool* bypassCache,
                             const std::string* script_to_evaluate_on_load,
                             const std::string* script_preprocessor) {
  WebContentsImpl* web_contents = GetWebContents();
  if (!web_contents)
    return Response::InternalError("Could not connect to view");

  if (web_contents->IsCrashed() ||
      (web_contents->GetController().GetVisibleEntry() &&
       web_contents->GetController().GetVisibleEntry()->IsViewSourceMode())) {
    if (bypassCache && *bypassCache)
      web_contents->GetController().ReloadBypassingCache(false);
    else
      web_contents->GetController().Reload(false);
    return Response::OK();
  } else {
    // Handle reload in renderer except for crashed and view source mode.
    return Response::FallThrough();
  }
}

Response PageHandler::Navigate(const std::string& url,
                               FrameId* frame_id) {
  GURL gurl(url);
  if (!gurl.is_valid())
    return Response::InternalError("Cannot navigate to invalid URL");

  WebContentsImpl* web_contents = GetWebContents();
  if (!web_contents)
    return Response::InternalError("Could not connect to view");

  web_contents->GetController()
      .LoadURL(gurl, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  return Response::FallThrough();
}

Response PageHandler::GetNavigationHistory(int* current_index,
                                           NavigationEntries* entries) {
  WebContentsImpl* web_contents = GetWebContents();
  if (!web_contents)
    return Response::InternalError("Could not connect to view");

  NavigationController& controller = web_contents->GetController();
  *current_index = controller.GetCurrentEntryIndex();
  for (int i = 0; i != controller.GetEntryCount(); ++i) {
    entries->push_back(NavigationEntry::Create()
        ->set_id(controller.GetEntryAtIndex(i)->GetUniqueID())
        ->set_url(controller.GetEntryAtIndex(i)->GetURL().spec())
        ->set_title(
            base::UTF16ToUTF8(controller.GetEntryAtIndex(i)->GetTitle())));
  }
  return Response::OK();
}

Response PageHandler::NavigateToHistoryEntry(int entry_id) {
  WebContentsImpl* web_contents = GetWebContents();
  if (!web_contents)
    return Response::InternalError("Could not connect to view");

  NavigationController& controller = web_contents->GetController();
  for (int i = 0; i != controller.GetEntryCount(); ++i) {
    if (controller.GetEntryAtIndex(i)->GetUniqueID() == entry_id) {
      controller.GoToIndex(i);
      return Response::OK();
    }
  }

  return Response::InvalidParams("No entry with passed id");
}

Response PageHandler::CaptureScreenshot(DevToolsCommandId command_id) {
  if (!host_ || !host_->GetRenderWidgetHost())
    return Response::InternalError("Could not connect to view");

  host_->GetRenderWidgetHost()->GetSnapshotFromBrowser(
      base::Bind(&PageHandler::ScreenshotCaptured,
          weak_factory_.GetWeakPtr(), command_id));
  return Response::OK();
}

Response PageHandler::StartScreencast(const std::string* format,
                                      const int* quality,
                                      const int* max_width,
                                      const int* max_height,
                                      const int* every_nth_frame) {
  RenderWidgetHostImpl* widget_host =
      host_ ? host_->GetRenderWidgetHost() : nullptr;
  if (!widget_host)
    return Response::InternalError("Could not connect to view");

  screencast_enabled_ = true;
  screencast_format_ = format ? *format : kPng;
  screencast_quality_ = quality ? *quality : kDefaultScreenshotQuality;
  if (screencast_quality_ < 0 || screencast_quality_ > 100)
    screencast_quality_ = kDefaultScreenshotQuality;
  screencast_max_width_ = max_width ? *max_width : -1;
  screencast_max_height_ = max_height ? *max_height : -1;
  ++session_id_;
  frame_counter_ = 0;
  frames_in_flight_ = 0;
  capture_every_nth_frame_ =
      every_nth_frame && *every_nth_frame ? *every_nth_frame : 1;

  bool visible = !widget_host->is_hidden();
  NotifyScreencastVisibility(visible);
  if (visible) {
    if (has_compositor_frame_metadata_) {
      InnerSwapCompositorFrame();
    } else {
      widget_host->Send(
          new ViewMsg_ForceRedraw(widget_host->GetRoutingID(), 0));
    }
  }
  return Response::FallThrough();
}

Response PageHandler::StopScreencast() {
  screencast_enabled_ = false;
  return Response::FallThrough();
}

Response PageHandler::ScreencastFrameAck(int session_id) {
  if (session_id == session_id_)
    --frames_in_flight_;
  return Response::OK();
}

Response PageHandler::HandleJavaScriptDialog(bool accept,
                                             const std::string* prompt_text) {
  base::string16 prompt_override;
  if (prompt_text)
    prompt_override = base::UTF8ToUTF16(*prompt_text);

  WebContentsImpl* web_contents = GetWebContents();
  if (!web_contents)
    return Response::InternalError("Could not connect to view");

  JavaScriptDialogManager* manager =
      web_contents->GetDelegate()->GetJavaScriptDialogManager(web_contents);
  if (manager && manager->HandleJavaScriptDialog(
          web_contents, accept, prompt_text ? &prompt_override : nullptr)) {
    return Response::OK();
  }

  return Response::InternalError("Could not handle JavaScript dialog");
}

Response PageHandler::QueryUsageAndQuota(DevToolsCommandId command_id,
                                         const std::string& security_origin) {
  return Response::OK();
}

Response PageHandler::SetColorPickerEnabled(bool enabled) {
  if (!host_)
    return Response::InternalError("Could not connect to view");

  color_picker_->SetEnabled(enabled);
  return Response::OK();
}

Response PageHandler::RequestAppBanner() {
  WebContentsImpl* web_contents = GetWebContents();
  if (!web_contents)
    return Response::InternalError("Could not connect to view");
  web_contents->GetDelegate()->RequestAppBannerFromDevTools(web_contents);
  return Response::OK();
}

WebContentsImpl* PageHandler::GetWebContents() {
  return host_ ?
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(host_)) :
      nullptr;
}

void PageHandler::NotifyScreencastVisibility(bool visible) {
  if (visible)
    capture_retry_count_ = kCaptureRetryLimit;
  client_->ScreencastVisibilityChanged(
      ScreencastVisibilityChangedParams::Create()->set_visible(visible));
}

void PageHandler::InnerSwapCompositorFrame() {
  if (!host_ || !host_->GetView())
    return;

  if (frames_in_flight_ > kMaxScreencastFramesInFlight)
    return;

  if (++frame_counter_ % capture_every_nth_frame_)
    return;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      host_->GetView());
  // TODO(vkuzkokov): do not use previous frame metadata.
  cc::CompositorFrameMetadata& metadata = last_compositor_frame_metadata_;

  gfx::SizeF viewport_size_dip = gfx::ScaleSize(
      metadata.scrollable_viewport_size, metadata.page_scale_factor);
  gfx::SizeF screen_size_dip =
      gfx::ScaleSize(gfx::SizeF(view->GetPhysicalBackingSize()),
                     1 / metadata.device_scale_factor);

  blink::WebScreenInfo screen_info;
  view->GetScreenInfo(&screen_info);
  double device_scale_factor = screen_info.deviceScaleFactor;
  double scale = 1;

  if (screencast_max_width_ > 0) {
    double max_width_dip = screencast_max_width_ / device_scale_factor;
    scale = std::min(scale, max_width_dip / screen_size_dip.width());
  }
  if (screencast_max_height_ > 0) {
    double max_height_dip = screencast_max_height_ / device_scale_factor;
    scale = std::min(scale, max_height_dip / screen_size_dip.height());
  }

  if (scale <= 0)
    scale = 0.1;

  gfx::Size snapshot_size_dip(gfx::ToRoundedSize(
      gfx::ScaleSize(viewport_size_dip, scale)));

  if (snapshot_size_dip.width() > 0 && snapshot_size_dip.height() > 0) {
    gfx::Rect viewport_bounds_dip(gfx::ToRoundedSize(viewport_size_dip));
    view->CopyFromCompositingSurface(
        viewport_bounds_dip, snapshot_size_dip,
        base::Bind(&PageHandler::ScreencastFrameCaptured,
                   weak_factory_.GetWeakPtr(),
                   base::Passed(last_compositor_frame_metadata_.Clone())),
        kN32_SkColorType);
    frames_in_flight_++;
  }
}

void PageHandler::ScreencastFrameCaptured(cc::CompositorFrameMetadata metadata,
                                          const SkBitmap& bitmap,
                                          ReadbackResponse response) {
  if (response != READBACK_SUCCESS) {
    if (capture_retry_count_) {
      --capture_retry_count_;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, base::Bind(&PageHandler::InnerSwapCompositorFrame,
                                weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kFrameRetryDelayMs));
    }
    --frames_in_flight_;
    return;
  }
  base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true).get(), FROM_HERE,
      base::Bind(&EncodeScreencastFrame, bitmap, screencast_format_,
                 screencast_quality_),
      base::Bind(&PageHandler::ScreencastFrameEncoded,
                 weak_factory_.GetWeakPtr(), base::Passed(&metadata),
                 base::Time::Now()));
}

void PageHandler::ScreencastFrameEncoded(cc::CompositorFrameMetadata metadata,
                                         const base::Time& timestamp,
                                         const std::string& data) {
  // Consider metadata empty in case it has no device scale factor.
  if (metadata.device_scale_factor == 0 || !host_ || data.empty()) {
    --frames_in_flight_;
    return;
  }

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      host_->GetView());
  if (!view) {
    --frames_in_flight_;
    return;
  }

  gfx::SizeF screen_size_dip =
      gfx::ScaleSize(gfx::SizeF(view->GetPhysicalBackingSize()),
                     1 / metadata.device_scale_factor);
  scoped_refptr<ScreencastFrameMetadata> param_metadata =
      ScreencastFrameMetadata::Create()
          ->set_page_scale_factor(metadata.page_scale_factor)
          ->set_offset_top(metadata.location_bar_content_translation.y())
          ->set_device_width(screen_size_dip.width())
          ->set_device_height(screen_size_dip.height())
          ->set_scroll_offset_x(metadata.root_scroll_offset.x())
          ->set_scroll_offset_y(metadata.root_scroll_offset.y())
          ->set_timestamp(timestamp.ToDoubleT());
  client_->ScreencastFrame(ScreencastFrameParams::Create()
      ->set_data(data)
      ->set_metadata(param_metadata)
      ->set_session_id(session_id_));
}

void PageHandler::ScreenshotCaptured(DevToolsCommandId command_id,
                                     const unsigned char* png_data,
                                     size_t png_size) {
  if (!png_data || !png_size) {
    client_->SendError(command_id,
                       Response::InternalError("Unable to capture screenshot"));
    return;
  }

  std::string base_64_data;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(png_data), png_size),
      &base_64_data);

  client_->SendCaptureScreenshotResponse(command_id,
      CaptureScreenshotResponse::Create()->set_data(base_64_data));
}

void PageHandler::OnColorPicked(int r, int g, int b, int a) {
  scoped_refptr<dom::RGBA> color =
      dom::RGBA::Create()->set_r(r)->set_g(g)->set_b(b)->set_a(a);
  client_->ColorPicked(ColorPickedParams::Create()->set_color(color));
}

}  // namespace page
}  // namespace devtools
}  // namespace content
