// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/network_service_impl.h"

#include "mojo/services/network/url_loader_impl.h"

namespace mojo {

NetworkServiceImpl::NetworkServiceImpl(NetworkContext* context)
    : context_(context) {
}

NetworkServiceImpl::~NetworkServiceImpl() {
}

void NetworkServiceImpl::CreateURLLoader(
    InterfaceRequest<URLLoader> loader) {
  BindToRequest(new URLLoaderImpl(context_), &loader);
}

}  // namespace mojo
