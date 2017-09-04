// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blimp/synced_property_remote.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(SyncedPropertyRemoteTest, BasicDeltaTracking) {
  SyncedPropertyRemote<ScaleGroup> synced_float;

  // Set up the initial value.
  float initial_state = 0.5f;
  synced_float.PushFromEngineMainThread(initial_state);
  EXPECT_EQ(synced_float.EngineMain(), initial_state);

  // Apply a delta from the impl thread.
  float delta_from_impl = 0.2f;
  synced_float.UpdateDeltaFromImplThread(initial_state * delta_from_impl);
  EXPECT_EQ(synced_float.DeltaNotAppliedOnEngine(), delta_from_impl);
  EXPECT_EQ(synced_float.EngineMain(), initial_state);

  // Pull a state update for the engine.
  float delta_to_send = synced_float.PullDeltaForEngineUpdate();
  EXPECT_EQ(delta_to_send, delta_from_impl);

  // The initial delta should still be reported as unapplied.
  EXPECT_EQ(synced_float.DeltaNotAppliedOnEngine(), delta_from_impl);

  // Now send some more delta from impl. It should be included in the unapplied
  // delta.
  float second_delta_from_impl = 0.3f;
  synced_float.UpdateDeltaFromImplThread(synced_float.EngineMain() *
                                         second_delta_from_impl);
  float third_delta_from_impl = 0.4f;
  synced_float.UpdateDeltaFromImplThread(synced_float.EngineMain() *
                                         third_delta_from_impl);

  float expected_total_delta =
      delta_from_impl * second_delta_from_impl * third_delta_from_impl;
  EXPECT_EQ(synced_float.DeltaNotAppliedOnEngine(), expected_total_delta);

  // Inform that the delta was applied on the engine and make sure it is
  // pre-emptively applied on the client.
  synced_float.DidApplySentDeltaOnEngine();
  EXPECT_EQ(synced_float.EngineMain(), initial_state * delta_from_impl);

  // Now pull another delta, it should be now the accumulated delta.
  EXPECT_EQ(synced_float.PullDeltaForEngineUpdate(),
            second_delta_from_impl * third_delta_from_impl);
}

}  // namespace
}  // namespace cc
