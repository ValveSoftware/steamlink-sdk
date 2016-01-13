// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/strings/string_tokenizer.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace mojo {
namespace examples {

class ImageViewer;

class NavigatorImpl : public InterfaceImpl<navigation::Navigator> {
 public:
  explicit NavigatorImpl(ImageViewer* viewer) : viewer_(viewer) {}
  virtual ~NavigatorImpl() {}

 private:
  // Overridden from navigation::Navigate:
  virtual void Navigate(
      uint32_t node_id,
      navigation::NavigationDetailsPtr navigation_details,
      navigation::ResponseDetailsPtr response_details) OVERRIDE {
    int content_length = GetContentLength(response_details->response->headers);
    unsigned char* data = new unsigned char[content_length];
    unsigned char* buf = data;
    uint32_t bytes_remaining = content_length;
    uint32_t num_bytes = bytes_remaining;
    while (bytes_remaining > 0) {
      MojoResult result = ReadDataRaw(
          response_details->response_body_stream.get(),
          buf,
          &num_bytes,
          MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        Wait(response_details->response_body_stream.get(),
             MOJO_HANDLE_SIGNAL_READABLE,
             MOJO_DEADLINE_INDEFINITE);
      } else if (result == MOJO_RESULT_OK) {
        buf += num_bytes;
        num_bytes = bytes_remaining -= num_bytes;
      } else {
        break;
      }
    }

    SkBitmap bitmap;
    gfx::PNGCodec::Decode(static_cast<const unsigned char*>(data),
                          content_length, &bitmap);
    UpdateView(node_id, bitmap);

    delete[] data;
  }

  void UpdateView(view_manager::Id node_id, const SkBitmap& bitmap);

  int GetContentLength(const Array<String>& headers) {
    for (size_t i = 0; i < headers.size(); ++i) {
      base::StringTokenizer t(headers[i], ": ;=");
      while (t.GetNext()) {
        if (!t.token_is_delim() && t.token() == "Content-Length") {
          while (t.GetNext()) {
            if (!t.token_is_delim())
              return atoi(t.token().c_str());
          }
        }
      }
    }
    return 0;
  }

  ImageViewer* viewer_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorImpl);
};

class ImageViewer : public Application,
                    public view_manager::ViewManagerDelegate {
 public:
  ImageViewer() : content_view_(NULL) {}
  virtual ~ImageViewer() {}

  void UpdateView(view_manager::Id node_id, const SkBitmap& bitmap) {
    bitmap_ = bitmap;
    DrawBitmap();
  }

 private:
  // Overridden from Application:
  virtual void Initialize() OVERRIDE {
    AddService<NavigatorImpl>(this);
    view_manager::ViewManager::Create(this, this);
  }

  // Overridden from view_manager::ViewManagerDelegate:
  virtual void OnRootAdded(view_manager::ViewManager* view_manager,
                           view_manager::Node* root) OVERRIDE {
    content_view_ = view_manager::View::Create(view_manager);
    root->SetActiveView(content_view_);
    content_view_->SetColor(SK_ColorRED);
    if (!bitmap_.isNull())
      DrawBitmap();
  }

  void DrawBitmap() {
    if (content_view_)
      content_view_->SetContents(bitmap_);
  }

  view_manager::View* content_view_;
  SkBitmap bitmap_;

  DISALLOW_COPY_AND_ASSIGN(ImageViewer);
};

void NavigatorImpl::UpdateView(view_manager::Id node_id,
                                     const SkBitmap& bitmap) {
  viewer_->UpdateView(node_id, bitmap);
}

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::ImageViewer;
}

}  // namespace mojo
