// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_RENDERER_BLIMP_CONTENT_RENDERER_CLIENT_H_
#define BLIMP_ENGINE_RENDERER_BLIMP_CONTENT_RENDERER_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "content/public/renderer/content_renderer_client.h"

namespace web_cache {
class WebCacheImpl;
}

namespace blimp {
namespace engine {

class BlimpContentRendererClient : public content::ContentRendererClient {
 public:
  BlimpContentRendererClient();
  ~BlimpContentRendererClient() override;

  // content::ContentRendererClient implementation.
  void RenderThreadStarted() override;
  cc::ImageSerializationProcessor* GetImageSerializationProcessor() override;

 private:
  // Manages the process-global web cache.
  std::unique_ptr<web_cache::WebCacheImpl> web_cache_impl_;

  // Provides the functionality to serialize images in SkPicture.
  std::unique_ptr<cc::ImageSerializationProcessor>
      image_serialization_processor_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentRendererClient);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_RENDERER_BLIMP_CONTENT_RENDERER_CLIENT_H_
