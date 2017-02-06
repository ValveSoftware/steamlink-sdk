// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_EMULATION_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_EMULATION_HANDLER_H_

#include "base/macros.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
#include "third_party/WebKit/public/web/WebDeviceEmulationParams.h"

namespace content {

class RenderFrameHostImpl;
class WebContentsImpl;

namespace devtools {

namespace page { class PageHandler; }

namespace emulation {

class EmulationHandler {
 public:
  using Response = DevToolsProtocolClient::Response;

  EmulationHandler();
  ~EmulationHandler();

  void SetRenderFrameHost(RenderFrameHostImpl* host);
  void Detached();

  Response SetGeolocationOverride(double* latitude,
                                  double* longitude,
                                  double* accuracy);
  Response ClearGeolocationOverride();

  Response SetTouchEmulationEnabled(bool enabled,
                                    const std::string* configuration);

  Response CanEmulate(bool* result);
  Response SetDeviceMetricsOverride(
      int width,
      int height,
      double device_scale_factor,
      bool mobile,
      bool fit_window,
      const double* optional_scale,
      const double* optional_offset_x,
      const double* optional_offset_y,
      const int* screen_widget,
      const int* screen_height,
      const int* position_x,
      const int* position_y,
      const std::unique_ptr<base::DictionaryValue>& screen_orientation);
  Response ClearDeviceMetricsOverride();

 private:
  WebContentsImpl* GetWebContents();
  void UpdateTouchEventEmulationState();
  void UpdateDeviceEmulationState();

  bool touch_emulation_enabled_;
  std::string touch_emulation_configuration_;

  bool device_emulation_enabled_;
  blink::WebDeviceEmulationParams device_emulation_params_;

  RenderFrameHostImpl* host_;

  DISALLOW_COPY_AND_ASSIGN(EmulationHandler);
};

}  // namespace emulation
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_EMULATION_HANDLER_H_
