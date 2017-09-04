// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_WINRT_H_
#define MEDIA_MIDI_MIDI_MANAGER_WINRT_H_

#include <memory>

#include "base/strings/string16.h"
#include "base/threading/thread.h"
#include "media/midi/midi_manager.h"

namespace base {
class ThreadChecker;
}

namespace midi {

class MidiScheduler;

class MIDI_EXPORT MidiManagerWinrt final : public MidiManager {
 public:
  MidiManagerWinrt();
  ~MidiManagerWinrt() override;

  // MidiManager overrides:
  void StartInitialization() final;
  void Finalize() final;
  void DispatchSendMidiData(MidiManagerClient* client,
                            uint32_t port_index,
                            const std::vector<uint8_t>& data,
                            double timestamp) final;

 private:
  // Callbacks on the COM thread.
  void InitializeOnComThread();
  void FinalizeOnComThread();
  void SendOnComThread(uint32_t port_index, const std::vector<uint8_t>& data);

  // Callback from MidiPortManager::OnEnumerationComplete on the COM thread.
  // Calls CompleteInitialization() when both MidiPortManagers are ready.
  void OnPortManagerReady();

  // Subclasses that access private/protected members of MidiManager.
  template <typename InterfaceType,
            typename RuntimeType,
            typename StaticsInterfaceType,
            base::char16 const* runtime_class_id>
  class MidiPortManager;
  class MidiInPortManager;
  class MidiOutPortManager;

  // COM-initialized thread for calling WinRT methods.
  base::Thread com_thread_;

  // Lock to ensure all smart pointers initialized in InitializeOnComThread()
  // and destroyed in FinalizeOnComThread() will not be accidentally destructed
  // twice in the destructor.
  base::Lock lazy_init_member_lock_;

  // Should be instantiated on COM thread.
  std::unique_ptr<base::ThreadChecker>
      com_thread_checker_;  // GUARDED_BY(lazy_init_member_lock_)

  // All operations to MidiPortManager should be done on COM thread.
  std::unique_ptr<MidiInPortManager>
      port_manager_in_;  // GUARDED_BY(lazy_init_member_lock_)
  std::unique_ptr<MidiOutPortManager>
      port_manager_out_;  // GUARDED_BY(lazy_init_member_lock_)

  // |scheduler_| is used by DispatchSendMidiData on Chrome_IOThread, should
  // acquire lock before use.
  std::unique_ptr<MidiScheduler>
      scheduler_;  // GUARDED_BY(lazy_init_member_lock_)

  // Incremented when a MidiPortManager is ready.
  uint8_t port_manager_ready_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MidiManagerWinrt);
};

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_MANAGER_WINRT_H_
