// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorTest_h
#define CompositorTest_h

#include "base/memory/ref_counted.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CompositorTest : public testing::Test {
    WTF_MAKE_NONCOPYABLE(CompositorTest);

public:
    CompositorTest();
    virtual ~CompositorTest();

protected:
    // Mock task runner is initialized here because tests create
    // WebLayerTreeViewImplForTesting which needs the current task runner handle.
    scoped_refptr<base::TestMockTimeTaskRunner> m_runner;
    base::ThreadTaskRunnerHandle m_runnerHandle;
};

} // namespace blink

#endif // CompositorTest_h
