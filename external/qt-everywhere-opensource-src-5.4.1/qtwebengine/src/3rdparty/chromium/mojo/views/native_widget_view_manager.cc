// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/views/native_widget_view_manager.h"

#include "mojo/aura/window_tree_host_mojo.h"
#include "mojo/services/public/cpp/input_events/input_events_type_converters.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/focus_controller.h"

namespace mojo {
namespace {

// TODO: figure out what this should be.
class FocusRulesImpl : public wm::BaseFocusRules {
 public:
  FocusRulesImpl() {}
  virtual ~FocusRulesImpl() {}

  virtual bool SupportsChildActivation(aura::Window* window) const OVERRIDE {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusRulesImpl);
};

class MinimalInputEventFilter : public ui::internal::InputMethodDelegate,
                                public ui::EventHandler {
 public:
  explicit MinimalInputEventFilter(aura::Window* root)
      : root_(root),
        input_method_(
            ui::CreateInputMethod(this, gfx::kNullAcceleratedWidget).Pass()) {
    ui::InitializeInputMethodForTesting();
    input_method_->Init(true);
    root_->AddPreTargetHandler(this);
    root_->SetProperty(aura::client::kRootWindowInputMethodKey,
                       input_method_.get());
  }

  virtual ~MinimalInputEventFilter() {
    root_->RemovePreTargetHandler(this);
    root_->SetProperty(aura::client::kRootWindowInputMethodKey,
                       static_cast<ui::InputMethod*>(NULL));
  }

 private:
  // ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    // See the comment in InputMethodEventFilter::OnKeyEvent() for details.
    if (event->IsTranslated()) {
      event->SetTranslated(false);
    } else {
      if (input_method_->DispatchKeyEvent(*event))
        event->StopPropagation();
    }
  }

  // ui::internal::InputMethodDelegate:
  virtual bool DispatchKeyEventPostIME(const ui::KeyEvent& event) OVERRIDE {
    // See the comment in InputMethodEventFilter::DispatchKeyEventPostIME() for
    // details.
    ui::KeyEvent aura_event(event);
    aura_event.SetTranslated(true);
    ui::EventDispatchDetails details =
        root_->GetHost()->dispatcher()->OnEventFromSource(&aura_event);
    return aura_event.handled() || details.dispatcher_destroyed;
  }

  aura::Window* root_;
  scoped_ptr<ui::InputMethod> input_method_;

  DISALLOW_COPY_AND_ASSIGN(MinimalInputEventFilter);
};

}  // namespace

NativeWidgetViewManager::NativeWidgetViewManager(
    views::internal::NativeWidgetDelegate* delegate, view_manager::Node* node)
    : NativeWidgetAura(delegate),
      node_(node) {
  node_->active_view()->AddObserver(this);
  window_tree_host_.reset(new WindowTreeHostMojo(node_, this));
  window_tree_host_->InitHost();

  ime_filter_.reset(
      new MinimalInputEventFilter(window_tree_host_->window()));

  focus_client_.reset(new wm::FocusController(new FocusRulesImpl));

  aura::client::SetFocusClient(window_tree_host_->window(),
                               focus_client_.get());
  aura::client::SetActivationClient(window_tree_host_->window(),
                                    focus_client_.get());
  window_tree_host_->window()->AddPreTargetHandler(focus_client_.get());
}

NativeWidgetViewManager::~NativeWidgetViewManager() {
  node_->active_view()->RemoveObserver(this);
}

void NativeWidgetViewManager::InitNativeWidget(
    const views::Widget::InitParams& in_params) {
  views::Widget::InitParams params(in_params);
  params.parent = window_tree_host_->window();
  NativeWidgetAura::InitNativeWidget(params);
}

void NativeWidgetViewManager::CompositorContentsChanged(
    const SkBitmap& bitmap) {
  node_->active_view()->SetContents(bitmap);
}

void NativeWidgetViewManager::OnViewInputEvent(view_manager::View* view,
                                               const EventPtr& event) {
  scoped_ptr<ui::Event> ui_event(event.To<scoped_ptr<ui::Event> >());
  if (ui_event.get())
    window_tree_host_->SendEventToProcessor(ui_event.get());
}

}  // namespace mojo
