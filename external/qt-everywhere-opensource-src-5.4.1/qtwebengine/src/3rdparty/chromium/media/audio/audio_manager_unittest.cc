// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/fake_audio_log_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_ALSA)
#include "media/audio/alsa/audio_manager_alsa.h"
#endif  // defined(USE_ALSA)

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/wavein_input_win.h"
#endif

#if defined(USE_PULSEAUDIO)
#include "media/audio/pulse/audio_manager_pulse.h"
#endif  // defined(USE_PULSEAUDIO)

namespace media {

// Test fixture which allows us to override the default enumeration API on
// Windows.
class AudioManagerTest : public ::testing::Test {
 protected:
  AudioManagerTest()
      : audio_manager_(AudioManager::CreateForTesting())
#if defined(OS_WIN)
      , com_init_(base::win::ScopedCOMInitializer::kMTA)
#endif
  {
    // Wait for audio thread initialization to complete.  Otherwise the
    // enumeration type may not have been set yet.
    base::WaitableEvent event(false, false);
    audio_manager_->GetTaskRunner()->PostTask(FROM_HERE, base::Bind(
        &base::WaitableEvent::Signal, base::Unretained(&event)));
    event.Wait();
  }

  AudioManager* audio_manager() { return audio_manager_.get(); };

#if defined(OS_WIN)
  bool SetMMDeviceEnumeration() {
    AudioManagerWin* amw = static_cast<AudioManagerWin*>(audio_manager_.get());
    // Windows Wave is used as default if Windows XP was detected =>
    // return false since MMDevice is not supported on XP.
    if (amw->enumeration_type() == AudioManagerWin::kWaveEnumeration)
      return false;

    amw->SetEnumerationType(AudioManagerWin::kMMDeviceEnumeration);
    return true;
  }

  void SetWaveEnumeration() {
    AudioManagerWin* amw = static_cast<AudioManagerWin*>(audio_manager_.get());
    amw->SetEnumerationType(AudioManagerWin::kWaveEnumeration);
  }

  std::string GetDeviceIdFromPCMWaveInAudioInputStream(
      const std::string& device_id) {
    AudioManagerWin* amw = static_cast<AudioManagerWin*>(audio_manager_.get());
    AudioParameters parameters(
        AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_STEREO,
        AudioParameters::kAudioCDSampleRate, 16,
        1024);
    scoped_ptr<PCMWaveInAudioInputStream> stream(
        static_cast<PCMWaveInAudioInputStream*>(
            amw->CreatePCMWaveInAudioInputStream(parameters, device_id)));
    return stream.get() ? stream->device_id_ : std::string();
  }
#endif

  // Helper method which verifies that the device list starts with a valid
  // default record followed by non-default device names.
  static void CheckDeviceNames(const AudioDeviceNames& device_names) {
    VLOG(2) << "Got " << device_names.size() << " audio devices.";
    if (!device_names.empty()) {
      AudioDeviceNames::const_iterator it = device_names.begin();

      // The first device in the list should always be the default device.
      EXPECT_EQ(std::string(AudioManagerBase::kDefaultDeviceName),
                it->device_name);
      EXPECT_EQ(std::string(AudioManagerBase::kDefaultDeviceId), it->unique_id);
      ++it;

      // Other devices should have non-empty name and id and should not contain
      // default name or id.
      while (it != device_names.end()) {
        EXPECT_FALSE(it->device_name.empty());
        EXPECT_FALSE(it->unique_id.empty());
        VLOG(2) << "Device ID(" << it->unique_id
                << "), label: " << it->device_name;
        EXPECT_NE(std::string(AudioManagerBase::kDefaultDeviceName),
                  it->device_name);
        EXPECT_NE(std::string(AudioManagerBase::kDefaultDeviceId),
                  it->unique_id);
        ++it;
      }
    } else {
      // Log a warning so we can see the status on the build bots.  No need to
      // break the test though since this does successfully test the code and
      // some failure cases.
      LOG(WARNING) << "No input devices detected";
    }
  }

  bool CanRunInputTest() {
    return audio_manager_->HasAudioInputDevices();
  }

  bool CanRunOutputTest() {
    return audio_manager_->HasAudioOutputDevices();
  }

#if defined(USE_ALSA) || defined(USE_PULSEAUDIO)
  template <class T>
  void CreateAudioManagerForTesting() {
    // Only one AudioManager may exist at a time, so destroy the one we're
    // currently holding before creating a new one.
    audio_manager_.reset();
    audio_manager_.reset(T::Create(&fake_audio_log_factory_));
  }
#endif

