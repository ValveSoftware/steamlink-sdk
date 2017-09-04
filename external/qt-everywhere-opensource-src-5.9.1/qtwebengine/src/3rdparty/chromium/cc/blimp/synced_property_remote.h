// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_SYNCED_PROPERTY_REMOTE_H_
#define CC_BLIMP_SYNCED_PROPERTY_REMOTE_H_

#include "cc/base/synced_property.h"

namespace cc {

// This class is heavily inspired from SyncedProperty and is the equivalent of
// SyncedProperty for synchronizing state between the engine and client in
// remote mode. There are effectively 2 trees on the main thread, the LayerTree
// on the main thread on the engine, and the tree on the main thread on the
// client.
//
// The client main thread tree receives deltas from the impl thread, but must
// synchronize them with the engine main thread before applying them onto the
// the main thread associated state. At the same time there are different sets
// of deltas, the value that was received from the impl thread but has not yet
// been reported to the engine main thread, and the value that has been sent
// to the engine main thread, but a state update with the application of these
// deltas has not been received from the engine. The purpose of this class is to
// track these deltas for deciding what needs to be reported to the main thread
// on the engine, and providing the impl thread with the delta that it should
// apply to its associated state since it could not be reflected in the local
// BeginMainFrame on the client main thread.
//
// Instances of this class are held on the client main thread and have a 1:1
// mapping with their corresponding SyncedProperty instances.

// Note: This class currently supports only one set of deltas to be in flight to
// the engine.
template <typename T>
class SyncedPropertyRemote {
 public:
  SyncedPropertyRemote() = default;

  SyncedPropertyRemote(SyncedPropertyRemote&& other) = default;

  SyncedPropertyRemote& operator=(SyncedPropertyRemote&& other) = default;

  // Push the main thread state from the engine onto the client main thread
  // associated state.
  void PushFromEngineMainThread(
      typename T::ValueType engine_main_thread_value) {
    engine_main_base_ = T(engine_main_thread_value);
  }

  // Called when an update for changes made on the impl thread was received on
  // the client main thread.
  // |main_thread_value| holds the updated value on the client main thread.
  void UpdateDeltaFromImplThread(typename T::ValueType main_thread_value) {
    T delta_from_impl_thread =
        T(main_thread_value).InverseCombine(engine_main_base_);
    unsent_delta_from_impl_thread_ =
        unsent_delta_from_impl_thread_.Combine(delta_from_impl_thread);
  }

  // Pull deltas for changes tracked on the client main thread to be sent to the
  // engine main thread.
  // Each call to this must be followed with a call to DidApplySentDeltaOnEngine
  // before another set of deltas can be pulled.
  typename T::ValueType PullDeltaForEngineUpdate() {
    DCHECK(sent_but_unapplied_delta_from_impl_thread_.get() ==
           T::Identity().get());

    T delta_to_send = unsent_delta_from_impl_thread_;
    sent_but_unapplied_delta_from_impl_thread_ = delta_to_send;
    unsent_delta_from_impl_thread_ = T::Identity();
    return delta_to_send.get();
  }

  // Called when deltas sent to the engine were applied to the main thread state
  // on the engine.
  void DidApplySentDeltaOnEngine() {
    // Pre-emptively apply the deltas that were sent. This is necessary since
    // the engine may not send a frame update if the deltas were simply
    // reflected back by the engine main thread (equivalent to
    // BeginMainFrameAborted between the main and impl thread).
    // If the engine did send a frame, we expect the frame to come with the ack
    // for the deltas, so the value will be over-written with the engine update
    // before being pushed to the impl thread.
    engine_main_base_ =
        engine_main_base_.Combine(sent_but_unapplied_delta_from_impl_thread_);
    sent_but_unapplied_delta_from_impl_thread_ = T::Identity();
  }

  // Returns the delta that was reported to the main thread on the client but
  // has not yet been applied to the main thread on the engine.
  typename T::ValueType DeltaNotAppliedOnEngine() {
    T total_delta_on_client = unsent_delta_from_impl_thread_.Combine(
        sent_but_unapplied_delta_from_impl_thread_);
    return total_delta_on_client.get();
  }

  typename T::ValueType EngineMain() { return engine_main_base_.get(); }

 private:
  // The delta that was applied to the state on the impl thread and received on
  // the main thread on the client during BeginMainFrame, but has not yet been
  // reported to the main thread on the engine.
  T unsent_delta_from_impl_thread_;

  // The delta that was applied to the state on the impl thread and has been
  // sent to the main thread on the engine, but has not yet been applied to the
  // main thread on the engine.
  T sent_but_unapplied_delta_from_impl_thread_;

  // The value as last received from the main thread on the engine. The value on
  // the main thread state on the client should always be set to this value,
  // outside of the interval when deltas from the impl thread are reported to
  // the main thread on the client.
  T engine_main_base_;

  DISALLOW_COPY_AND_ASSIGN(SyncedPropertyRemote);
};

}  // namespace cc

#endif  // CC_BLIMP_SYNCED_PROPERTY_REMOTE_H_
