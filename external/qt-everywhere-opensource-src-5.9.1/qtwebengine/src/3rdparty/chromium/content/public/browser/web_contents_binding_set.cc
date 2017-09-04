// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_binding_set.h"

#include <utility>

#include "base/logging.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

void WebContentsBindingSet::Binder::OnRequestForFrame(
    RenderFrameHost* render_frame_host,
    mojo::ScopedInterfaceEndpointHandle handle) {
  NOTREACHED();
}

WebContentsBindingSet::WebContentsBindingSet(WebContents* web_contents,
                                             const std::string& interface_name,
                                             std::unique_ptr<Binder> binder)
    : remove_callback_(
          static_cast<WebContentsImpl*>(web_contents)->AddBindingSet(
              interface_name, this)),
      binder_(std::move(binder)) {}

WebContentsBindingSet::~WebContentsBindingSet() {
  remove_callback_.Run();
}

void WebContentsBindingSet::CloseAllBindings() {
  binder_.reset();
}

void WebContentsBindingSet::OnRequestForFrame(
    RenderFrameHost* render_frame_host,
    mojo::ScopedInterfaceEndpointHandle handle) {
  DCHECK(binder_);
  binder_->OnRequestForFrame(render_frame_host, std::move(handle));
}

}  // namespace content
