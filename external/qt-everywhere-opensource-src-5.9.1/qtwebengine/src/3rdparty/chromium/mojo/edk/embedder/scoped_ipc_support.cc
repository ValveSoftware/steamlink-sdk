// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/scoped_ipc_support.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/process_delegate.h"

namespace mojo {
namespace edk {

namespace {
class IPCSupportInitializer : public mojo::edk::ProcessDelegate {
 public:
  IPCSupportInitializer() {}
  ~IPCSupportInitializer() override {}

  void Init(scoped_refptr<base::TaskRunner> io_thread_task_runner) {
    CHECK(!io_thread_task_runner_);
    CHECK(io_thread_task_runner);
    io_thread_task_runner_ = io_thread_task_runner;

    mojo::edk::InitIPCSupport(this, io_thread_task_runner_);
  }

  void ShutDown() {
    CHECK(io_thread_task_runner_);
    mojo::edk::ShutdownIPCSupport();
  }

 private:
  // mojo::edk::ProcessDelegate:
  void OnShutdownComplete() override {
    // TODO(rockot): We should ensure that IO runner shutdown is blocked until
    // this is called.
  }

  scoped_refptr<base::TaskRunner> io_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(IPCSupportInitializer);
};

base::LazyInstance<IPCSupportInitializer>::Leaky ipc_support_initializer;

}  // namespace

ScopedIPCSupport::ScopedIPCSupport(
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  ipc_support_initializer.Get().Init(io_thread_task_runner);
}

ScopedIPCSupport::~ScopedIPCSupport() {
  ipc_support_initializer.Get().ShutDown();
}

}  // namespace edk
}  // namespace mojo
