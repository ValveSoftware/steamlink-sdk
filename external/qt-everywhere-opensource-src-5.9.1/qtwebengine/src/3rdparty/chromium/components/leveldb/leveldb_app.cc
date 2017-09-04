// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/leveldb_app.h"

#include "base/threading/thread_task_runner_handle.h"
#include "components/leveldb/leveldb_service_impl.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace leveldb {

LevelDBApp::LevelDBApp() {}

LevelDBApp::~LevelDBApp() {}

void LevelDBApp::OnStart() {
  tracing_.Initialize(context()->connector(), context()->identity().name());
}

bool LevelDBApp::OnConnect(const service_manager::ServiceInfo& remote_info,
                           service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::LevelDBService>(this);
  return true;
}

void LevelDBApp::Create(const service_manager::Identity& remote_identity,
                        leveldb::mojom::LevelDBServiceRequest request) {
  if (!service_)
    service_.reset(new LevelDBServiceImpl(base::ThreadTaskRunnerHandle::Get()));
  bindings_.AddBinding(service_.get(), std::move(request));
}

}  // namespace leveldb
