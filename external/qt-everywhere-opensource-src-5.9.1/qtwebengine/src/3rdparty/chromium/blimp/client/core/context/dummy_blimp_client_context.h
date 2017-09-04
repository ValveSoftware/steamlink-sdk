// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTEXT_DUMMY_BLIMP_CLIENT_CONTEXT_H_
#define BLIMP_CLIENT_CORE_CONTEXT_DUMMY_BLIMP_CLIENT_CONTEXT_H_

#include "base/macros.h"
#include "blimp/client/public/blimp_client_context.h"
#include "blimp/client/public/contents/blimp_contents.h"

namespace blimp {
namespace client {

// A dummy implementation of the BlimpClientContext which is in use by
// embedders that do not set the GN argument |enable_blimp_client| to true.
// It exists so that it is possible for an embedder to still have code depending
// on the public APIs of blimp, without in fact linking in all the core code.
class DummyBlimpClientContext : public BlimpClientContext {
 public:
  DummyBlimpClientContext();
  ~DummyBlimpClientContext() override;

  // BlimpClientContext implementation.
  void SetDelegate(BlimpClientContextDelegate* delegate) override;
  std::unique_ptr<BlimpContents> CreateBlimpContents(
      gfx::NativeWindow window) override;
  void Connect() override;
  void ConnectWithAssignment(const Assignment& assignment) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyBlimpClientContext);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTEXT_DUMMY_BLIMP_CLIENT_CONTEXT_H_
