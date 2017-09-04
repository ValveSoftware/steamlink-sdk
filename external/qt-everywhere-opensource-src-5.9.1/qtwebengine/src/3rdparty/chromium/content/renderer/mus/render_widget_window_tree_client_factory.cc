// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mus/render_widget_window_tree_client_factory.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/render_widget_window_tree_client_factory.mojom.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/service_manager_connection.h"
#include "content/renderer/mus/render_widget_mus_connection.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

void BindMusConnectionOnMainThread(
    uint32_t routing_id,
    ui::mojom::WindowTreeClientRequest request) {
  RenderWidgetMusConnection* connection =
      RenderWidgetMusConnection::GetOrCreate(routing_id);
  connection->Bind(std::move(request));
}

// This object's lifetime is managed by ServiceManagerConnection because it's a
// registered with it.
class RenderWidgetWindowTreeClientFactoryImpl
    : public ConnectionFilter,
      public mojom::RenderWidgetWindowTreeClientFactory {
 public:
  RenderWidgetWindowTreeClientFactoryImpl() : weak_factory_(this) {
    main_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }

  ~RenderWidgetWindowTreeClientFactoryImpl() override {}

 private:
  // ConnectionFilter implementation:
  bool OnConnect(const service_manager::Identity& remote_identity,
                 service_manager::InterfaceRegistry* registry,
                 service_manager::Connector* connector) override {
    registry->AddInterface(
        base::Bind(&RenderWidgetWindowTreeClientFactoryImpl::CreateFactory,
                   weak_factory_.GetWeakPtr()));
    return true;
  }

  // mojom::RenderWidgetWindowTreeClientFactory implementation.
  void CreateWindowTreeClientForRenderWidget(
      uint32_t routing_id,
      ui::mojom::WindowTreeClientRequest request) override {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&BindMusConnectionOnMainThread, routing_id,
                              base::Passed(&request)));
  }

  void CreateFactory(
      mojom::RenderWidgetWindowTreeClientFactoryRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;
  mojo::BindingSet<mojom::RenderWidgetWindowTreeClientFactory> bindings_;
  base::WeakPtrFactory<RenderWidgetWindowTreeClientFactoryImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetWindowTreeClientFactoryImpl);
};

}  // namespace

void CreateRenderWidgetWindowTreeClientFactory(
    ServiceManagerConnection* connection) {
  connection->AddConnectionFilter(
      base::MakeUnique<RenderWidgetWindowTreeClientFactoryImpl>());
}

}  // namespace content