  // Synchronously runs the provided callback/closure on the audio thread.
  void RunOnAudioThread(const base::Closure& closure) {
    if (!audio_manager()->GetTaskRunner()->BelongsToCurrentThread()) {
      base::WaitableEvent event(false, false);
      audio_manager_->GetTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(&AudioManagerTest::RunOnAudioThreadImpl,
                     base::Unretained(this),
                     closure,
                     &event));
      event.Wait();
    } else {
      closure.Run();
    }
  }

  void RunOnAudioThreadImpl(const base::Closure& closure,
                            base::WaitableEvent* event) {
    DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
    closure.Run();
    event->Signal();
  }

  FakeAudioLogFactory fake_audio_log_factory_;
  scoped_ptr<AudioManager> audio_manager_;

#if defined(OS_WIN)
  // The MMDevice API requires COM to be initialized on the current thread.
  base::win::ScopedCOMInitializer com_init_;
#endif
};

// Test that devices can be enumerated.
TEST_F(AudioManagerTest, EnumerateInputDevices) {
  if (!CanRunInputTest())
    return;

  AudioDeviceNames device_names;
  RunOnAudioThread(
      base::Bind(&AudioManager::GetAudioInputDeviceNames,
                 base::Unretained(audio_manager()),
                 &device_names));
  CheckDeviceNames(device_names);
}

// Test that devices can be enumerated.
TEST_F(AudioManagerTest, EnumerateOutputDevices) {
  if (!CanRunOutputTest())
    return;

  AudioDeviceNames device_names;
  RunOnAudioThread(
      base::Bind(&AudioManager::GetAudioOutputDeviceNames,
                 base::Unretained(audio_manager()),
                 &device_names));
  CheckDeviceNames(device_names);
}

// Run additional tests for Windows since enumeration can be done using
// two different APIs. MMDevice is default for Vista and higher and Wave
// is default for XP and lower.
#if defined(OS_WIN)

// Override default enumeration API and force usage of Windows MMDevice.
// This test will only run on Windows Vista and higher.
TEST_F(AudioManagerTest, EnumerateInputDevicesWinMMDevice) {
  if (!CanRunInputTest())
    return;

  AudioDeviceNames device_names;
  if (!SetMMDeviceEnumeration()) {
    // Usage of MMDevice will fail on XP and lower.
    LOG(WARNING) << "MM device enumeration is not supported.";
    return;
  }
  audio_manager_->GetAudioInputDeviceNames(&device_names);
  CheckDeviceNames(device_names);
}

TEST_F(AudioManagerTest, EnumerateOutputDevicesWinMMDevice) {
  if (!CanRunOutputTest())
    return;

  AudioDeviceNames device_names;
  if (!SetMMDeviceEnumeration()) {
    // Usage of MMDevice will fail on XP and lower.
    LOG(WARNING) << "MM device enumeration is not supported.";
    return;
  }
  audio_manager_->GetAudioOutputDeviceNames(&device_names);
  CheckDeviceNames(device_names);
}

// Override default enumeration API and force usage of Windows Wave.
// This test will run on Windows XP, Windows Vista and Windows 7.
TEST_F(AudioManagerTest, EnumerateInputDevicesWinWave) {
  if (!CanRunInputTest())
    return;

  AudioDeviceNames device_names;
  SetWaveEnumeration();
  audio_manager_->GetAudioInputDeviceNames(&device_names);
  CheckDeviceNames(device_names);
}

TEST_F(AudioManagerTest, EnumerateOutputDevicesWinWave) {
  if (!CanRunOutputTest())
    return;

  AudioDeviceNames device_names;
  SetWaveEnumeration();
  audio_manager_->GetAudioOutputDeviceNames(&device_names);
  CheckDeviceNames(device_names);
}

TEST_F(AudioManagerTest, WinXPDeviceIdUnchanged) {
  if (!CanRunInputTest())
    return;

  AudioDeviceNames xp_device_names;
  SetWaveEnumeration();
  audio_manager_->GetAudioInputDeviceNames(&xp_device_names);
  CheckDeviceNames(xp_device_names);

  // Device ID should remain unchanged, including the default device ID.
  for (AudioDeviceNames::iterator i = xp_device_names.begin();
       i != xp_device_names.end(); ++i) {
    EXPECT_EQ(i->unique_id,
              GetDeviceIdFromPCMWaveInAudioInputStream(i->unique_id));
  }
}

