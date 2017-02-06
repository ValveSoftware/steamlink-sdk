// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_AVSETTINGS_H_
#define CHROMECAST_PUBLIC_AVSETTINGS_H_

#include <stdint.h>

#include "output_restrictions.h"
#include "task_runner.h"

namespace chromecast {

// Pure abstract interface to get and set media-related information. Each
// platform must provide its own implementation.
// All functions except constructor and destructor are called in one thread.
// All delegate functions can be called by platform implementation on any
// threads, for example, created by platform implementation internally.
class AvSettings {
 public:
  // Defines whether or not the cast receiver is the current active source of
  // the screen. If the device is connected to HDMI sinks, it may be unknown.
  enum ActiveState {
    UNKNOWN,
    STANDBY,   // Screen is off
    INACTIVE,  // Screen is on, but cast receiver is not active
    ACTIVE,    // Screen is on and cast receiver is active
  };

  // Audio codec supported by the device (or HDMI sink).
  enum AudioCodec {
    AC3 = 1 << 0,
    DTS = 1 << 1,
    DTS_HD = 1 << 2,
    EAC3 = 1 << 3,
    LPCM = 1 << 4,
  };

  // Defines the type of audio volume control of the device.
  enum AudioVolumeControlType {
    UNKNOWN_VOLUME,

    // MASTER_VOLUME: Devices of CEC audio controls is a master volume system,
    // i.e the system volume is changed, but not attenuated,
    // e.g. normal TVs, audio devices.
    MASTER_VOLUME,

    // ATTENUATION_VOLUME: Devices which do not do CEC audio controls,
    // e.g. Chromecast.
    ATTENUATION_VOLUME,

    // FIXED_VOLUME: Devices which have fixed volume, e.g. Nexus Player.
    FIXED_VOLUME,
  };

  // Defines the status of platform wake-on-cast feature.
  enum WakeOnCastStatus {
    WAKE_ON_CAST_UNKNOWN,  // Should only been used very rarely when platform
                           // has error to get the status.
    WAKE_ON_CAST_NOT_SUPPORTED,  // Platform doesn't support wake-on-cast.
    WAKE_ON_CAST_DISABLED,
    WAKE_ON_CAST_ENABLED,
  };

  enum Event {
    // This event shall be fired whenever the active state is changed including
    // when the screen turned on, when the cast receiver (or the device where
    // cast receiver is running on) became the active input source, or after a
    // call to TurnActive() or TurnStandby().
    // WakeSystem() may change the active state depending on implementation.
    // On this event, GetActiveState() will be called on the thread where
    // Initialize() was called.
    ACTIVE_STATE_CHANGED = 0,

    // This event shall be fired whenever the system volume level or muted state
    // are changed including when user changed volume via a remote controller,
    // or after a call to SetAudioVolume() or SetAudioMuted().
    // On this event, GetAudioVolume() and IsAudioMuted() will be called on
    // the thread where Initialize() was called.
    AUDIO_VOLUME_CHANGED = 1,

    // This event shall be fired whenever the audio codecs supported by the
    // device (or HDMI sinks connected to the device) are changed.
    // On this event, GetAudioCodecsSupported() and GetMaxAudioChannels() will
    // be called on the thread where Initialize() was called.
    AUDIO_CODECS_SUPPORTED_CHANGED = 2,

    // This event shall be fired whenever the screen information of the device
    // (or HDMI sinks connected to the device) are changed including screen
    // resolution.
    // On this event, GetScreenResolution() will be called on the thread where
    // Initialize() was called.
    SCREEN_INFO_CHANGED = 3,

    // This event should be fired whenever the active output restrictions on the
    // device outputs change. On this event, GetOutputRestrictions() will be
    // called on the thread where Initialize() was called.
    OUTPUT_RESTRICTIONS_CHANGED = 4,

    // This event shall be fired whenever the type of volume control provided
    // by the device is changed, for e.g., when the device is connected or
    // disconnected to HDMI sinks
    AUDIO_VOLUME_CONTROL_TYPE_CHANGED = 5,

    // This event shall be fired whenever wake-on-cast status is changed by
    // platform.
    WAKE_ON_CAST_CHANGED = 6,

    // This event shall be fired whenever the volume step interval provided
    // by the device is changed, for e.g. when connecting to an AVR setup
    // where step interval should be 1%.
    AUDIO_VOLUME_STEP_INTERVAL_CHANGED = 7,

    // This event should be fired when the device is connected to HDMI sinks.
    HDMI_CONNECTED = 100,

    // This event should be fired when the device is disconnected to HDMI sinks.
    HDMI_DISCONNECTED = 101,

    // This event should be fired when an HDMI error occurs.
    HDMI_ERROR = 102,
  };

  // Delegate to inform the caller events. As a subclass of TaskRunner,
  // AvSettings implementation can post tasks to the thread where Initialize()
  // was called.
  class Delegate : public TaskRunner {
   public:
    // This may be invoked to posts a task to the thread where Initialize() was
    // called.
    bool PostTask(Task* task, uint64_t delay_ms) override = 0;

    // This must be invoked to fire an event when one of the conditions
    // described above (Event) happens.
    virtual void OnMediaEvent(Event event) = 0;

