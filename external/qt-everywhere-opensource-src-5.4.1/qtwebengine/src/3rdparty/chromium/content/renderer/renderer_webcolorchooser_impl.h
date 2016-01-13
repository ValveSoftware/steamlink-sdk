// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_WEBCOLORCHOOSER_IMPL_H_
#define CONTENT_RENDERER_RENDERER_WEBCOLORCHOOSER_IMPL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "content/public/common/color_suggestion.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/web/WebColorChooser.h"
#include "third_party/WebKit/public/web/WebColorChooserClient.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {
class WebFrame;
}

namespace content {

class RendererWebColorChooserImpl : public blink::WebColorChooser,
                                    public RenderFrameObserver {
 public:
  explicit RendererWebColorChooserImpl(RenderFrame* render_frame,
                                       blink::WebColorChooserClient*);
  virtual ~RendererWebColorChooserImpl();

  // blink::WebColorChooser implementation:
  virtual void setSelectedColor(const blink::WebColor);
  virtual void endChooser();

  void Open(SkColor initial_color,
            const std::vector<content::ColorSuggestion>& suggestions);

  blink::WebColorChooserClient* client() { return client_; }

 private:
  // RenderFrameObserver implementation:
  // Don't destroy the RendererWebColorChooserImpl when the RenderFrame goes
  // away. RendererWebColorChooserImpl is owned by
  // blink::ColorChooserUIController.
  virtual void OnDestruct() OVERRIDE {}
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnDidChooseColorResponse(int color_chooser_id, SkColor color);
  void OnDidEndColorChooser(int color_chooser_id);

  int identifier_;
  blink::WebColorChooserClient* client_;

  DISALLOW_COPY_AND_ASSIGN(RendererWebColorChooserImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_WEBCOLORCHOOSER_IMPL_H_
