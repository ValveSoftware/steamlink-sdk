// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_CONTEXT_PROVIDER_H_
#define COMPONENTS_MUS_PUBLIC_CPP_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "cc/output/context_provider.h"
#include "components/mus/public/interfaces/command_buffer.mojom.h"
#include "mojo/public/cpp/system/core.h"

namespace shell {
class Connector;
}

namespace mus {

class GLES2Context;

class ContextProvider : public cc::ContextProvider {
 public:
  explicit ContextProvider(shell::Connector* connector);

  // cc::ContextProvider implementation.
  bool BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  void InvalidateGrContext(uint32_t state) override;
  base::Lock* GetLock() override;
  gpu::Capabilities ContextCapabilities() override;
  void DeleteCachedResources() override {}
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override {}

 protected:
  friend class base::RefCountedThreadSafe<ContextProvider>;
  ~ContextProvider() override;

 private:
  std::unique_ptr<shell::Connector> connector_;
  std::unique_ptr<GLES2Context> context_;

  DISALLOW_COPY_AND_ASSIGN(ContextProvider);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_CONTEXT_PROVIDER_H_
