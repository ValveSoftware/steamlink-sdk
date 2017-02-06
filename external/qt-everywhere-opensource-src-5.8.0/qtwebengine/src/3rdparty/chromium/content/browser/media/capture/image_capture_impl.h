// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_IMAGE_CAPTURE_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_IMAGE_CAPTURE_IMPL_H_

#include "media/mojo/interfaces/image_capture.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

class ImageCaptureImpl : public media::mojom::ImageCapture {
 public:
  static void Create(
      mojo::InterfaceRequest<media::mojom::ImageCapture> request);
  ~ImageCaptureImpl() override;

  void GetCapabilities(const mojo::String& source_id,
                       const GetCapabilitiesCallback& callback) override;

  void SetOptions(const mojo::String& source_id,
                  media::mojom::PhotoSettingsPtr settings,
                  const SetOptionsCallback& callback) override;

  void TakePhoto(const mojo::String& source_id,
                 const TakePhotoCallback& callback) override;

 private:
  explicit ImageCaptureImpl(
      mojo::InterfaceRequest<media::mojom::ImageCapture> request);

  mojo::StrongBinding<media::mojom::ImageCapture> binding_;

  DISALLOW_COPY_AND_ASSIGN(ImageCaptureImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_IMAGE_CAPTURE_IMPL_H_
