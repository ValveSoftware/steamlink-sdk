// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/ui_resource_provider_impl.h"

#include "cc/resources/ui_resource_client.h"
#include "cc/trees/layer_tree_host.h"
#include "content/public/browser/android/ui_resource_client_android.h"

namespace content {

UIResourceProviderImpl::UIResourceProviderImpl() : host_(NULL) {
}

UIResourceProviderImpl::~UIResourceProviderImpl() {
  SetLayerTreeHost(NULL);
}

void UIResourceProviderImpl::SetLayerTreeHost(cc::LayerTreeHost* host) {
  if (host_ == host)
    return;
  host_ = host;
  UIResourcesAreInvalid();
}

void UIResourceProviderImpl::UIResourcesAreInvalid() {
  UIResourceClientMap client_map = ui_resource_client_map_;
  ui_resource_client_map_.clear();
  for (UIResourceClientMap::iterator iter = client_map.begin();
       iter != client_map.end();
       iter++) {
    iter->second->UIResourceIsInvalid();
  }
}

cc::UIResourceId UIResourceProviderImpl::CreateUIResource(
    UIResourceClientAndroid* client) {
  if (!host_)
    return 0;
  cc::UIResourceId id = host_->CreateUIResource(client);
  DCHECK(ui_resource_client_map_.find(id) == ui_resource_client_map_.end());

  ui_resource_client_map_[id] = client;
  return id;
}

void UIResourceProviderImpl::DeleteUIResource(cc::UIResourceId ui_resource_id) {
  UIResourceClientMap::iterator iter =
      ui_resource_client_map_.find(ui_resource_id);
  DCHECK(iter != ui_resource_client_map_.end());

  ui_resource_client_map_.erase(iter);

  if (!host_)
    return;
  host_->DeleteUIResource(ui_resource_id);
}

}  // namespace content
