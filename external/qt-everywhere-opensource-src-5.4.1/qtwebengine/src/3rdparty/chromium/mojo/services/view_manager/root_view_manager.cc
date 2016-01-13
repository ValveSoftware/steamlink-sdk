// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/root_view_manager.h"

#include "base/auto_reset.h"
#include "base/scoped_observer.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/services/view_manager/root_node_manager.h"
#include "mojo/services/view_manager/root_view_manager_delegate.h"
#include "mojo/services/view_manager/screen_impl.h"
#include "mojo/services/view_manager/window_tree_host_impl.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace mojo {
namespace view_manager {
namespace service {

// TODO(sky): revisit this, we may need a more sophisticated FocusClient
// implementation.
class FocusClientImpl : public aura::client::FocusClient,
                        public aura::WindowObserver {
 public:
  FocusClientImpl()
      : focused_window_(NULL),
        observer_manager_(this) {
  }
  virtual ~FocusClientImpl() {}

 private:
  // Overridden from aura::client::FocusClient:
  virtual void AddObserver(aura::client::FocusChangeObserver* observer)
      OVERRIDE {
  }
  virtual void RemoveObserver(aura::client::FocusChangeObserver* observer)
      OVERRIDE {
  }
  virtual void FocusWindow(aura::Window* window) OVERRIDE {
    if (window && !window->CanFocus())
      return;
    if (focused_window_)
      observer_manager_.Remove(focused_window_);
    aura::Window* old_focused_window = focused_window_;
    focused_window_ = window;
    if (focused_window_)
      observer_manager_.Add(focused_window_);

    aura::client::FocusChangeObserver* observer =
        aura::client::GetFocusChangeObserver(old_focused_window);
    if (observer)
      observer->OnWindowFocused(focused_window_, old_focused_window);
    observer = aura::client::GetFocusChangeObserver(focused_window_);
    if (observer)
      observer->OnWindowFocused(focused_window_, old_focused_window);
  }
  virtual void ResetFocusWithinActiveWindow(aura::Window* window) OVERRIDE {
    if (!window->Contains(focused_window_))
      FocusWindow(window);
  }
  virtual aura::Window* GetFocusedWindow() OVERRIDE {
    return focused_window_;
  }

  // Overridden from WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    DCHECK_EQ(window, focused_window_);
    FocusWindow(NULL);
  }

  aura::Window* focused_window_;
  ScopedObserver<aura::Window, aura::WindowObserver> observer_manager_;

  DISALLOW_COPY_AND_ASSIGN(FocusClientImpl);
};

class WindowTreeClientImpl : public aura::client::WindowTreeClient {
 public:
  explicit WindowTreeClientImpl(aura::Window* window) : window_(window) {
    aura::client::SetWindowTreeClient(window_, this);
  }

  virtual ~WindowTreeClientImpl() {
    aura::client::SetWindowTreeClient(window_, NULL);
  }

  // Overridden from aura::client::WindowTreeClient:
  virtual aura::Window* GetDefaultParent(aura::Window* context,
                                         aura::Window* window,
                                         const gfx::Rect& bounds) OVERRIDE {
    if (!capture_client_) {
      capture_client_.reset(
          new aura::client::DefaultCaptureClient(window_->GetRootWindow()));
    }
    return window_;
  }

 private:
  aura::Window* window_;

  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientImpl);
};

RootViewManager::RootViewManager(ServiceProvider* service_provider,
                                 RootNodeManager* root_node,
                                 RootViewManagerDelegate* delegate)
    : delegate_(delegate),
      root_node_manager_(root_node),
      in_setup_(false) {
  screen_.reset(ScreenImpl::Create());
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());
  NativeViewportPtr viewport;
  ConnectToService(service_provider,
                   "mojo:mojo_native_viewport_service",
                   &viewport);
  window_tree_host_.reset(new WindowTreeHostImpl(
        viewport.Pass(),
        gfx::Rect(800, 600),
        base::Bind(&RootViewManager::OnCompositorCreated,
                   base::Unretained(this))));
}

RootViewManager::~RootViewManager() {
  window_tree_client_.reset();
  window_tree_host_.reset();
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, NULL);
}

void RootViewManager::OnCompositorCreated() {
  base::AutoReset<bool> resetter(&in_setup_, true);
  window_tree_host_->InitHost();

  aura::Window* root = root_node_manager_->root()->window();
  window_tree_host_->window()->AddChild(root);
  root->SetBounds(gfx::Rect(window_tree_host_->window()->bounds().size()));
  root_node_manager_->root()->window()->Show();

  window_tree_client_.reset(
      new WindowTreeClientImpl(window_tree_host_->window()));

  focus_client_.reset(new FocusClientImpl());
  aura::client::SetFocusClient(window_tree_host_->window(),
                               focus_client_.get());

  window_tree_host_->Show();

  delegate_->OnRootViewManagerWindowTreeHostCreated();
}

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
