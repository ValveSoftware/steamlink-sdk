// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_PROVIDER_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_PROVIDER_H_

#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "base/system_monitor/system_monitor.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace base {
class MessageLoopProxy;
class Thread;
}

namespace content {

class GamepadDataFetcher;
struct GamepadHardwareBuffer;

class CONTENT_EXPORT GamepadProvider :
  public base::SystemMonitor::DevicesChangedObserver {
 public:
  GamepadProvider();

  // Manually specifies the data fetcher. Used for testing.
  explicit GamepadProvider(scoped_ptr<GamepadDataFetcher> fetcher);

  virtual ~GamepadProvider();

  // Returns the shared memory handle of the gamepad data duplicated into the
  // given process.
  base::SharedMemoryHandle GetSharedMemoryHandleForProcess(
      base::ProcessHandle renderer_process);

  void GetCurrentGamepadData(blink::WebGamepads* data);

  // Pause and resume the background polling thread. Can be called from any
  // thread.
  void Pause();
  void Resume();

  // Registers the given closure for calling when the user has interacted with
  // the device. This callback will only be issued once.
  void RegisterForUserGesture(const base::Closure& closure);

  // base::SystemMonitor::DevicesChangedObserver implementation.
  virtual void OnDevicesChanged(base::SystemMonitor::DeviceType type) OVERRIDE;

 private:
  void Initialize(scoped_ptr<GamepadDataFetcher> fetcher);

  // Method for setting up the platform-specific data fetcher. Takes ownership
  // of |fetcher|.
  void DoInitializePollingThread(scoped_ptr<GamepadDataFetcher> fetcher);

  // Method for sending pause hints to the low-level data fetcher. Runs on
  // polling_thread_.
  void SendPauseHint(bool paused);

  // Method for polling a GamepadDataFetcher. Runs on the polling_thread_.
  void DoPoll();
  void ScheduleDoPoll();

  void OnGamepadConnectionChange(bool connected,
                                 int index,
                                 const blink::WebGamepad& pad);
  void DispatchGamepadConnectionChange(bool connected,
                                       int index,
                                       const blink::WebGamepad& pad);

  GamepadHardwareBuffer* SharedMemoryAsHardwareBuffer();

  // Checks the gamepad state to see if the user has interacted with it.
  void CheckForUserGesture();

  enum { kDesiredSamplingIntervalMs = 16 };

  // Keeps track of when the background thread is paused. Access to is_paused_
  // must be guarded by is_paused_lock_.
  base::Lock is_paused_lock_;
  bool is_paused_;

  // Keep track of when a polling task is schedlued, so as to prevent us from
  // accidentally scheduling more than one at any time, when rapidly toggling
  // |is_paused_|.
  bool have_scheduled_do_poll_;

  // Lists all observers registered for user gestures, and the thread which
  // to issue the callbacks on. Since we always issue the callback on the
  // thread which the registration happened, and this class lives on the I/O
  // thread, the message loop proxies will normally just be the I/O thread.
  // However, this will be the main thread for unit testing.
  base::Lock user_gesture_lock_;
  struct ClosureAndThread {
    ClosureAndThread(const base::Closure& c,
                     const scoped_refptr<base::MessageLoopProxy>& m);
    ~ClosureAndThread();

    base::Closure closure;
    scoped_refptr<base::MessageLoopProxy> message_loop;
  };
  typedef std::vector<ClosureAndThread> UserGestureObserverVector;
  UserGestureObserverVector user_gesture_observers_;

  // Updated based on notification from SystemMonitor when the system devices
  // have been updated, and this notification is passed on to the data fetcher
  // to enable it to avoid redundant (and possibly expensive) is-connected
  // tests. Access to devices_changed_ must be guarded by
  // devices_changed_lock_.
  base::Lock devices_changed_lock_;
  bool devices_changed_;

  bool ever_had_user_gesture_;

  class PadState {
   public:
    PadState() {
      SetDisconnected();
    }

    bool Match(const blink::WebGamepad& pad) const;
    void SetPad(const blink::WebGamepad& pad);
    void SetDisconnected();
    void AsWebGamepad(blink::WebGamepad* pad);

    bool connected() const { return connected_; }

   private:
    bool connected_;
    unsigned axes_length_;
    unsigned buttons_length_;
    blink::WebUChar id_[blink::WebGamepad::idLengthCap];
    blink::WebUChar mapping_[blink::WebGamepad::mappingLengthCap];
  };

  // Used to detect connections and disconnections.
  scoped_ptr<PadState[]> pad_states_;

  // Only used on the polling thread.
  scoped_ptr<GamepadDataFetcher> data_fetcher_;

  base::Lock shared_memory_lock_;
  base::SharedMemory gamepad_shared_memory_;

  // Polling is done on this background thread.
  scoped_ptr<base::Thread> polling_thread_;

  static GamepadProvider* instance_;

  DISALLOW_COPY_AND_ASSIGN(GamepadProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_PROVIDER_H_
