// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/navigation/navigation.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "services/navigation/view_impl.h"

namespace navigation {

Navigation::Navigation()
    : ref_factory_(base::MessageLoop::QuitWhenIdleClosure()) {
  bindings_.set_connection_error_handler(
      base::Bind(&Navigation::ViewFactoryLost, base::Unretained(this)));
}
Navigation::~Navigation() {}

void Navigation::Initialize(shell::Connector* connector,
                            const shell::Identity& identity,
                            uint32_t instance_id) {
  connector_ = connector;
}

bool Navigation::AcceptConnection(shell::Connection* connection) {
  std::string remote_user_id = connection->GetRemoteIdentity().user_id();
  if (!client_user_id_.empty() && client_user_id_ != remote_user_id) {
    LOG(ERROR) << "Must have a separate Navigation service instance for "
               << "different BrowserContexts.";
    return false;
  }
  client_user_id_ = remote_user_id;

  connection->AddInterface<mojom::ViewFactory>(this);
  return true;
}

void Navigation::Create(shell::Connection* connection,
                        mojom::ViewFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
  refs_.insert(ref_factory_.CreateRef());
}

void Navigation::CreateView(mojom::ViewClientPtr client,
                            mojom::ViewRequest request) {
  new ViewImpl(connector_, client_user_id_, std::move(client),
               std::move(request), ref_factory_.CreateRef());
}

void Navigation::ViewFactoryLost() {
  refs_.erase(refs_.begin());
}

}  // navigation
