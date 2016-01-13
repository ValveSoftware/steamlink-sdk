// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "mojo/examples/window_manager/window_manager.mojom.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/interfaces/launcher/launcher.mojom.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "ui/events/event_constants.h"

#if defined CreateWindow
#undef CreateWindow
#endif

using mojo::view_manager::Id;
using mojo::view_manager::Node;
using mojo::view_manager::NodeObserver;
using mojo::view_manager::View;
using mojo::view_manager::ViewManager;
using mojo::view_manager::ViewManagerDelegate;
using mojo::view_manager::ViewObserver;

namespace mojo {
namespace examples {

class WindowManager;

namespace {

const SkColor kColors[] = { SK_ColorYELLOW,
                            SK_ColorRED,
                            SK_ColorGREEN,
                            SK_ColorMAGENTA };

}  // namespace

class WindowManagerConnection : public InterfaceImpl<IWindowManager> {
 public:
  explicit WindowManagerConnection(WindowManager* window_manager)
      : window_manager_(window_manager) {}
  virtual ~WindowManagerConnection() {}

 private:
  // Overridden from IWindowManager:
  virtual void CloseWindow(Id node_id) OVERRIDE;

  WindowManager* window_manager_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerConnection);
};

class NavigatorHost : public InterfaceImpl<navigation::NavigatorHost> {
 public:
  explicit NavigatorHost(WindowManager* window_manager)
      : window_manager_(window_manager) {
  }
  virtual ~NavigatorHost() {
  }
 private:
  virtual void RequestNavigate(
      uint32 source_node_id,
      navigation::NavigationDetailsPtr nav_details) OVERRIDE;
  WindowManager* window_manager_;
  DISALLOW_COPY_AND_ASSIGN(NavigatorHost);
};

class WindowManager : public Application,
                      public ViewObserver,
                      public ViewManagerDelegate,
                      public InterfaceImpl<launcher::LauncherClient> {
 public:
  WindowManager() : launcher_ui_(NULL), view_manager_(NULL) {}
  virtual ~WindowManager() {}

  void CloseWindow(Id node_id) {
    Node* node = view_manager_->GetNodeById(node_id);
    DCHECK(node);
    std::vector<Node*>::iterator iter =
        std::find(windows_.begin(), windows_.end(), node);
    DCHECK(iter != windows_.end());
    windows_.erase(iter);
    node->Destroy();
  }

  void RequestNavigate(
    uint32 source_node_id,
    navigation::NavigationDetailsPtr nav_details) {
    if (!launcher_.get()) {
      ConnectTo("mojo:mojo_launcher", &launcher_);
      launcher_.set_client(this);
    }
    launcher_->Launch(nav_details->url);
  }

 private:
  // Overridden from Application:
  virtual void Initialize() MOJO_OVERRIDE {
    AddService<WindowManagerConnection>(this);
    AddService<NavigatorHost>(this);
    ViewManager::Create(this, this);
  }

    // Overridden from ViewObserver:
  virtual void OnViewInputEvent(View* view, const EventPtr& event) OVERRIDE {
    if (event->action == ui::ET_MOUSE_RELEASED) {
      std::string app_url;
      if (event->flags & ui::EF_LEFT_MOUSE_BUTTON)
        app_url = "mojo://mojo_embedded_app";
      else if (event->flags & ui::EF_RIGHT_MOUSE_BUTTON)
        app_url = "mojo://mojo_nesting_app";
      if (app_url.empty())
        return;

      Node* node = view_manager_->GetNodeById(parent_node_id_);
      navigation::NavigationDetailsPtr nav_details(
          navigation::NavigationDetails::New());
      size_t index = node->children().size() - 1;
      nav_details->url = base::StringPrintf(
          "%s/%x", app_url.c_str(), kColors[index % arraysize(kColors)]);
      navigation::ResponseDetailsPtr response;
      CreateWindow(app_url, nav_details.Pass(), response.Pass());
    }
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnRootAdded(ViewManager* view_manager, Node* root) OVERRIDE {
    DCHECK(!view_manager_);
    view_manager_ = view_manager;

    Node* node = Node::Create(view_manager);
    view_manager->GetRoots().front()->AddChild(node);
    node->SetBounds(gfx::Rect(800, 600));
    parent_node_id_ = node->id();

    View* view = View::Create(view_manager);
    node->SetActiveView(view);
    view->SetColor(SK_ColorBLUE);
    view->AddObserver(this);

    CreateLauncherUI();
  }

  // Overridden from LauncherClient:
  virtual void OnLaunch(
      const mojo::String& requested_url, const mojo::String& handler_url,
      navigation::ResponseDetailsPtr response) OVERRIDE {
    navigation::NavigationDetailsPtr nav_details(
        navigation::NavigationDetails::New());
    nav_details->url = requested_url;
    CreateWindow(handler_url, nav_details.Pass(), response.Pass());
  }

  void CreateLauncherUI() {
    navigation::NavigationDetailsPtr nav_details;
    navigation::ResponseDetailsPtr response;
    launcher_ui_ = CreateChild("mojo:mojo_browser", gfx::Rect(25, 25, 400, 25),
                               nav_details.Pass(), response.Pass());
  }

  void CreateWindow(const std::string& handler_url,
                    navigation::NavigationDetailsPtr nav_details,
                    navigation::ResponseDetailsPtr response) {
    gfx::Rect bounds(25, 75, 400, 400);
    if (!windows_.empty()) {
      gfx::Point position = windows_.back()->bounds().origin();
      position.Offset(35, 35);
      bounds.set_origin(position);
    }
    windows_.push_back(CreateChild(handler_url, bounds, nav_details.Pass(),
                                   response.Pass()));
  }

  Node* CreateChild(const std::string& url,
                    const gfx::Rect& bounds,
                    navigation::NavigationDetailsPtr nav_details,
                    navigation::ResponseDetailsPtr response) {
    Node* node = view_manager_->GetNodeById(parent_node_id_);
    Node* embedded = Node::Create(view_manager_);
    node->AddChild(embedded);
    embedded->SetBounds(bounds);
    embedded->Embed(url);

    if (nav_details.get()) {
      navigation::NavigatorPtr navigator;
      ConnectTo(url, &navigator);
      navigator->Navigate(embedded->id(), nav_details.Pass(), response.Pass());
    }

    return embedded;
  }

  launcher::LauncherPtr launcher_;
  Node* launcher_ui_;
  std::vector<Node*> windows_;
  ViewManager* view_manager_;
  Id parent_node_id_;

  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

void WindowManagerConnection::CloseWindow(Id node_id) {
  window_manager_->CloseWindow(node_id);
}

void NavigatorHost::RequestNavigate(
    uint32 source_node_id,
    navigation::NavigationDetailsPtr nav_details) {
  window_manager_->RequestNavigate(source_node_id, nav_details.Pass());
}

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::WindowManager;
}

}  // namespace mojo
