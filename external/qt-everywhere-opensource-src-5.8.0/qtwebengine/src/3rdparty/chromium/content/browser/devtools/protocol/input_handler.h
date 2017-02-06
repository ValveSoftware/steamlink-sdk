// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "ui/gfx/geometry/size_f.h"

namespace cc {
class CompositorFrameMetadata;
}

namespace gfx {
class Point;
}

namespace content {

class RenderWidgetHostImpl;

namespace devtools {
namespace input {

class InputHandler {
 public:
  typedef DevToolsProtocolClient::Response Response;

  InputHandler();
  virtual ~InputHandler();

  void SetRenderWidgetHost(RenderWidgetHostImpl* host);
  void SetClient(std::unique_ptr<Client> client);
  void OnSwapCompositorFrame(const cc::CompositorFrameMetadata& frame_metadata);

  Response DispatchKeyEvent(const std::string& type,
                            const int* modifiers,
                            const double* timestamp,
                            const std::string* text,
                            const std::string* unmodified_text,
                            const std::string* key_identifier,
                            const std::string* code,
                            const std::string* key,
                            const int* windows_virtual_key_code,
                            const int* native_virtual_key_code,
                            const bool* auto_repeat,
                            const bool* is_keypad,
                            const bool* is_system_key);

  Response DispatchMouseEvent(const std::string& type,
                              int x,
                              int y,
                              const int* modifiers,
                              const double* timestamp,
                              const std::string* button,
                              const int* click_count);

  Response EmulateTouchFromMouseEvent(const std::string& type,
                                      int x,
                                      int y,
                                      double timestamp,
                                      const std::string& button,
                                      double* delta_x,
                                      double* delta_y,
                                      int* modifiers,
                                      int* click_count);

  Response SynthesizePinchGesture(DevToolsCommandId command_id,
                                  int x,
                                  int y,
                                  double scale_factor,
                                  const int* relative_speed,
                                  const std::string* gesture_source_type);

  Response SynthesizeScrollGesture(DevToolsCommandId command_id,
                                   int x,
                                   int y,
                                   const int* x_distance,
                                   const int* y_distance,
                                   const int* x_overscroll,
                                   const int* y_overscroll,
                                   const bool* prevent_fling,
                                   const int* speed,
                                   const std::string* gesture_source_type,
                                   const int* repeat_count,
                                   const int* repeat_delay_ms,
                                   const std::string* interaction_marker_name);

  Response SynthesizeTapGesture(DevToolsCommandId command_id,
                                int x,
                                int y,
                                const int* duration,
                                const int* tap_count,
                                const std::string* gesture_source_type);

 private:
  void SendSynthesizePinchGestureResponse(DevToolsCommandId command_id,
                                          SyntheticGesture::Result result);

  void SendSynthesizeScrollGestureResponse(DevToolsCommandId command_id,
                                           SyntheticGesture::Result result);

  void SendSynthesizeTapGestureResponse(DevToolsCommandId command_id,
                                        bool send_success,
                                        SyntheticGesture::Result result);

  void SynthesizeRepeatingScroll(
      SyntheticSmoothScrollGestureParams gesture_params,
      int repeat_count,
      base::TimeDelta repeat_delay,
      std::string interaction_marker_name,
      DevToolsCommandId command_id);

  void OnScrollFinished(SyntheticSmoothScrollGestureParams gesture_params,
                        int repeat_count,
                        base::TimeDelta repeat_delay,
                        std::string interaction_marker_name,
                        DevToolsCommandId command_id,
                        SyntheticGesture::Result result);

  RenderWidgetHostImpl* host_;
  std::unique_ptr<Client> client_;
  float page_scale_factor_;
  gfx::SizeF scrollable_viewport_size_;
  base::WeakPtrFactory<InputHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InputHandler);
};

}  // namespace input
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INPUT_HANDLER_H_
