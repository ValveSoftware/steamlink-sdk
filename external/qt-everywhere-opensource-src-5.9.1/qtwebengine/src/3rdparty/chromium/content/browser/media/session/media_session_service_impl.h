// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_SERVICE_IMPL_H_

#include "base/optional.h"
#include "content/public/common/media_metadata.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/modules/mediasession/media_session.mojom.h"

namespace content {

class MediaSessionImpl;
class RenderFrameHost;

// There is one MediaSessionService per frame.
class MediaSessionServiceImpl : public blink::mojom::MediaSessionService {
 public:
  ~MediaSessionServiceImpl() override;

  static void Create(RenderFrameHost* render_frame_host,
                     blink::mojom::MediaSessionServiceRequest request);
  const blink::mojom::MediaSessionClientPtr& GetClient() { return client_; }

 private:
  explicit MediaSessionServiceImpl(RenderFrameHost* render_frame_host);

  // blink::mojom::MediaSessionService implementation.
  void SetClient(blink::mojom::MediaSessionClientPtr client) override;

  void SetMetadata(
      const base::Optional<content::MediaMetadata>& metadata) override;

  void EnableAction(blink::mojom::MediaSessionAction action) override;
  void DisableAction(blink::mojom::MediaSessionAction action) override;

  // Returns the content::MediaSession this service is associated with. Only
  // returns non-null when this service is in the top-level frame.
  MediaSessionImpl* GetMediaSession();

  void Bind(blink::mojom::MediaSessionServiceRequest request);

  RenderFrameHost* render_frame_host_;

  // RAII binding of |this| to an MediaSessionService interface request.
  // The binding is removed when binding_ is cleared or goes out of scope.
  std::unique_ptr<mojo::Binding<blink::mojom::MediaSessionService>> binding_;
  blink::mojom::MediaSessionClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_SERVICE_IMPL_H_
