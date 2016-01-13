// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell_test_helper.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "mojo/shell/init.h"

namespace mojo {
namespace shell {

class ShellTestHelper::TestServiceProvider : public ServiceProvider {
 public:
  TestServiceProvider() {}
  virtual ~TestServiceProvider() {}

  // ServiceProvider:
  virtual void ConnectToService(
      const mojo::String& service_url,
      const mojo::String& service_name,
      ScopedMessagePipeHandle client_handle,
      const mojo::String& requestor_url) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestServiceProvider);
};

ShellTestHelper::ShellTestHelper() {
  base::CommandLine::Init(0, NULL);
  mojo::shell::InitializeLogging();
}

ShellTestHelper::~ShellTestHelper() {
}

void ShellTestHelper::Init() {
  context_.reset(new Context);
  test_api_.reset(new ServiceManager::TestAPI(context_->service_manager()));
  local_service_provider_.reset(new TestServiceProvider);
  service_provider_.Bind(test_api_->GetServiceProviderHandle().Pass());
  service_provider_.set_client(local_service_provider_.get());
}

void ShellTestHelper::SetLoaderForURL(scoped_ptr<ServiceLoader> loader,
                                      const GURL& url) {
  context_->service_manager()->SetLoaderForURL(loader.Pass(), url);
}

}  // namespace shell
}  // namespace mojo
