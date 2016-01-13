// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "mojo/examples/pepper_container_app/mojo_ppapi_globals.h"
#include "mojo/examples/pepper_container_app/plugin_instance.h"
#include "mojo/examples/pepper_container_app/plugin_module.h"
#include "mojo/examples/pepper_container_app/type_converters.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/gles2/gles2.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/public/interfaces/native_viewport/native_viewport.mojom.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace mojo {
namespace examples {

class PepperContainerApp: public Application,
                          public NativeViewportClient,
                          public MojoPpapiGlobals::Delegate {
 public:
  explicit PepperContainerApp()
      : Application(),
        ppapi_globals_(this),
        plugin_module_(new PluginModule) {}

  virtual ~PepperContainerApp() {}

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
  }

  // NativeViewportClient implementation.
  virtual void OnCreated() OVERRIDE {
    ppapi::ProxyAutoLock lock;

    plugin_instance_ = plugin_module_->CreateInstance().Pass();
    if (!plugin_instance_->DidCreate())
      plugin_instance_.reset();
  }

  virtual void OnDestroyed() OVERRIDE {
    ppapi::ProxyAutoLock lock;

    if (plugin_instance_) {
      plugin_instance_->DidDestroy();
      plugin_instance_.reset();
    }

    base::MessageLoop::current()->Quit();
  }

  virtual void OnBoundsChanged(RectPtr bounds) OVERRIDE {
    ppapi::ProxyAutoLock lock;

    if (plugin_instance_)
      plugin_instance_->DidChangeView(bounds.To<PP_Rect>());
  }

  virtual void OnEvent(EventPtr event,
                       const mojo::Callback<void()>& callback) OVERRIDE {
    if (!event->location.is_null()) {
      ppapi::ProxyAutoLock lock;

      // TODO(yzshen): Handle events.
    }
    callback.Run();
  }

  // MojoPpapiGlobals::Delegate implementation.
  virtual ScopedMessagePipeHandle CreateGLES2Context() OVERRIDE {
    CommandBufferPtr command_buffer;
    viewport_->CreateGLES2Context(Get(&command_buffer));
    return command_buffer.PassMessagePipe();
  }

 private:
  MojoPpapiGlobals ppapi_globals_;

  NativeViewportPtr viewport_;
  scoped_refptr<PluginModule> plugin_module_;
  scoped_ptr<PluginInstance> plugin_instance_;

  DISALLOW_COPY_AND_ASSIGN(PepperContainerApp);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::PepperContainerApp();
}

}  // namespace mojo
