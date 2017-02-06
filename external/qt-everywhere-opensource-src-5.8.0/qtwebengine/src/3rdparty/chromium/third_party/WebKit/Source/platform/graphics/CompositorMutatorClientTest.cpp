// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorMutatorClient.h"

#include "base/callback.h"
#include "platform/graphics/CompositorMutation.h"
#include "platform/graphics/CompositorMutationsTarget.h"
#include "platform/graphics/CompositorMutator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

using ::testing::_;

namespace blink {
namespace {

class StubCompositorMutator : public CompositorMutator {
public:
    StubCompositorMutator() {}

    bool mutate(double monotonicTimeNow,
        CompositorMutableStateProvider* stateProvider) override
    {
        return false;
    }
};

class MockCompositoMutationsTarget : public CompositorMutationsTarget {
public:
    MOCK_METHOD1(applyMutations, void(CompositorMutations*));
};

TEST(CompositorMutatorClient, CallbackForNonNullMutationsShouldApply)
{
    MockCompositoMutationsTarget target;

    CompositorMutatorClient client(new StubCompositorMutator, &target);
    std::unique_ptr<CompositorMutations> mutations = wrapUnique(new CompositorMutations());
    client.setMutationsForTesting(std::move(mutations));

    EXPECT_CALL(target, applyMutations(_));
    client.TakeMutations().Run();
}

TEST(CompositorMutatorClient, CallbackForNullMutationsShouldBeNoop)
{
    MockCompositoMutationsTarget target;
    CompositorMutatorClient client(new StubCompositorMutator, &target);

    EXPECT_CALL(target, applyMutations(_)).Times(0);
    EXPECT_TRUE(client.TakeMutations().is_null());
}

} // namespace
} // namespace blink
