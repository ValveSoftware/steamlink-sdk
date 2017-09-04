// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_INPUT_BLIMP_INPUT_MANAGER_H_
#define BLIMP_CLIENT_CORE_INPUT_BLIMP_INPUT_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"
#include "ui/events/gesture_detection/motion_event.h"

namespace cc {
class InputHandler;
}  // namespace cc

namespace blimp {
namespace client {
class BlimpInputHandlerWrapper;

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
// The class is created on and lives on the main thread.
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

  void OnInputHandlerWrapperInitialized(
      base::WeakPtr<BlimpInputHandlerWrapper> input_handler_wrapper_weak_ptr);

 private:
  BlimpInputManager(
      BlimpInputManagerClient* client,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      const base::WeakPtr<cc::InputHandler>& input_handler);

  static void CreateInputHandlerWrapperOnCompositorThread(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      const base::WeakPtr<BlimpInputManager>& input_manager_weak_ptr,
      const base::WeakPtr<cc::InputHandler>& input_handler);

  // ui::GestureProviderClient implementation.
  void OnGestureEvent(const ui::GestureEventData& gesture) override;

  BlimpInputManagerClient* client_;

  ui::FilteredGestureProvider gesture_provider_;

  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  std::unique_ptr<BlimpInputHandlerWrapper> input_handler_wrapper_;

  // WeakPtr used to post tasks to the wrapper on the compositor thread.
  base::WeakPtr<BlimpInputHandlerWrapper> input_handler_wrapper_weak_ptr_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<BlimpInputManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpInputManager);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_INPUT_BLIMP_INPUT_MANAGER_H_
