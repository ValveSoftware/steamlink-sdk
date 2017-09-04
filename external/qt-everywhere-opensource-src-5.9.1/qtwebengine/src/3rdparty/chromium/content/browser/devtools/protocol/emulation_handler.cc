// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/emulation_handler.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/common/url_constants.h"
#include "device/geolocation/geolocation_service_context.h"
#include "device/geolocation/geoposition.h"

namespace content {
namespace devtools {
namespace emulation {

using Response = DevToolsProtocolClient::Response;
using GeolocationServiceContext = device::GeolocationServiceContext;
using Geoposition = device::Geoposition;

namespace {

blink::WebScreenOrientationType WebScreenOrientationTypeFromString(
    const std::string& type) {
  if (type == screen_orientation::kTypePortraitPrimary)
    return blink::WebScreenOrientationPortraitPrimary;
  if (type == screen_orientation::kTypePortraitSecondary)
    return blink::WebScreenOrientationPortraitSecondary;
  if (type == screen_orientation::kTypeLandscapePrimary)
    return blink::WebScreenOrientationLandscapePrimary;
  if (type == screen_orientation::kTypeLandscapeSecondary)
    return blink::WebScreenOrientationLandscapeSecondary;
  return blink::WebScreenOrientationUndefined;
}

ui::GestureProviderConfigType TouchEmulationConfigurationToType(
    const std::string& protocol_value) {
  ui::GestureProviderConfigType result =
      ui::GestureProviderConfigType::CURRENT_PLATFORM;
  if (protocol_value ==
      set_touch_emulation_enabled::kConfigurationMobile) {
    result = ui::GestureProviderConfigType::GENERIC_MOBILE;
  }
  if (protocol_value ==
      set_touch_emulation_enabled::kConfigurationDesktop) {
    result = ui::GestureProviderConfigType::GENERIC_DESKTOP;
  }
  return result;
}

}  // namespace

EmulationHandler::EmulationHandler()
    : touch_emulation_enabled_(false),
      device_emulation_enabled_(false),
      host_(nullptr) {
}

EmulationHandler::~EmulationHandler() {
}

void EmulationHandler::SetRenderFrameHost(RenderFrameHostImpl* host) {
  if (host_ == host)
    return;

  host_ = host;
  UpdateTouchEventEmulationState();
  UpdateDeviceEmulationState();
}

void EmulationHandler::Detached() {
  touch_emulation_enabled_ = false;
  device_emulation_enabled_ = false;
  UpdateTouchEventEmulationState();
  UpdateDeviceEmulationState();
}

Response EmulationHandler::SetGeolocationOverride(
    double* latitude, double* longitude, double* accuracy) {
  if (!GetWebContents())
    return Response::InternalError("Could not connect to view");

  GeolocationServiceContext* geolocation_context =
      GetWebContents()->GetGeolocationServiceContext();
  std::unique_ptr<Geoposition> geoposition(new Geoposition());
  if (latitude && longitude && accuracy) {
    geoposition->latitude = *latitude;
    geoposition->longitude = *longitude;
    geoposition->accuracy = *accuracy;
    geoposition->timestamp = base::Time::Now();
    if (!geoposition->Validate()) {
      return Response::InternalError("Invalid geolocation");
    }
  } else {
    geoposition->error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
  }
  geolocation_context->SetOverride(std::move(geoposition));
  return Response::OK();
}

Response EmulationHandler::ClearGeolocationOverride() {
  if (!GetWebContents())
    return Response::InternalError("Could not connect to view");

  GeolocationServiceContext* geolocation_context =
      GetWebContents()->GetGeolocationServiceContext();
  geolocation_context->ClearOverride();
  return Response::OK();
}

Response EmulationHandler::SetTouchEmulationEnabled(
    bool enabled, const std::string* configuration) {
  touch_emulation_enabled_ = enabled;
  touch_emulation_configuration_ =
      configuration ? *configuration : std::string();
  UpdateTouchEventEmulationState();
  return Response::FallThrough();
}

Response EmulationHandler::CanEmulate(bool* result) {
#if defined(OS_ANDROID)
  *result = false;
#else
  *result = true;
  if (WebContentsImpl* web_contents = GetWebContents())
    *result &= !web_contents->GetVisibleURL().SchemeIs(kChromeDevToolsScheme);
  if (host_ && host_->GetRenderWidgetHost())
    *result &= !host_->GetRenderWidgetHost()->auto_resize_enabled();
#endif  // defined(OS_ANDROID)
  return Response::OK();
}

Response EmulationHandler::SetDeviceMetricsOverride(
    int width,
    int height,
    double device_scale_factor,
    bool mobile,
    bool fit_window,
    const double* optional_scale,
    const double* optional_offset_x,
    const double* optional_offset_y,
    const int* screen_width,
    const int* screen_height,
    const int* position_x,
    const int* position_y,
    const std::unique_ptr<base::DictionaryValue>& screen_orientation) {
  const static int max_size = 10000000;
  const static double max_scale = 10;
  const static int max_orientation_angle = 360;

  if (!host_)
    return Response::InternalError("Could not connect to view");

  if (screen_width && screen_height &&
      (*screen_width < 0 || *screen_height < 0 ||
           *screen_width > max_size || *screen_height > max_size)) {
    return Response::InvalidParams(
        "Screen width and height values must be positive, not greater than " +
        base::IntToString(max_size));
  }

  if (screen_width && screen_height && position_x && position_y &&
      (*position_x < 0 || *position_y < 0 ||
           *position_x > *screen_width || *position_y > *screen_height)) {
    return Response::InvalidParams("View position should be on the screen");
  }

  if (width < 0 || height < 0 || width > max_size || height > max_size) {
    return Response::InvalidParams(
        "Width and height values must be positive, not greater than " +
        base::IntToString(max_size));
  }

  if (device_scale_factor < 0)
    return Response::InvalidParams("deviceScaleFactor must be non-negative");

  if (optional_scale && (*optional_scale <= 0 || *optional_scale > max_scale)) {
    return Response::InvalidParams(
        "scale must be positive, not greater than " +
        base::DoubleToString(max_scale));
  }

  blink::WebScreenOrientationType orientationType =
      blink::WebScreenOrientationUndefined;
  int orientationAngle = 0;
  if (screen_orientation) {
    std::string orientationTypeString;
    if (!screen_orientation->GetString("type", &orientationTypeString)) {
      return Response::InvalidParams(
          "Screen orientation type must be a string");
    }
    orientationType = WebScreenOrientationTypeFromString(orientationTypeString);
    if (orientationType == blink::WebScreenOrientationUndefined)
      return Response::InvalidParams("Invalid screen orientation type value");

    if (!screen_orientation->GetInteger("angle", &orientationAngle)) {
      return Response::InvalidParams(
          "Screen orientation angle must be a number");
    }
    if (orientationAngle < 0 || orientationAngle >= max_orientation_angle) {
      return Response::InvalidParams(
          "Screen orientation angle must be non-negative, less than " +
          base::IntToString(max_orientation_angle));
    }
  }

  blink::WebDeviceEmulationParams params;
  params.screenPosition = mobile ? blink::WebDeviceEmulationParams::Mobile :
      blink::WebDeviceEmulationParams::Desktop;
  if (screen_width && screen_height)
    params.screenSize = blink::WebSize(*screen_width, *screen_height);
  if (position_x && position_y)
    params.viewPosition = blink::WebPoint(*position_x, *position_y);
  params.deviceScaleFactor = device_scale_factor;
  params.viewSize = blink::WebSize(width, height);
  params.fitToView = fit_window;
  params.scale = optional_scale ? *optional_scale : 1;
  params.screenOrientationType = orientationType;
  params.screenOrientationAngle = orientationAngle;

  if (device_emulation_enabled_ && params == device_emulation_params_)
    return Response::OK();

  device_emulation_enabled_ = true;
  device_emulation_params_ = params;
  UpdateDeviceEmulationState();
  return Response::OK();
}

Response EmulationHandler::ClearDeviceMetricsOverride() {
  if (!device_emulation_enabled_)
    return Response::OK();

  device_emulation_enabled_ = false;
  UpdateDeviceEmulationState();
  return Response::OK();
}

Response EmulationHandler::SetVisibleSize(int width, int height) {
  if (width < 0 || height < 0)
    return Response::InvalidParams("Width and height must be non-negative");

  // Set size of frame by resizing RWHV if available.
  RenderWidgetHostImpl* widget_host =
      host_ ? host_->GetRenderWidgetHost() : nullptr;
  if (!widget_host)
    return Response::ServerError("Target does not support setVisibleSize");

  widget_host->GetView()->SetSize(gfx::Size(width, height));
  return Response::OK();
}

Response EmulationHandler::ForceViewport(double x, double y, double scale) {
  return Response::FallThrough();
}

Response EmulationHandler::ResetViewport() {
  return Response::FallThrough();
}

Response EmulationHandler::ResetPageScaleFactor() {
  return Response::FallThrough();
}

Response EmulationHandler::SetPageScaleFactor(double page_scale_factor) {
  return Response::FallThrough();
}

Response EmulationHandler::SetScriptExecutionDisabled(bool disabled) {
  return Response::FallThrough();
}

Response EmulationHandler::SetEmulatedMedia(const std::string& media) {
  return Response::FallThrough();
}

Response EmulationHandler::SetCPUThrottlingRate(double rate) {
  return Response::FallThrough();
}

Response EmulationHandler::SetVirtualTimePolicy(
    const std::string& policy,
    const int* budget) {
  return Response::FallThrough();
}

WebContentsImpl* EmulationHandler::GetWebContents() {
  return host_ ?
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(host_)) :
      nullptr;
}

void EmulationHandler::UpdateTouchEventEmulationState() {
  RenderWidgetHostImpl* widget_host =
      host_ ? host_->GetRenderWidgetHost() : nullptr;
  if (!widget_host)
    return;
  bool enabled = touch_emulation_enabled_;
  ui::GestureProviderConfigType config_type =
      TouchEmulationConfigurationToType(touch_emulation_configuration_);
  widget_host->SetTouchEventEmulationEnabled(enabled, config_type);
  if (GetWebContents())
    GetWebContents()->SetForceDisableOverscrollContent(enabled);
}

void EmulationHandler::UpdateDeviceEmulationState() {
  RenderWidgetHostImpl* widget_host =
      host_ ? host_->GetRenderWidgetHost() : nullptr;
  if (!widget_host)
    return;
  if (device_emulation_enabled_) {
    widget_host->Send(new ViewMsg_EnableDeviceEmulation(
        widget_host->GetRoutingID(), device_emulation_params_));
  } else {
    widget_host->Send(new ViewMsg_DisableDeviceEmulation(
        widget_host->GetRoutingID()));
  }
}

}  // namespace emulation
}  // namespace devtools
}  // namespace content
