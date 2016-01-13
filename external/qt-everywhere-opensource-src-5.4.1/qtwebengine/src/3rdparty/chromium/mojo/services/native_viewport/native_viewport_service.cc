// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport_service.h"

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/gles2/command_buffer_impl.h"
#include "mojo/services/native_viewport/native_viewport.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/input_events/input_events_type_converters.h"
#include "mojo/services/public/interfaces/native_viewport/native_viewport.mojom.h"
#include "ui/events/event.h"

namespace mojo {
namespace services {
namespace {

bool IsRateLimitedEventType(ui::Event* event) {
  return event->type() == ui::ET_MOUSE_MOVED ||
         event->type() == ui::ET_MOUSE_DRAGGED ||
         event->type() == ui::ET_TOUCH_MOVED;
}

}

class NativeViewportImpl
    : public InterfaceImpl<mojo::NativeViewport>,
      public NativeViewportDelegate {
 public:
  NativeViewportImpl(shell::Context* context)
      : context_(context),
        widget_(gfx::kNullAcceleratedWidget),
        waiting_for_event_ack_(false),
        weak_factory_(this) {}
  virtual ~NativeViewportImpl() {
    // Destroy the NativeViewport early on as it may call us back during
    // destruction and we want to be in a known state.
    native_viewport_.reset();
  }

  virtual void Create(RectPtr bounds) OVERRIDE {
    native_viewport_ =
        services::NativeViewport::Create(context_, this);
    native_viewport_->Init(bounds.To<gfx::Rect>());
    client()->OnCreated();
    OnBoundsChanged(bounds.To<gfx::Rect>());
  }

  virtual void Show() OVERRIDE {
    native_viewport_->Show();
  }

  virtual void Hide() OVERRIDE {
    native_viewport_->Hide();
  }

  virtual void Close() OVERRIDE {
    command_buffer_.reset();
    DCHECK(native_viewport_);
    native_viewport_->Close();
  }

  virtual void SetBounds(RectPtr bounds) OVERRIDE {
    native_viewport_->SetBounds(bounds.To<gfx::Rect>());
  }

  virtual void CreateGLES2Context(
      InterfaceRequest<CommandBuffer> command_buffer_request) OVERRIDE {
    if (command_buffer_.get() || command_buffer_request_.is_pending()) {
      LOG(ERROR) << "Can't create multiple contexts on a NativeViewport";
      return;
    }
    command_buffer_request_ = command_buffer_request.Pass();
    CreateCommandBufferIfNeeded();
  }

  void AckEvent() {
    waiting_for_event_ack_ = false;
  }

  void CreateCommandBufferIfNeeded() {
    if (!command_buffer_request_.is_pending())
      return;
    DCHECK(!command_buffer_.get());
    if (widget_ == gfx::kNullAcceleratedWidget)
      return;
    gfx::Size size = native_viewport_->GetSize();
    if (size.IsEmpty())
      return;
    command_buffer_.reset(
        new CommandBufferImpl(widget_, native_viewport_->GetSize()));
    BindToRequest(command_buffer_.get(), &command_buffer_request_);
  }

  virtual bool OnEvent(ui::Event* ui_event) OVERRIDE {
    // Must not return early before updating capture.
    switch (ui_event->type()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_TOUCH_PRESSED:
      native_viewport_->SetCapture();
      break;
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_TOUCH_RELEASED:
      native_viewport_->ReleaseCapture();
      break;
    default:
      break;
    }

    if (waiting_for_event_ack_ && IsRateLimitedEventType(ui_event))
      return false;

    client()->OnEvent(
        TypeConverter<EventPtr, ui::Event>::ConvertFrom(*ui_event),
        base::Bind(&NativeViewportImpl::AckEvent,
                   weak_factory_.GetWeakPtr()));
    waiting_for_event_ack_ = true;
    return false;
  }

  virtual void OnAcceleratedWidgetAvailable(
      gfx::AcceleratedWidget widget) OVERRIDE {
    widget_ = widget;
    CreateCommandBufferIfNeeded();
  }

  virtual void OnBoundsChanged(const gfx::Rect& bounds) OVERRIDE {
    CreateCommandBufferIfNeeded();
    client()->OnBoundsChanged(Rect::From(bounds));
  }

  virtual void OnDestroyed() OVERRIDE {
    command_buffer_.reset();
    client()->OnDestroyed();
    base::MessageLoop::current()->Quit();
  }

 private:
  shell::Context* context_;
  gfx::AcceleratedWidget widget_;
  scoped_ptr<services::NativeViewport> native_viewport_;
  InterfaceRequest<CommandBuffer> command_buffer_request_;
  scoped_ptr<CommandBufferImpl> command_buffer_;
  bool waiting_for_event_ack_;
  base::WeakPtrFactory<NativeViewportImpl> weak_factory_;
};

}  // namespace services
}  // namespace mojo


MOJO_NATIVE_VIEWPORT_EXPORT mojo::Application*
    CreateNativeViewportService(
        mojo::shell::Context* context,
        mojo::ScopedMessagePipeHandle service_provider_handle) {
  mojo::Application* app = new mojo::Application(
      service_provider_handle.Pass());
  app->AddService<mojo::services::NativeViewportImpl>(context);
  return app;
}
