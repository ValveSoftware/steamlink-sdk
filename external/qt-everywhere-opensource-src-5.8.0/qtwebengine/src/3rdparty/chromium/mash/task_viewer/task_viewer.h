// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_TASK_VIEWER_TASK_VIEWER_H_
#define MASH_TASK_VIEWER_TASK_VIEWER_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"

namespace views {
class AuraInit;
class Widget;
class WindowManagerConnection;
}

namespace mash {
namespace task_viewer {

class TaskViewer : public shell::ShellClient,
                   public mojom::Launchable,
                   public shell::InterfaceFactory<mojom::Launchable> {
 public:
  TaskViewer();
  ~TaskViewer() override;

  void RemoveWindow(views::Widget* widget);

 private:
  // shell::ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(shell::Connection* connection) override;

  // mojom::Launchable:
  void Launch(uint32_t what, mojom::LaunchMode how) override;

  // shell::InterfaceFactory<mojom::Launchable>:
  void Create(shell::Connection* connection,
              mojom::LaunchableRequest request) override;

  shell::Connector* connector_ = nullptr;
  mojo::BindingSet<mojom::Launchable> bindings_;
  std::vector<views::Widget*> windows_;

  mojo::TracingImpl tracing_;
  std::unique_ptr<views::AuraInit> aura_init_;
  std::unique_ptr<views::WindowManagerConnection> window_manager_connection_;

  DISALLOW_COPY_AND_ASSIGN(TaskViewer);
};

}  // namespace task_viewer
}  // namespace mash

#endif  // MASH_TASK_VIEWER_TASK_VIEWER_H_
