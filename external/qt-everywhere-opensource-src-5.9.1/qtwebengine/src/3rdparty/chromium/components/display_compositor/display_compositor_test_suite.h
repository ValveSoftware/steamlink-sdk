// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DISPLAY_COMPOSITOR_DISPLAY_COMPOSITOR_TEST_SUITE_H_
#define COMPONENTS_DISPLAY_COMPOSITOR_DISPLAY_COMPOSITOR_TEST_SUITE_H_

#include <memory>

#include "base/macros.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_suite.h"

namespace base {
class MessageLoop;
}

namespace display_compositor {

class DisplayCompositorTestSuite : public base::TestSuite {
 public:
  DisplayCompositorTestSuite(int argc, char** argv);
  ~DisplayCompositorTestSuite() override;

 protected:
  // Overridden from base::TestSuite:
  void Initialize() override;
  void Shutdown() override;

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;

  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
  DISALLOW_COPY_AND_ASSIGN(DisplayCompositorTestSuite);
};

}  // namespace display_compositor

#endif  // COMPONENTS_DISPLAY_COMPOSITOR_DISPLAY_COMPOSITOR_TEST_SUITE_H_
