// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/leveldb_app.h"

#include "base/message_loop/message_loop.h"
#include "components/leveldb/leveldb_service_impl.h"
#include "services/shell/public/cpp/connection.h"

namespace leveldb {

LevelDBApp::LevelDBApp() {}

LevelDBApp::~LevelDBApp() {}

void LevelDBApp::Initialize(shell::Connector* connector,
                            const shell::Identity& identity,
                            uint32_t id) {
  tracing_.Initialize(connector, identity.name());
}

bool LevelDBApp::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface<mojom::LevelDBService>(this);
  return true;
}

void LevelDBApp::Create(shell::Connection* connection,
                        leveldb::mojom::LevelDBServiceRequest request) {
  if (!service_)
    service_.reset(
        new LevelDBServiceImpl(base::MessageLoop::current()->task_runner()));
  bindings_.AddBinding(service_.get(), std::move(request));
}

}  // namespace leveldb
