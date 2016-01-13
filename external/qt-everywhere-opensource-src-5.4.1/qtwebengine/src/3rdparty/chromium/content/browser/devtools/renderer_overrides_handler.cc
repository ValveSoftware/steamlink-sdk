// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/renderer_overrides_handler.h"

#include <map>
#include <string>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/browser/devtools/devtools_tracing_handler.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "ipc/ipc_sender.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"
#include "ui/snapshot/snapshot.h"
#include "url/gurl.h"
#include "webkit/browser/quota/quota_manager.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;

namespace content {

namespace {

static const char kPng[] = "png";
static const char kJpeg[] = "jpeg";
static int kDefaultScreenshotQuality = 80;
static int kFrameRateThresholdMs = 100;
static int kCaptureRetryLimit = 2;

void ParseGenericInputParams(base::DictionaryValue* params,
                             WebInputEvent* event) {
  int modifiers = 0;
  if (params->GetInteger(devtools::Input::dispatchMouseEvent::kParamModifiers,
                         &modifiers)) {
    if (modifiers & 1)
      event->modifiers |= WebInputEvent::AltKey;
    if (modifiers & 2)
      event->modifiers |= WebInputEvent::ControlKey;
    if (modifiers & 4)
      event->modifiers |= WebInputEvent::MetaKey;
    if (modifiers & 8)
      event->modifiers |= WebInputEvent::ShiftKey;
  }

  params->GetDouble(devtools::Input::dispatchMouseEvent::kParamTimestamp,
                    &event->timeStampSeconds);
}

}  // namespace

RendererOverridesHandler::RendererOverridesHandler(DevToolsAgentHost* agent)
    : agent_(agent),
      capture_retry_count_(0),
      weak_factory_(this) {
  RegisterCommandHandler(
      devtools::DOM::setFileInputFiles::kName,
      base::Bind(
          &RendererOverridesHandler::GrantPermissionsForSetFileInputFiles,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Network::clearBrowserCache::kName,
      base::Bind(
          &RendererOverridesHandler::ClearBrowserCache,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Network::clearBrowserCookies::kName,
      base::Bind(
          &RendererOverridesHandler::ClearBrowserCookies,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::disable::kName,
      base::Bind(
          &RendererOverridesHandler::PageDisable, base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::handleJavaScriptDialog::kName,
      base::Bind(
          &RendererOverridesHandler::PageHandleJavaScriptDialog,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::navigate::kName,
      base::Bind(
          &RendererOverridesHandler::PageNavigate,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::reload::kName,
      base::Bind(
          &RendererOverridesHandler::PageReload,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::getNavigationHistory::kName,
      base::Bind(
          &RendererOverridesHandler::PageGetNavigationHistory,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::navigateToHistoryEntry::kName,
      base::Bind(
          &RendererOverridesHandler::PageNavigateToHistoryEntry,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::captureScreenshot::kName,
      base::Bind(
          &RendererOverridesHandler::PageCaptureScreenshot,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::canScreencast::kName,
      base::Bind(
          &RendererOverridesHandler::PageCanScreencast,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::startScreencast::kName,
      base::Bind(
          &RendererOverridesHandler::PageStartScreencast,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::stopScreencast::kName,
      base::Bind(
          &RendererOverridesHandler::PageStopScreencast,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::queryUsageAndQuota::kName,
      base::Bind(
          &RendererOverridesHandler::PageQueryUsageAndQuota,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Input::dispatchMouseEvent::kName,
      base::Bind(
          &RendererOverridesHandler::InputDispatchMouseEvent,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Input::dispatchGestureEvent::kName,
      base::Bind(
          &RendererOverridesHandler::InputDispatchGestureEvent,
          base::Unretained(this)));
}

RendererOverridesHandler::~RendererOverridesHandler() {}

void RendererOverridesHandler::OnClientDetached() {
  screencast_command_ = NULL;
}

void RendererOverridesHandler::OnSwapCompositorFrame(
    const cc::CompositorFrameMetadata& frame_metadata) {
  last_compositor_frame_metadata_ = frame_metadata;

  if (screencast_command_)
    InnerSwapCompositorFrame();
}

void RendererOverridesHandler::OnVisibilityChanged(bool visible) {
  if (!screencast_command_)
    return;
  NotifyScreencastVisibility(visible);
}

void RendererOverridesHandler::InnerSwapCompositorFrame() {
  if ((base::TimeTicks::Now() - last_frame_time_).InMilliseconds() <
          kFrameRateThresholdMs) {
    return;
  }

  RenderViewHost* host = agent_->GetRenderViewHost();
  if (!host->GetView())
    return;

  last_frame_time_ = base::TimeTicks::Now();
  std::string format;
  int quality = kDefaultScreenshotQuality;
  double scale = 1;
  ParseCaptureParameters(screencast_command_.get(), &format, &quality, &scale);

  const gfx::Display& display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  float device_scale_factor = display.device_scale_factor();

  gfx::Rect view_bounds = host->GetView()->GetViewBounds();
  gfx::Size snapshot_size(gfx::ToCeiledSize(
      gfx::ScaleSize(view_bounds.size(), scale / device_scale_factor)));

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      host->GetView());
  view->CopyFromCompositingSurface(
      view_bounds, snapshot_size,
      base::Bind(&RendererOverridesHandler::ScreencastFrameCaptured,
                 weak_factory_.GetWeakPtr(),
                 format, quality, last_compositor_frame_metadata_),
      SkBitmap::kARGB_8888_Config);
}

void RendererOverridesHandler::ParseCaptureParameters(
    DevToolsProtocol::Command* command,
    std::string* format,
    int* quality,
    double* scale) {
  *quality = kDefaultScreenshotQuality;
  *scale = 1;
  double max_width = -1;
  double max_height = -1;
  base::DictionaryValue* params = command->params();
  if (params) {
    params->GetString(devtools::Page::startScreencast::kParamFormat,
                      format);
    params->GetInteger(devtools::Page::startScreencast::kParamQuality,
                       quality);
    params->GetDouble(devtools::Page::startScreencast::kParamMaxWidth,
                      &max_width);
    params->GetDouble(devtools::Page::startScreencast::kParamMaxHeight,
                      &max_height);
  }

  RenderViewHost* host = agent_->GetRenderViewHost();
  CHECK(host->GetView());
  gfx::Rect view_bounds = host->GetView()->GetViewBounds();
  if (max_width > 0)
    *scale = std::min(*scale, max_width / view_bounds.width());
  if (max_height > 0)
    *scale = std::min(*scale, max_height / view_bounds.height());

  if (format->empty())
    *format = kPng;
  if (*quality < 0 || *quality > 100)
    *quality = kDefaultScreenshotQuality;
  if (*scale <= 0)
    *scale = 0.1;
  if (*scale > 5)
    *scale = 5;
}

base::DictionaryValue* RendererOverridesHandler::CreateScreenshotResponse(
    const std::vector<unsigned char>& png_data) {
  std::string base_64_data;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(&png_data[0]),
                        png_data.size()),
      &base_64_data);

  base::DictionaryValue* response = new base::DictionaryValue();
  response->SetString(
      devtools::Page::captureScreenshot::kResponseData, base_64_data);
  return response;
}

// DOM agent handlers  --------------------------------------------------------

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::GrantPermissionsForSetFileInputFiles(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  base::ListValue* file_list = NULL;
  const char* param =
      devtools::DOM::setFileInputFiles::kParamFiles;
  if (!params || !params->GetList(param, &file_list))
    return command->InvalidParamResponse(param);
  RenderViewHost* host = agent_->GetRenderViewHost();
  if (!host)
    return NULL;

  for (size_t i = 0; i < file_list->GetSize(); ++i) {
    base::FilePath::StringType file;
    if (!file_list->GetString(i, &file))
      return command->InvalidParamResponse(param);
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
        host->GetProcess()->GetID(), base::FilePath(file));
  }
  return NULL;
}


// Network agent handlers  ----------------------------------------------------

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::ClearBrowserCache(
    scoped_refptr<DevToolsProtocol::Command> command) {
  GetContentClient()->browser()->ClearCache(agent_->GetRenderViewHost());
  return command->SuccessResponse(NULL);
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::ClearBrowserCookies(
    scoped_refptr<DevToolsProtocol::Command> command) {
  GetContentClient()->browser()->ClearCookies(agent_->GetRenderViewHost());
  return command->SuccessResponse(NULL);
}


// Page agent handlers  -------------------------------------------------------

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageDisable(
    scoped_refptr<DevToolsProtocol::Command> command) {
  screencast_command_ = NULL;
  return NULL;
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageHandleJavaScriptDialog(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  const char* paramAccept =
      devtools::Page::handleJavaScriptDialog::kParamAccept;
  bool accept;
  if (!params || !params->GetBoolean(paramAccept, &accept))
    return command->InvalidParamResponse(paramAccept);
  base::string16 prompt_override;
  base::string16* prompt_override_ptr = &prompt_override;
  if (!params || !params->GetString(
      devtools::Page::handleJavaScriptDialog::kParamPromptText,
      prompt_override_ptr)) {
    prompt_override_ptr = NULL;
  }

  RenderViewHost* host = agent_->GetRenderViewHost();
  if (host) {
    WebContents* web_contents = host->GetDelegate()->GetAsWebContents();
    if (web_contents) {
      JavaScriptDialogManager* manager =
          web_contents->GetDelegate()->GetJavaScriptDialogManager();
      if (manager && manager->HandleJavaScriptDialog(
              web_contents, accept, prompt_override_ptr)) {
        return command->SuccessResponse(new base::DictionaryValue());
      }
    }
  }
  return command->InternalErrorResponse("No JavaScript dialog to handle");
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageNavigate(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  std::string url;
  const char* param = devtools::Page::navigate::kParamUrl;
  if (!params || !params->GetString(param, &url))
    return command->InvalidParamResponse(param);
  GURL gurl(url);
  if (!gurl.is_valid()) {
    return command->InternalErrorResponse("Cannot navigate to invalid URL");
  }
  RenderViewHost* host = agent_->GetRenderViewHost();
  if (host) {
    WebContents* web_contents = host->GetDelegate()->GetAsWebContents();
    if (web_contents) {
      web_contents->GetController()
          .LoadURL(gurl, Referrer(), PAGE_TRANSITION_TYPED, std::string());
      return command->SuccessResponse(new base::DictionaryValue());
    }
  }
  return command->InternalErrorResponse("No WebContents to navigate");
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageReload(
    scoped_refptr<DevToolsProtocol::Command> command) {
  RenderViewHost* host = agent_->GetRenderViewHost();
  if (host) {
    WebContents* web_contents = host->GetDelegate()->GetAsWebContents();
    if (web_contents) {
      // Override only if it is crashed.
      if (!web_contents->IsCrashed())
        return NULL;

      web_contents->GetController().Reload(false);
      return command->SuccessResponse(NULL);
    }
  }
  return command->InternalErrorResponse("No WebContents to reload");
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageGetNavigationHistory(
    scoped_refptr<DevToolsProtocol::Command> command) {
  RenderViewHost* host = agent_->GetRenderViewHost();
  if (host) {
    WebContents* web_contents = host->GetDelegate()->GetAsWebContents();
    if (web_contents) {
      base::DictionaryValue* result = new base::DictionaryValue();
      NavigationController& controller = web_contents->GetController();
      result->SetInteger(
          devtools::Page::getNavigationHistory::kResponseCurrentIndex,
          controller.GetCurrentEntryIndex());
      base::ListValue* entries = new base::ListValue();
      for (int i = 0; i != controller.GetEntryCount(); ++i) {
        const NavigationEntry* entry = controller.GetEntryAtIndex(i);
        base::DictionaryValue* entry_value = new base::DictionaryValue();
        entry_value->SetInteger(
            devtools::Page::NavigationEntry::kParamId,
            entry->GetUniqueID());
        entry_value->SetString(
            devtools::Page::NavigationEntry::kParamUrl,
            entry->GetURL().spec());
        entry_value->SetString(
            devtools::Page::NavigationEntry::kParamTitle,
            entry->GetTitle());
        entries->Append(entry_value);
      }
      result->Set(
          devtools::Page::getNavigationHistory::kResponseEntries,
          entries);
      return command->SuccessResponse(result);
    }
  }
  return command->InternalErrorResponse("No WebContents to navigate");
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageNavigateToHistoryEntry(
    scoped_refptr<DevToolsProtocol::Command> command) {
  int entry_id;

  base::DictionaryValue* params = command->params();
  const char* param = devtools::Page::navigateToHistoryEntry::kParamEntryId;
  if (!params || !params->GetInteger(param, &entry_id)) {
    return command->InvalidParamResponse(param);
  }

  RenderViewHost* host = agent_->GetRenderViewHost();
  if (host) {
    WebContents* web_contents = host->GetDelegate()->GetAsWebContents();
    if (web_contents) {
      NavigationController& controller = web_contents->GetController();
      for (int i = 0; i != controller.GetEntryCount(); ++i) {
        if (controller.GetEntryAtIndex(i)->GetUniqueID() == entry_id) {
          controller.GoToIndex(i);
          return command->SuccessResponse(new base::DictionaryValue());
        }
      }
      return command->InvalidParamResponse(param);
    }
  }
  return command->InternalErrorResponse("No WebContents to navigate");
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageCaptureScreenshot(
    scoped_refptr<DevToolsProtocol::Command> command) {
  RenderViewHost* host = agent_->GetRenderViewHost();
  if (!host->GetView())
    return command->InternalErrorResponse("Unable to access the view");

  gfx::Rect view_bounds = host->GetView()->GetViewBounds();
  gfx::Rect snapshot_bounds(view_bounds.size());
  gfx::Size snapshot_size = snapshot_bounds.size();

  std::vector<unsigned char> png_data;
  if (ui::GrabViewSnapshot(host->GetView()->GetNativeView(),
                           &png_data,
                           snapshot_bounds)) {
    if (png_data.size())
      return command->SuccessResponse(CreateScreenshotResponse(png_data));
    else
      return command->InternalErrorResponse("Unable to capture screenshot");
  }

  ui::GrabViewSnapshotAsync(
      host->GetView()->GetNativeView(),
      snapshot_bounds,
      base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&RendererOverridesHandler::ScreenshotCaptured,
                 weak_factory_.GetWeakPtr(), command));
  return command->AsyncResponsePromise();
}

void RendererOverridesHandler::ScreenshotCaptured(
    scoped_refptr<DevToolsProtocol::Command> command,
    scoped_refptr<base::RefCountedBytes> png_data) {
  if (png_data) {
    SendAsyncResponse(
        command->SuccessResponse(CreateScreenshotResponse(png_data->data())));
  } else {
    SendAsyncResponse(
        command->InternalErrorResponse("Unable to capture screenshot"));
  }
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageCanScreencast(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* result = new base::DictionaryValue();
#if defined(OS_ANDROID)
  result->SetBoolean(devtools::kResult, true);
#else
  result->SetBoolean(devtools::kResult, false);
#endif  // defined(OS_ANDROID)
  return command->SuccessResponse(result);
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageStartScreencast(
    scoped_refptr<DevToolsProtocol::Command> command) {
  screencast_command_ = command;
  RenderViewHostImpl* host = static_cast<RenderViewHostImpl*>(
      agent_->GetRenderViewHost());
  bool visible = !host->is_hidden();
  NotifyScreencastVisibility(visible);
  if (visible)
    InnerSwapCompositorFrame();
  return command->SuccessResponse(NULL);
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageStopScreencast(
    scoped_refptr<DevToolsProtocol::Command> command) {
  last_frame_time_ = base::TimeTicks();
  screencast_command_ = NULL;
  return command->SuccessResponse(NULL);
}

void RendererOverridesHandler::ScreencastFrameCaptured(
    const std::string& format,
    int quality,
    const cc::CompositorFrameMetadata& metadata,
    bool success,
    const SkBitmap& bitmap) {
  if (!success) {
    if (capture_retry_count_) {
      --capture_retry_count_;
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&RendererOverridesHandler::InnerSwapCompositorFrame,
                     weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kFrameRateThresholdMs));
    }
    return;
  }

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
    return;

  std::string base_64_data;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<char*>(&data[0]), data.size()),
      &base_64_data);

  base::DictionaryValue* response = new base::DictionaryValue();
  response->SetString(devtools::Page::screencastFrame::kParamData,
                      base_64_data);

  // Consider metadata empty in case it has no device scale factor.
  if (metadata.device_scale_factor != 0) {
    base::DictionaryValue* response_metadata = new base::DictionaryValue();

    response_metadata->SetDouble(
        devtools::Page::ScreencastFrameMetadata::kParamDeviceScaleFactor,
        metadata.device_scale_factor);
    response_metadata->SetDouble(
        devtools::Page::ScreencastFrameMetadata::kParamPageScaleFactor,
        metadata.page_scale_factor);
    response_metadata->SetDouble(
        devtools::Page::ScreencastFrameMetadata::kParamPageScaleFactorMin,
        metadata.min_page_scale_factor);
    response_metadata->SetDouble(
        devtools::Page::ScreencastFrameMetadata::kParamPageScaleFactorMax,
        metadata.max_page_scale_factor);
    response_metadata->SetDouble(
        devtools::Page::ScreencastFrameMetadata::kParamOffsetTop,
        metadata.location_bar_content_translation.y());
    response_metadata->SetDouble(
        devtools::Page::ScreencastFrameMetadata::kParamOffsetBottom,
        metadata.overdraw_bottom_height);

    base::DictionaryValue* viewport = new base::DictionaryValue();
    viewport->SetDouble(devtools::DOM::Rect::kParamX,
                        metadata.root_scroll_offset.x());
    viewport->SetDouble(devtools::DOM::Rect::kParamY,
                        metadata.root_scroll_offset.y());
    viewport->SetDouble(devtools::DOM::Rect::kParamWidth,
                        metadata.viewport_size.width());
    viewport->SetDouble(devtools::DOM::Rect::kParamHeight,
                        metadata.viewport_size.height());
    response_metadata->Set(
        devtools::Page::ScreencastFrameMetadata::kParamViewport, viewport);

    response->Set(devtools::Page::screencastFrame::kParamMetadata,
                  response_metadata);
  }

  SendNotification(devtools::Page::screencastFrame::kName, response);
}

// Quota and Usage ------------------------------------------

namespace {

typedef base::Callback<void(scoped_ptr<base::DictionaryValue>)>
    ResponseCallback;

void QueryUsageAndQuotaCompletedOnIOThread(
    scoped_ptr<base::DictionaryValue> quota,
    scoped_ptr<base::DictionaryValue> usage,
    ResponseCallback callback) {

  scoped_ptr<base::DictionaryValue> response_data(new base::DictionaryValue);
  response_data->Set(devtools::Page::queryUsageAndQuota::kResponseQuota,
                     quota.release());
  response_data->Set(devtools::Page::queryUsageAndQuota::kResponseUsage,
                     usage.release());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, base::Passed(&response_data)));
}

void DidGetHostUsage(
    base::ListValue* list,
    const std::string& client_id,
    const base::Closure& barrier,
    int64 value) {
  base::DictionaryValue* usage_item = new base::DictionaryValue;
  usage_item->SetString(devtools::Page::UsageItem::kParamId, client_id);
  usage_item->SetDouble(devtools::Page::UsageItem::kParamValue, value);
  list->Append(usage_item);
  barrier.Run();
}

void DidGetQuotaValue(
    base::DictionaryValue* dictionary,
    const std::string& item_name,
    const base::Closure& barrier,
    quota::QuotaStatusCode status,
    int64 value) {
  if (status == quota::kQuotaStatusOk)
    dictionary->SetDouble(item_name, value);
  barrier.Run();
}

void DidGetUsageAndQuotaForWebApps(
    base::DictionaryValue* quota,
    const std::string& item_name,
    const base::Closure& barrier,
    quota::QuotaStatusCode status,
    int64 used_bytes,
    int64 quota_in_bytes) {
  if (status == quota::kQuotaStatusOk)
    quota->SetDouble(item_name, quota_in_bytes);
  barrier.Run();
}

std::string GetStorageTypeName(quota::StorageType type) {
  switch (type) {
    case quota::kStorageTypeTemporary:
      return devtools::Page::Usage::kParamTemporary;
    case quota::kStorageTypePersistent:
      return devtools::Page::Usage::kParamPersistent;
    case quota::kStorageTypeSyncable:
      return devtools::Page::Usage::kParamSyncable;
    case quota::kStorageTypeQuotaNotManaged:
    case quota::kStorageTypeUnknown:
      NOTREACHED();
  }
  return "";
}

std::string GetQuotaClientName(quota::QuotaClient::ID id) {
  switch (id) {
    case quota::QuotaClient::kFileSystem:
      return devtools::Page::UsageItem::Id::kEnumFilesystem;
    case quota::QuotaClient::kDatabase:
      return devtools::Page::UsageItem::Id::kEnumDatabase;
    case quota::QuotaClient::kAppcache:
      return devtools::Page::UsageItem::Id::kEnumAppcache;
    case quota::QuotaClient::kIndexedDatabase:
      return devtools::Page::UsageItem::Id::kEnumIndexeddatabase;
    default:
      NOTREACHED();
      return "";
  }
}

void QueryUsageAndQuotaOnIOThread(
    scoped_refptr<quota::QuotaManager> quota_manager,
    const GURL& security_origin,
    const ResponseCallback& callback) {
  scoped_ptr<base::DictionaryValue> quota(new base::DictionaryValue);
  scoped_ptr<base::DictionaryValue> usage(new base::DictionaryValue);

  static quota::QuotaClient::ID kQuotaClients[] = {
      quota::QuotaClient::kFileSystem,
      quota::QuotaClient::kDatabase,
      quota::QuotaClient::kAppcache,
      quota::QuotaClient::kIndexedDatabase
  };

  static const size_t kStorageTypeCount = quota::kStorageTypeUnknown;
  std::map<quota::StorageType, base::ListValue*> storage_type_lists;

  for (size_t i = 0; i != kStorageTypeCount; i++) {
    const quota::StorageType type = static_cast<quota::StorageType>(i);
    if (type == quota::kStorageTypeQuotaNotManaged)
      continue;
    storage_type_lists[type] = new base::ListValue;
    usage->Set(GetStorageTypeName(type), storage_type_lists[type]);
  }

  const int kExpectedResults =
      2 + arraysize(kQuotaClients) * storage_type_lists.size();
  base::DictionaryValue* quota_raw_ptr = quota.get();

  // Takes ownership on usage and quota.
  base::Closure barrier = BarrierClosure(
      kExpectedResults,
      base::Bind(&QueryUsageAndQuotaCompletedOnIOThread,
                 base::Passed(&quota),
                 base::Passed(&usage),
                 callback));
  std::string host = net::GetHostOrSpecFromURL(security_origin);

  quota_manager->GetUsageAndQuotaForWebApps(
      security_origin,
      quota::kStorageTypeTemporary,
      base::Bind(&DidGetUsageAndQuotaForWebApps, quota_raw_ptr,
                 std::string(devtools::Page::Quota::kParamTemporary), barrier));

  quota_manager->GetPersistentHostQuota(
      host,
      base::Bind(&DidGetQuotaValue, quota_raw_ptr,
                 std::string(devtools::Page::Quota::kParamPersistent),
                 barrier));

  for (size_t i = 0; i != arraysize(kQuotaClients); i++) {
    std::map<quota::StorageType, base::ListValue*>::const_iterator iter;
    for (iter = storage_type_lists.begin();
         iter != storage_type_lists.end(); ++iter) {
      const quota::StorageType type = (*iter).first;
      if (!quota_manager->IsTrackingHostUsage(type, kQuotaClients[i])) {
        barrier.Run();
        continue;
      }
      quota_manager->GetHostUsage(
          host, type, kQuotaClients[i],
          base::Bind(&DidGetHostUsage, (*iter).second,
                     GetQuotaClientName(kQuotaClients[i]),
                     barrier));
    }
  }
}

} // namespace

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageQueryUsageAndQuota(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  std::string security_origin;
  if (!params || !params->GetString(
      devtools::Page::queryUsageAndQuota::kParamSecurityOrigin,
      &security_origin)) {
    return command->InvalidParamResponse(
        devtools::Page::queryUsageAndQuota::kParamSecurityOrigin);
  }

  ResponseCallback callback = base::Bind(
      &RendererOverridesHandler::PageQueryUsageAndQuotaCompleted,
      weak_factory_.GetWeakPtr(),
      command);

  scoped_refptr<quota::QuotaManager> quota_manager =
      agent_->GetRenderViewHost()->GetProcess()->
          GetStoragePartition()->GetQuotaManager();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &QueryUsageAndQuotaOnIOThread,
          quota_manager,
          GURL(security_origin),
          callback));

  return command->AsyncResponsePromise();
}

void RendererOverridesHandler::PageQueryUsageAndQuotaCompleted(
    scoped_refptr<DevToolsProtocol::Command> command,
    scoped_ptr<base::DictionaryValue> response_data) {
  SendAsyncResponse(command->SuccessResponse(response_data.release()));
}

void RendererOverridesHandler::NotifyScreencastVisibility(bool visible) {
  if (visible)
    capture_retry_count_ = kCaptureRetryLimit;
  base::DictionaryValue* params = new base::DictionaryValue();
  params->SetBoolean(
      devtools::Page::screencastVisibilityChanged::kParamVisible, visible);
  SendNotification(
      devtools::Page::screencastVisibilityChanged::kName, params);
}

// Input agent handlers  ------------------------------------------------------

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::InputDispatchMouseEvent(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  if (!params)
    return NULL;

  bool device_space = false;
  if (!params->GetBoolean(
          devtools::Input::dispatchMouseEvent::kParamDeviceSpace,
          &device_space) ||
      !device_space) {
    return NULL;
  }

  RenderViewHost* host = agent_->GetRenderViewHost();
  blink::WebMouseEvent mouse_event;
  ParseGenericInputParams(params, &mouse_event);

  std::string type;
  if (params->GetString(devtools::Input::dispatchMouseEvent::kParamType,
                        &type)) {
    if (type ==
        devtools::Input::dispatchMouseEvent::Type::kEnumMousePressed)
      mouse_event.type = WebInputEvent::MouseDown;
    else if (type ==
        devtools::Input::dispatchMouseEvent::Type::kEnumMouseReleased)
      mouse_event.type = WebInputEvent::MouseUp;
    else if (type ==
        devtools::Input::dispatchMouseEvent::Type::kEnumMouseMoved)
      mouse_event.type = WebInputEvent::MouseMove;
    else
      return NULL;
  } else {
    return NULL;
  }

  if (!params->GetInteger(devtools::Input::dispatchMouseEvent::kParamX,
                          &mouse_event.x) ||
      !params->GetInteger(devtools::Input::dispatchMouseEvent::kParamY,
                          &mouse_event.y)) {
    return NULL;
  }

  mouse_event.windowX = mouse_event.x;
  mouse_event.windowY = mouse_event.y;
  mouse_event.globalX = mouse_event.x;
  mouse_event.globalY = mouse_event.y;

  params->GetInteger(devtools::Input::dispatchMouseEvent::kParamClickCount,
                     &mouse_event.clickCount);

  std::string button;
  if (!params->GetString(devtools::Input::dispatchMouseEvent::kParamButton,
                         &button)) {
    return NULL;
  }

  if (button == "none") {
    mouse_event.button = WebMouseEvent::ButtonNone;
  } else if (button == "left") {
    mouse_event.button = WebMouseEvent::ButtonLeft;
    mouse_event.modifiers |= WebInputEvent::LeftButtonDown;
  } else if (button == "middle") {
    mouse_event.button = WebMouseEvent::ButtonMiddle;
    mouse_event.modifiers |= WebInputEvent::MiddleButtonDown;
  } else if (button == "right") {
    mouse_event.button = WebMouseEvent::ButtonRight;
    mouse_event.modifiers |= WebInputEvent::RightButtonDown;
  } else {
    return NULL;
  }

  host->ForwardMouseEvent(mouse_event);
  return command->SuccessResponse(NULL);
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::InputDispatchGestureEvent(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  if (!params)
    return NULL;

  RenderViewHostImpl* host = static_cast<RenderViewHostImpl*>(
      agent_->GetRenderViewHost());
  blink::WebGestureEvent event;
  ParseGenericInputParams(params, &event);
  event.sourceDevice = blink::WebGestureDeviceTouchscreen;

  std::string type;
  if (params->GetString(devtools::Input::dispatchGestureEvent::kParamType,
                        &type)) {
    if (type ==
        devtools::Input::dispatchGestureEvent::Type::kEnumScrollBegin)
      event.type = WebInputEvent::GestureScrollBegin;
    else if (type ==
        devtools::Input::dispatchGestureEvent::Type::kEnumScrollUpdate)
      event.type = WebInputEvent::GestureScrollUpdate;
    else if (type ==
        devtools::Input::dispatchGestureEvent::Type::kEnumScrollEnd)
      event.type = WebInputEvent::GestureScrollEnd;
    else if (type ==
        devtools::Input::dispatchGestureEvent::Type::kEnumTapDown)
      event.type = WebInputEvent::GestureTapDown;
    else if (type ==
        devtools::Input::dispatchGestureEvent::Type::kEnumTap)
      event.type = WebInputEvent::GestureTap;
    else if (type ==
        devtools::Input::dispatchGestureEvent::Type::kEnumPinchBegin)
      event.type = WebInputEvent::GesturePinchBegin;
    else if (type ==
        devtools::Input::dispatchGestureEvent::Type::kEnumPinchUpdate)
      event.type = WebInputEvent::GesturePinchUpdate;
    else if (type ==
        devtools::Input::dispatchGestureEvent::Type::kEnumPinchEnd)
      event.type = WebInputEvent::GesturePinchEnd;
    else
      return NULL;
  } else {
    return NULL;
  }

  if (!params->GetInteger(devtools::Input::dispatchGestureEvent::kParamX,
                          &event.x) ||
      !params->GetInteger(devtools::Input::dispatchGestureEvent::kParamY,
                          &event.y)) {
    return NULL;
  }
  event.globalX = event.x;
  event.globalY = event.y;

  if (type == "scrollUpdate") {
    int dx;
    int dy;
    if (!params->GetInteger(
            devtools::Input::dispatchGestureEvent::kParamDeltaX, &dx) ||
        !params->GetInteger(
            devtools::Input::dispatchGestureEvent::kParamDeltaY, &dy)) {
      return NULL;
    }
    event.data.scrollUpdate.deltaX = dx;
    event.data.scrollUpdate.deltaY = dy;
  }

  if (type == "pinchUpdate") {
    double scale;
    if (!params->GetDouble(
        devtools::Input::dispatchGestureEvent::kParamPinchScale,
        &scale)) {
      return NULL;
    }
    event.data.pinchUpdate.scale = static_cast<float>(scale);
  }

  host->ForwardGestureEvent(event);
  return command->SuccessResponse(NULL);
}

}  // namespace content