TEST_F(AudioManagerTest, ConvertToWinXPInputDeviceId) {
  if (!CanRunInputTest())
    return;

  if (!SetMMDeviceEnumeration()) {
    // Usage of MMDevice will fail on XP and lower.
    LOG(WARNING) << "MM device enumeration is not supported.";
    return;
  }

  AudioDeviceNames device_names;
  audio_manager_->GetAudioInputDeviceNames(&device_names);
  CheckDeviceNames(device_names);

  for (AudioDeviceNames::iterator i = device_names.begin();
       i != device_names.end(); ++i) {
    std::string converted_id =
        GetDeviceIdFromPCMWaveInAudioInputStream(i->unique_id);
    if (i == device_names.begin()) {
      // The first in the list is the default device ID, which should not be
      // changed when passed to PCMWaveInAudioInputStream.
      EXPECT_EQ(i->unique_id, converted_id);
    } else {
      // MMDevice-style device IDs should be converted to WaveIn-style device
      // IDs.
      EXPECT_NE(i->unique_id, converted_id);
    }
  }
}

#endif  // defined(OS_WIN)

#if defined(USE_PULSEAUDIO)
// On Linux, there are two implementations available and both can
// sometimes be tested on a single system. These tests specifically
// test Pulseaudio.

TEST_F(AudioManagerTest, EnumerateInputDevicesPulseaudio) {
  if (!CanRunInputTest())
    return;

  CreateAudioManagerForTesting<AudioManagerPulse>();
  if (audio_manager_.get()) {
    AudioDeviceNames device_names;
    audio_manager_->GetAudioInputDeviceNames(&device_names);
    CheckDeviceNames(device_names);
  } else {
    LOG(WARNING) << "No pulseaudio on this system.";
  }
}

TEST_F(AudioManagerTest, EnumerateOutputDevicesPulseaudio) {
  if (!CanRunOutputTest())
    return;

  CreateAudioManagerForTesting<AudioManagerPulse>();
  if (audio_manager_.get()) {
    AudioDeviceNames device_names;
    audio_manager_->GetAudioOutputDeviceNames(&device_names);
    CheckDeviceNames(device_names);
  } else {
    LOG(WARNING) << "No pulseaudio on this system.";
  }
}
#endif  // defined(USE_PULSEAUDIO)

#if defined(USE_ALSA)
// On Linux, there are two implementations available and both can
// sometimes be tested on a single system. These tests specifically
// test Alsa.

TEST_F(AudioManagerTest, EnumerateInputDevicesAlsa) {
  if (!CanRunInputTest())
    return;

  VLOG(2) << "Testing AudioManagerAlsa.";
  CreateAudioManagerForTesting<AudioManagerAlsa>();
  AudioDeviceNames device_names;
  audio_manager_->GetAudioInputDeviceNames(&device_names);
  CheckDeviceNames(device_names);
}

TEST_F(AudioManagerTest, EnumerateOutputDevicesAlsa) {
  if (!CanRunOutputTest())
    return;

  VLOG(2) << "Testing AudioManagerAlsa.";
  CreateAudioManagerForTesting<AudioManagerAlsa>();
  AudioDeviceNames device_names;
  audio_manager_->GetAudioOutputDeviceNames(&device_names);
  CheckDeviceNames(device_names);
}
#endif  // defined(USE_ALSA)

TEST_F(AudioManagerTest, GetDefaultOutputStreamParameters) {
#if defined(OS_WIN) || defined(OS_MACOSX)
  if (!CanRunInputTest())
    return;

  AudioParameters params = audio_manager_->GetDefaultOutputStreamParameters();
  EXPECT_TRUE(params.IsValid());
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
}

TEST_F(AudioManagerTest, GetAssociatedOutputDeviceID) {
#if defined(OS_WIN) || defined(OS_MACOSX)
  if (!CanRunInputTest() || !CanRunOutputTest())
    return;

  AudioDeviceNames device_names;
  audio_manager_->GetAudioInputDeviceNames(&device_names);
  bool found_an_associated_device = false;
  for (AudioDeviceNames::iterator it = device_names.begin();
       it != device_names.end();
       ++it) {
    EXPECT_FALSE(it->unique_id.empty());
    EXPECT_FALSE(it->device_name.empty());
    std::string output_device_id(
        audio_manager_->GetAssociatedOutputDeviceID(it->unique_id));
    if (!output_device_id.empty()) {
      VLOG(2) << it->unique_id << " matches with " << output_device_id;
      found_an_associated_device = true;
    }
  }

  EXPECT_TRUE(found_an_associated_device);
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
}

}  // namespace media
