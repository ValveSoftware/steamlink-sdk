// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_DEPENDENCIES_H_
#define BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_DEPENDENCIES_H_

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"

namespace base {
class SingleThreadTaskRunner;
class Thread;
}  // namespace base

namespace cc {
class ImageSerializationProcessor;
class LayerTreeSettings;
class TaskGraphRunner;
}

namespace blimp {
namespace client {

class CompositorDependencies;

// Holds all dependencies shared by BlimpCompositor instances.  This includes
// shared threads and embedder dependencies.  Access to this will be shared
// among all BlimpCompositors and should only be used on the main thread.
class BlimpCompositorDependencies {
 public:
  explicit BlimpCompositorDependencies(
      std::unique_ptr<CompositorDependencies> embedder_dependencies);
  virtual ~BlimpCompositorDependencies();

  // External compositor dependencies provided by the embedder.
  CompositorDependencies* GetEmbedderDependencies();

  // Threads shared by all BlimpCompositor instances.
  cc::TaskGraphRunner* GetTaskGraphRunner();
  scoped_refptr<base::SingleThreadTaskRunner> GetCompositorTaskRunner();

  cc::ImageSerializationProcessor* GetImageSerializationProcessor();

  // TODO(xingliu): Get from the embedder when crbug.com/577985.
  cc::LayerTreeSettings* GetLayerTreeSettings();

 private:
  std::unique_ptr<CompositorDependencies> embedder_dependencies_;

  // Shared threads/task runners that will be used by all BlimpCompositors.
  std::unique_ptr<base::Thread> compositor_impl_thread_;
  std::unique_ptr<cc::TaskGraphRunner> task_graph_runner_;

  std::unique_ptr<cc::LayerTreeSettings> settings_;

  DISALLOW_COPY_AND_ASSIGN(BlimpCompositorDependencies);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_DEPENDENCIES_H_
