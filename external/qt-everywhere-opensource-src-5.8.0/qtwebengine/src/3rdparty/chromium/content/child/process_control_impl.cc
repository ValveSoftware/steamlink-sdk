// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/process_control_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "content/common/mojo/embedded_application_runner.h"
#include "content/public/common/content_client.h"

namespace content {

ProcessControlImpl::ProcessControlImpl() {
}

ProcessControlImpl::~ProcessControlImpl() {
}

void ProcessControlImpl::LoadApplication(
    const mojo::String& name,
    shell::mojom::ShellClientRequest request,
    const LoadApplicationCallback& callback) {
  // Only register apps on first run.
  if (!has_registered_apps_) {
    DCHECK(apps_.empty());
    ApplicationMap apps;
    RegisterApplications(&apps);
    for (const auto& app : apps) {
      std::unique_ptr<EmbeddedApplicationRunner> runner(
          new EmbeddedApplicationRunner(app.first, app.second));
      runner->SetQuitClosure(base::Bind(&ProcessControlImpl::OnApplicationQuit,
                                        base::Unretained(this)));
      apps_.insert(std::make_pair(app.first, std::move(runner)));
    }
    has_registered_apps_ = true;
  }

  auto it = apps_.find(name);
  if (it == apps_.end()) {
    callback.Run(false);
    OnLoadFailed();
    return;
  }

  callback.Run(true);
  it->second->BindShellClientRequest(std::move(request));
}

}  // namespace content