    // This should be invoked when a key is pressed.
    // |key_code| is a CEC code defined in User Control Codes table of the CEC
    // specification (CEC Table 30 in the HDMI 1.4a specification).
    virtual void OnKeyPressed(int key_code) = 0;

   protected:
    ~Delegate() override {}
  };

  virtual ~AvSettings() {}

  // Initializes avsettings and starts delivering events to |delegate|.
  // |delegate| must not be null.
  virtual void Initialize(Delegate* delegate) = 0;

  // Finalizes avsettings. It must assume |delegate| passed to Initialize() is
  // invalid after this call and stop delivering events.
  virtual void Finalize() = 0;

  // Returns current active state.
  virtual ActiveState GetActiveState() = 0;

  // Turns the screen on and sets the active input to the cast receiver.
  // If successful, it must return true and fire ACTIVE_STATE_CHANGED.
  virtual bool TurnActive() = 0;

  // Turns the screen off (or stand-by). If the device is connecting to HDMI
  // sinks, broadcasts a CEC standby message on the HDMI control bus to put all
  // sink devices (TV, AVR) into a standby state.
  // If successful, it must return true and fire ACTIVE_STATE_CHANGED.
  virtual bool TurnStandby() = 0;

  // Requests the system where cast receiver is running on to be kept awake for
  // |time_ms|. If the system is already being kept awake, the period should be
  // extended from |time_ms| in the future.
  // It will be called when cast senders discover the cast receiver while the
  // system is in a stand-by mode (or a deeper sleeping/dormant mode depending
  // on the system). To respond to cast senders' requests, cast receiver needs
  // the system awake for given amount of time. The system should not turn
  // screen on.
  // Returns true if successful.
  virtual bool KeepSystemAwake(int time_ms) = 0;

  // Returns the type of volume control, i.e. MASTER_VOLUME, FIXED_VOLUME or
  // ATTENUATION_VOLUME. For example, normal TVs, devices of CEC audio
  // controls, and audio devices are master volume systems. The counter
  // examples are Chromecast (which doesn't do CEC audio controls) and
  // Nexus Player which is fixed volume.
  virtual AudioVolumeControlType GetAudioVolumeControlType() = 0;

  // Retrieves the volume step interval in range [0.0, 1.0] that specifies how
  // much volume to change per step, e.g. 0.05 = 5%. Returns true if a valid
  // interval is specified by platform; returns false if interval should defer
  // to default values.
  //
  // Current default volume step intervals per control type are as follows:
  //  - MASTER_VOLUME: 0.05 (5%)
  //  - ATTENUATION_VOLUME: 0.02 (2%)
  //  - FIXED_VOLUME: 0.01 (1%)
  //  - UNKNOWN_VOLUME: 0.01 (1%)
  virtual bool GetAudioVolumeStepInterval(float* step_inteval) = 0;

  // Returns the current volume level, which must be from 0.0 (inclusive) to
  // 1.0 (inclusive).
  virtual float GetAudioVolume() = 0;

  // Sets new volume level of the device (or HDMI sinks). |level| is from 0.0
  // (inclusive) to 1.0 (inclusive).
  // If successful and the level has changed, it must return true and fire
  // AUDIO_VOLUME_CHANGED.
  virtual bool SetAudioVolume(float level) = 0;

  // Whether or not the device (or HDMI sinks) is muted.
  virtual bool IsAudioMuted() = 0;

  // Sets the device (or HDMI sinks) muted.
  // If successful and the muted state has changed, it must return true and fire
  // AUDIO_VOLUME_CHANGED.
  virtual bool SetAudioMuted(bool muted) = 0;

  // Gets audio codecs supported by the device (or HDMI sinks).
  // The result is an integer of OR'ed AudioCodec values.
  virtual int GetAudioCodecsSupported() = 0;

  // Gets maximum number of channels for given audio codec, |codec|.
  virtual int GetMaxAudioChannels(AudioCodec codec) = 0;

  // Retrieves the resolution of screen of the device (or HDMI sinks).
  // Returns true if it gets resolution successfully.
  virtual bool GetScreenResolution(int* width, int* height) = 0;

  // If supported, retrieves the restrictions active on the device outputs (as
  // specified by the PlayReady CDM; see output_restrictions.h). If reporting
  // output restrictions is unsupported, should return false.
  virtual bool GetOutputRestrictions(
      OutputRestrictions* output_restrictions) = 0;

  // If supported, sets which output restrictions should be active on the device
  // (as specified by the PlayReady CDM; see output_restrictions.h). The device
  // should try to apply these restrictions and fire OUTPUT_RESTRICTIONS_CHANGED
  // if they result in a change of active restrictions.
  virtual void ApplyOutputRestrictions(
      const OutputRestrictions& restrictions) = 0;

  // Returns current Wake-On-Cast status from platform.
  virtual WakeOnCastStatus GetWakeOnCastStatus() = 0;

  // Enables/Disables Wake-On-Cast status.
  // Returns false if failed or not supported.
  virtual bool EnableWakeOnCast(bool enabled) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_AVSETTINGS_H_
