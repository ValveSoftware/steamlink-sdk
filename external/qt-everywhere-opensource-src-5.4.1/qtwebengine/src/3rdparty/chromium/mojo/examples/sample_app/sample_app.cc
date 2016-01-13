// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "mojo/examples/sample_app/gles2_client_impl.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/gles2/gles2.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/public/interfaces/native_viewport/native_viewport.mojom.h"

namespace mojo {
namespace examples {

class SampleApp : public Application, public NativeViewportClient {
 public:
  SampleApp() {}

  virtual ~SampleApp() {
    // TODO(darin): Fix shutdown so we don't need to leak this.
    MOJO_ALLOW_UNUSED GLES2ClientImpl* leaked = gles2_client_.release();
  }

  virtual void Initialize() MOJO_OVERRIDE {
    ConnectTo("mojo:mojo_native_viewport_service", &viewport_);
    viewport_.set_client(this);

    RectPtr rect(Rect::New());
    rect->x = 10;
    rect->y = 10;
    rect->width = 800;
    rect->height = 600;
    viewport_->Create(rect.Pass());
    viewport_->Show();

    CommandBufferPtr command_buffer;
    viewport_->CreateGLES2Context(Get(&command_buffer));
    gles2_client_.reset(new GLES2ClientImpl(command_buffer.Pass()));
  }

  virtual void OnCreated() MOJO_OVERRIDE {
  }

  virtual void OnDestroyed() MOJO_OVERRIDE {
    RunLoop::current()->Quit();
  }

  virtual void OnBoundsChanged(RectPtr bounds) MOJO_OVERRIDE {
    assert(bounds);
    SizePtr size(Size::New());
    size->width = bounds->width;
    size->height = bounds->height;
    gles2_client_->SetSize(*size);
  }

  virtual void OnEvent(EventPtr event,
                       const Callback<void()>& callback) MOJO_OVERRIDE {
    assert(event);
    if (event->location)
      gles2_client_->HandleInputEvent(*event);
    callback.Run();
  }

 private:
  mojo::GLES2Initializer gles2;
  scoped_ptr<GLES2ClientImpl> gles2_client_;
  NativeViewportPtr viewport_;

  DISALLOW_COPY_AND_ASSIGN(SampleApp);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::SampleApp();
}

}  // namespace mojo
