// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_BINDING_SET_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_BINDING_SET_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace content {

class RenderFrameHost;
class WebContentsImpl;

// Base class for something which owns a mojo::AssociatedBindingSet on behalf
// of a WebContents. See WebContentsFrameBindingSet<T> below.
class CONTENT_EXPORT WebContentsBindingSet {
 protected:
  class CONTENT_EXPORT Binder {
   public:
    virtual ~Binder() {}

    virtual void OnRequestForFrame(
        RenderFrameHost* render_frame_host,
        mojo::ScopedInterfaceEndpointHandle handle);
  };

  WebContentsBindingSet(WebContents* web_contents,
                        const std::string& interface_name,
                        std::unique_ptr<Binder> binder);
  ~WebContentsBindingSet();

 private:
  friend class WebContentsImpl;

  void CloseAllBindings();
  void OnRequestForFrame(RenderFrameHost* render_frame_host,
                         mojo::ScopedInterfaceEndpointHandle handle);

  const base::Closure remove_callback_;
  std::unique_ptr<Binder> binder_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsBindingSet);
};

// Owns a set of Channel-associated interface bindings with frame context on
// message dispatch.
//
// To use this, a |mojom::Foo| implementation need only own an instance of
// WebContentsFrameBindingSet<mojom::Foo>. This allows remote RenderFrames to
// acquire handles to the |mojom::Foo| interface via
// RenderFrame::GetRemoteAssociatedInterfaces() and send messages here. When
// messages are dispatched to the implementation, the implementation can call
// GetCurrentTargetFrame() on this object (see below) to determine which
// frame sent the message.
//
// For example:
//
//   class FooImpl : public mojom::Foo {
//    public:
//     explicit FooImpl(WebContents* web_contents)
//         : web_contents_(web_contents), bindings_(web_contents, this) {}
//
//     // mojom::Foo:
//     void DoFoo() override {
//       if (bindings_.GetCurrentTargetFrame() == web_contents_->GetMainFrame())
//           ; // Do something interesting
//     }
//
//    private:
//     WebContents* web_contents_;
//     WebContentsFrameBindingSet<mojom::Foo> bindings_;
//   };
//
// When an instance of FooImpl is constructed over a WebContents, the mojom::Foo
// interface will be exposed to all remote RenderFrame objects. If the
// WebContents is destroyed at any point, the bindings will automatically reset
// and will cease to dispatch further incoming messages.
//
// If FooImpl is destroyed first, the bindings are automatically removed and
// future incoming interface requests for mojom::Foo will be rejected.
//
// Because this object uses Channel-associated interface bindings, all messages
// sent via these interfaces are ordered with respect to legacy Chrome IPC
// messages on the relevant IPC::Channel (i.e. the Channel between the browser
// and whatever render process hosts the sending frame.)
template <typename Interface>
class WebContentsFrameBindingSet : public WebContentsBindingSet {
 public:
  WebContentsFrameBindingSet(WebContents* web_contents, Interface* impl)
      : WebContentsBindingSet(
          web_contents, Interface::Name_,
          base::MakeUnique<FrameInterfaceBinder>(this, impl)) {}
  ~WebContentsFrameBindingSet() {}

  // Returns the RenderFrameHost currently targeted by a message dispatch to
  // this interface. Must only be called during the extent of a message dispatch
  // for this interface.
  RenderFrameHost* GetCurrentTargetFrame() {
    DCHECK(current_target_frame_);
    return current_target_frame_;
  }

  void SetCurrentTargetFrameForTesting(RenderFrameHost* render_frame_host) {
    current_target_frame_ = render_frame_host;
  }

 private:
  class FrameInterfaceBinder : public Binder {
   public:
    FrameInterfaceBinder(WebContentsFrameBindingSet* binding_set,
                         Interface* impl)
        : impl_(impl), bindings_(mojo::BindingSetDispatchMode::WITH_CONTEXT) {
      bindings_.set_pre_dispatch_handler(
          base::Bind(&WebContentsFrameBindingSet::WillDispatchForContext,
                     base::Unretained(binding_set)));
    }

    ~FrameInterfaceBinder() override {}

    // Binder:
    void OnRequestForFrame(
        RenderFrameHost* render_frame_host,
        mojo::ScopedInterfaceEndpointHandle handle) override {
      mojo::AssociatedInterfaceRequest<Interface> request;
      request.Bind(std::move(handle));
      bindings_.AddBinding(impl_, std::move(request), render_frame_host);
    }

    Interface* const impl_;
    mojo::AssociatedBindingSet<Interface> bindings_;

    DISALLOW_COPY_AND_ASSIGN(FrameInterfaceBinder);
  };

  void WillDispatchForContext(void* context) {
    current_target_frame_ = static_cast<RenderFrameHost*>(context);
  }

  RenderFrameHost* current_target_frame_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebContentsFrameBindingSet);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_BINDING_SET_H_
