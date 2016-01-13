// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "mojo/views/native_widget_view_manager.h"
#include "mojo/views/views_init.h"
#include "ui/events/event.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace mojo {
namespace examples {

class BrowserLayoutManager : public views::LayoutManager {
 public:
  BrowserLayoutManager() {}
  virtual ~BrowserLayoutManager() {}

 private:
  // Overridden from views::LayoutManager:
  virtual void Layout(views::View* host) OVERRIDE {
    // Browser view has one child, a text input field.
    DCHECK_EQ(1, host->child_count());
    views::View* text_field = host->child_at(0);
    gfx::Size ps = text_field->GetPreferredSize();
    text_field->SetBoundsRect(gfx::Rect(host->width(), ps.height()));
  }
  virtual gfx::Size GetPreferredSize(const views::View* host) const OVERRIDE {
    return gfx::Size();
  }

  DISALLOW_COPY_AND_ASSIGN(BrowserLayoutManager);
};

// This is the basics of creating a views widget with a textfield.
// TODO: cleanup!
class Browser : public Application,
                public view_manager::ViewManagerDelegate,
                public views::TextfieldController {
 public:
  Browser() : view_manager_(NULL) {}

  virtual ~Browser() {
  }

 private:
  // Overridden from Application:
  virtual void Initialize() MOJO_OVERRIDE {
    views_init_.reset(new ViewsInit);
    view_manager::ViewManager::Create(this, this);
    ConnectTo("mojo:mojo_window_manager", &navigator_host_);
  }

  void CreateWidget(view_manager::Node* node) {
    views::Textfield* textfield = new views::Textfield;
    textfield->set_controller(this);

    views::WidgetDelegateView* widget_delegate = new views::WidgetDelegateView;
    widget_delegate->GetContentsView()->AddChildView(textfield);
    widget_delegate->GetContentsView()->SetLayoutManager(
        new BrowserLayoutManager);

    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.native_widget = new NativeWidgetViewManager(widget, node);
    params.delegate = widget_delegate;
    params.bounds = gfx::Rect(node->bounds().width(), node->bounds().height());
    widget->Init(params);
    widget->Show();
    textfield->RequestFocus();
  }

  // view_manager::ViewManagerDelegate:
  virtual void OnRootAdded(view_manager::ViewManager* view_manager,
                           view_manager::Node* root) OVERRIDE {
    // TODO: deal with OnRootAdded() being invoked multiple times.
    view_manager_ = view_manager;
    root->SetActiveView(view_manager::View::Create(view_manager));
    root->SetFocus();
    CreateWidget(root);
  }

  // views::TextfieldController:
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const ui::KeyEvent& key_event) OVERRIDE {
    if (key_event.key_code() == ui::VKEY_RETURN) {
      GURL url(sender->text());
      printf("User entered this URL: %s\n", url.spec().c_str());
      navigation::NavigationDetailsPtr nav_details(
          navigation::NavigationDetails::New());
      nav_details->url = url.spec();
      navigator_host_->RequestNavigate(view_manager_->GetRoots().front()->id(),
                                       nav_details.Pass());
    }
    return false;
  }

  scoped_ptr<ViewsInit> views_init_;

  view_manager::ViewManager* view_manager_;
  navigation::NavigatorHostPtr navigator_host_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::Browser;
}

}  // namespace mojo
