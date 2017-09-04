/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebCompositorSupport_h
#define WebCompositorSupport_h

#include "WebCommon.h"
#include "WebFloatPoint.h"
#include "WebLayerTreeView.h"
#include "WebScrollbar.h"
#include "WebScrollbarThemePainter.h"

#include <memory>

namespace cc {
class Layer;
class TextureLayerClient;
}

namespace blink {

class WebContentLayer;
class WebContentLayerClient;
class WebExternalTextureLayer;
class WebImageLayer;
class WebLayer;
class WebScrollbarLayer;
class WebScrollbarThemeGeometry;

class WebCompositorSupport {
 public:
  // Layers -------------------------------------------------------

  virtual std::unique_ptr<WebLayer> createLayer() = 0;

  virtual std::unique_ptr<WebLayer> createLayerFromCCLayer(cc::Layer*) = 0;

  virtual std::unique_ptr<WebContentLayer> createContentLayer(
      WebContentLayerClient*) = 0;

  virtual std::unique_ptr<WebExternalTextureLayer> createExternalTextureLayer(
      cc::TextureLayerClient*) = 0;

  virtual std::unique_ptr<WebImageLayer> createImageLayer() = 0;

  virtual std::unique_ptr<WebScrollbarLayer> createScrollbarLayer(
      std::unique_ptr<WebScrollbar>,
      WebScrollbarThemePainter,
      std::unique_ptr<WebScrollbarThemeGeometry>) = 0;

  virtual std::unique_ptr<WebScrollbarLayer> createSolidColorScrollbarLayer(
      WebScrollbar::Orientation,
      int thumbThickness,
      int trackStart,
      bool isLeftSideVerticalScrollbar) = 0;

 protected:
  virtual ~WebCompositorSupport() {}
};
}

#endif  // WebCompositorSupport_h
