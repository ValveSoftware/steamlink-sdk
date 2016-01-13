// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/examples/window_manager/window_manager.mojom.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/node_observer.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"
#include "url/url_util.h"

using mojo::view_manager::Node;
using mojo::view_manager::NodeObserver;
using mojo::view_manager::View;
using mojo::view_manager::ViewManager;
using mojo::view_manager::ViewManagerDelegate;
using mojo::view_manager::ViewObserver;

namespace mojo {
namespace examples {

class EmbeddedApp : public Application,
                    public ViewManagerDelegate,
                    public ViewObserver,
                    public NodeObserver {
 public:
  EmbeddedApp() : view_manager_(NULL) {
    url::AddStandardScheme("mojo");
  }
  virtual ~EmbeddedApp() {}

  void SetNodeColor(uint32 node_id, SkColor color) {
    pending_node_colors_[node_id] = color;
    ProcessPendingNodeColor(node_id);
  }

 private:
  class Navigator : public InterfaceImpl<navigation::Navigator> {
   public:
    explicit Navigator(EmbeddedApp* app) : app_(app) {}
   private:
    virtual void Navigate(
        uint32 node_id,
        navigation::NavigationDetailsPtr navigation_details,
        navigation::ResponseDetailsPtr response_details) OVERRIDE {
      GURL url(navigation_details->url.To<std::string>());
      if (!url.is_valid()) {
        LOG(ERROR) << "URL is invalid.";
        return;
      }
      // TODO(aa): Verify new URL is same origin as current origin.
      SkColor color = 0x00;
      if (!base::HexStringToUInt(url.path().substr(1), &color)) {
        LOG(ERROR) << "Invalid URL, path not convertible to integer";
        return;
      }
      app_->SetNodeColor(node_id, color);
    }
    EmbeddedApp* app_;
    DISALLOW_COPY_AND_ASSIGN(Navigator);
  };

  // Overridden from Application:
  virtual void Initialize() MOJO_OVERRIDE {
    ViewManager::Create(this, this);
    // TODO(aa): Weird for embeddee to talk to embedder by URL. Seems like
    // embedder should be able to specify the SP embeddee receives, then
    // communication can be anonymous.
    ConnectTo<IWindowManager>("mojo:mojo_window_manager", &window_manager_);
    AddService<Navigator>(this);
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnRootAdded(ViewManager* view_manager, Node* root) OVERRIDE {
    View* view = View::Create(view_manager);
    view->AddObserver(this);
    root->SetActiveView(view);
    root->AddObserver(this);

    roots_[root->id()] = root;
    ProcessPendingNodeColor(root->id());
  }

  // Overridden from ViewObserver:
  virtual void OnViewInputEvent(View* view, const EventPtr& event) OVERRIDE {
    if (event->action == ui::ET_MOUSE_RELEASED)
      window_manager_->CloseWindow(view->node()->id());
  }

  // Overridden from NodeObserver:
  virtual void OnNodeActiveViewChange(
      Node* node,
      View* old_view,
      View* new_view,
      NodeObserver::DispositionChangePhase phase) OVERRIDE {
    if (new_view == 0)
      views_to_reap_[node] = old_view;
  }
  virtual void OnNodeDestroy(
      Node* node,
      NodeObserver::DispositionChangePhase phase) OVERRIDE {
    if (phase != NodeObserver::DISPOSITION_CHANGED)
      return;
    DCHECK(roots_.find(node->id()) != roots_.end());
    roots_.erase(node->id());
    std::map<Node*, View*>::const_iterator it = views_to_reap_.find(node);
    if (it != views_to_reap_.end())
      it->second->Destroy();
  }

  void ProcessPendingNodeColor(uint32 node_id) {
    RootMap::iterator root = roots_.find(node_id);
    if (root == roots_.end())
      return;

    PendingNodeColors::iterator color = pending_node_colors_.find(node_id);
    if (color == pending_node_colors_.end())
      return;

    root->second->active_view()->SetColor(color->second);
    pending_node_colors_.erase(color);
  }

  view_manager::ViewManager* view_manager_;
  IWindowManagerPtr window_manager_;
  std::map<Node*, View*> views_to_reap_;

  typedef std::map<view_manager::Id, Node*> RootMap;
  RootMap roots_;

  // We can receive navigations for nodes we don't have yet.
  typedef std::map<uint32, SkColor> PendingNodeColors;
  PendingNodeColors pending_node_colors_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedApp);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::EmbeddedApp;
}

}  // namespace mojo
