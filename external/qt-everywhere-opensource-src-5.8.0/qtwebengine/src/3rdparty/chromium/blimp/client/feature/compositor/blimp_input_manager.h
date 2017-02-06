// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_INPUT_MANAGER_H_
#define BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_INPUT_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "blimp/client/feature/compositor/blimp_input_handler_wrapper.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"
#include "ui/events/gesture_detection/motion_event.h"

namespace blimp {
namespace client {

class BlimpInputManagerClient {
 public:
  virtual void SendWebGestureEvent(
      const blink::WebGestureEvent& gesture_event) = 0;
};

// The BlimpInputManager handles input events for a specific web widget. The
// class processes ui::events to generate web gesture events which are forwarded
// to the compositor to be handled on the compositor thread. If the event can
// not be handled locally by the compositor, it is given to the
// BlimpInputManagerClient to be sent to the engine.
//
// The BlimpInputManager is created and destroyed on the main thread but can be
// called from the main or compositor thread. It is safe for the
// BlimpInputManager to be called on the compositor thread because:
// 1) The only compositor threaded callers of the BlimpInputManager are the
// BlimpInputManager itself.
// 2) BlimpInputManager blocks the main thread in its dtor to ensure that all
// tasks queued to call it on the compositor thread have been run before it is
// destroyed on the main thread.
//
// It is *important* to destroy the cc::InputHandler on the compositor thread
// before destroying the BlimpInputManager.

class BlimpInputManager : public ui::GestureProviderClient {
 public:
  static std::unique_ptr<BlimpInputManager> Create(
      BlimpInputManagerClient* client,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      const base::WeakPtr<cc::InputHandler>& input_handler);

  ~BlimpInputManager() override;

  // Called to process a ui::MotionEvent. Returns true if the event was
  // successfully processed.
  bool OnTouchEvent(const ui::MotionEvent& motion_event);

  // Called by the BlimpInputHandlerWrapper after an input event was handled on
  // the compositor thread.
  // |consumed| is false if the event was not handled by the compositor and
  // should be sent to the engine.
  void DidHandleWebGestureEvent(const blink::WebGestureEvent& gesture_event,
                                bool consumed);

 private:
  BlimpInputManager(
      BlimpInputManagerClient* client,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      const base::WeakPtr<cc::InputHandler>& input_handler);

  // ui::GestureProviderClient implementation.
  void OnGestureEvent(const ui::GestureEventData& gesture) override;

  // Called on the compositor thread.
  void CreateInputHandlerWrapperOnCompositorThread(
      base::WeakPtr<BlimpInputManager> input_manager_weak_ptr,
      const base::WeakPtr<cc::InputHandler>& input_handler);
  void HandleWebGestureEventOnCompositorThread(
      const blink::WebGestureEvent& gesture_event);
  void ShutdownOnCompositorThread(base::WaitableEvent* shutdown_event);

  bool IsMainThread() const;
  bool IsCompositorThread() const;

  BlimpInputManagerClient* client_;

  ui::FilteredGestureProvider gesture_provider_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  // Used for debug assertions to ensure that the main thread is blocked during
  // shutdown. Set in the destructor before the main thread is blocked and
  // read in ShutdownOnCompositorThread.
  bool main_thread_blocked_;

  std::unique_ptr<BlimpInputHandlerWrapper> input_handler_wrapper_;

  base::WeakPtrFactory<BlimpInputManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpInputManager);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_COMPOSITOR_BLIMP_INPUT_MANAGER_H_
