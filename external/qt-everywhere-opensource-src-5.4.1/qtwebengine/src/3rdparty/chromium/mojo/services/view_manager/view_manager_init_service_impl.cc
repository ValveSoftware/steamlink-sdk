// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_manager_init_service_impl.h"

#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/view_manager_service_impl.h"

namespace mojo {
namespace view_manager {
namespace service {

ViewManagerInitServiceImpl::ConnectParams::ConnectParams() {}

ViewManagerInitServiceImpl::ConnectParams::~ConnectParams() {}

ViewManagerInitServiceImpl::ViewManagerInitServiceImpl(
    ServiceProvider* service_provider)
    : service_provider_(service_provider),
      root_node_manager_(service_provider, this),
      is_tree_host_ready_(false) {
}

ViewManagerInitServiceImpl::~ViewManagerInitServiceImpl() {
}

void ViewManagerInitServiceImpl::MaybeEmbedRoot(
    const std::string& url,
    const Callback<void(bool)>& callback) {
  if (!is_tree_host_ready_)
    return;

  root_node_manager_.EmbedRoot(url);
  callback.Run(true);
}

void ViewManagerInitServiceImpl::EmbedRoot(
    const String& url,
    const Callback<void(bool)>& callback) {
  if (connect_params_.get()) {
    DVLOG(1) << "Ignoring second connect";
    callback.Run(false);
    return;
  }
  connect_params_.reset(new ConnectParams);
  connect_params_->url = url.To<std::string>();
  connect_params_->callback = callback;
  MaybeEmbedRoot(url.To<std::string>(), callback);
}

void ViewManagerInitServiceImpl::OnRootViewManagerWindowTreeHostCreated() {
  DCHECK(!is_tree_host_ready_);
  is_tree_host_ready_ = true;
  if (connect_params_.get())
    MaybeEmbedRoot(connect_params_->url, connect_params_->callback);
}

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
