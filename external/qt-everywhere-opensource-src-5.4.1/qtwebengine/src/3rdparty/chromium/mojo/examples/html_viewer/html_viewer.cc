// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/html_viewer/blink_platform_impl.h"
#include "mojo/examples/html_viewer/html_document_view.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "third_party/WebKit/public/web/WebKit.h"

namespace mojo {
namespace examples {

class HTMLViewer;

class NavigatorImpl : public InterfaceImpl<navigation::Navigator> {
 public:
  explicit NavigatorImpl(HTMLViewer* viewer) : viewer_(viewer) {}
  virtual ~NavigatorImpl() {}

 private:
  // Overridden from navigation::Navigator:
  virtual void Navigate(
      uint32_t node_id,
      navigation::NavigationDetailsPtr navigation_details,
      navigation::ResponseDetailsPtr response_details) OVERRIDE;

  HTMLViewer* viewer_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorImpl);
};

class HTMLViewer : public Application,
                   public view_manager::ViewManagerDelegate {
 public:
  HTMLViewer() : document_view_(NULL) {
  }
  virtual ~HTMLViewer() {
    blink::shutdown();
  }

  void Load(URLResponsePtr response,
            ScopedDataPipeConsumerHandle response_body_stream) {
    // Need to wait for OnRootAdded.
    response_ = response.Pass();
    response_body_stream_ = response_body_stream.Pass();
    MaybeLoad();
  }

 private:
  // Overridden from Application:
  virtual void Initialize() OVERRIDE {
    blink_platform_impl_.reset(new BlinkPlatformImpl(this));
    blink::initialize(blink_platform_impl_.get());

    AddService<NavigatorImpl>(this);
    view_manager::ViewManager::Create(this, this);
  }

  // Overridden from view_manager::ViewManagerDelegate:
  virtual void OnRootAdded(view_manager::ViewManager* view_manager,
                           view_manager::Node* root) OVERRIDE {
    document_view_ = new HTMLDocumentView(view_manager);
    document_view_->AttachToNode(root);
    MaybeLoad();
  }

  void MaybeLoad() {
    if (document_view_ && response_.get())
      document_view_->Load(response_.Pass(), response_body_stream_.Pass());
  }

  scoped_ptr<BlinkPlatformImpl> blink_platform_impl_;

  // TODO(darin): Figure out proper ownership of this instance.
  HTMLDocumentView* document_view_;
  URLResponsePtr response_;
  ScopedDataPipeConsumerHandle response_body_stream_;

  DISALLOW_COPY_AND_ASSIGN(HTMLViewer);
};

void NavigatorImpl::Navigate(
    uint32_t node_id,
    navigation::NavigationDetailsPtr navigation_details,
    navigation::ResponseDetailsPtr response_details) {
  printf("In HTMLViewer, rendering url: %s\n",
      response_details->response->url.data());
  viewer_->Load(response_details->response.Pass(),
                response_details->response_body_stream.Pass());
}

}

// static
Application* Application::Create() {
  return new examples::HTMLViewer;
}

}
