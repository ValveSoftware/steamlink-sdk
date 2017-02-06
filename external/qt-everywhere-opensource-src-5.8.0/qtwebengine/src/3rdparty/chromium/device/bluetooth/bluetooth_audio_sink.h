// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_AUDIO_SINK_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_AUDIO_SINK_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

// TODO(mcchou): Define a BluetoothAudioSink specific IOBuffer abstraction.

// BluetoothAudioSink represents a local A2DP audio sink where a remote device
// can stream audio data. Once a BluetoothAudioSink is successfully registered,
// user applications can obtain a pointer to a BluetoothAudioSink object via
// the interface provided by BluetoothAdapter. The validity of a
// BluetoothAudioSink depends on whether BluetoothAdapter is present and whether
// it is powered.
class DEVICE_BLUETOOTH_EXPORT BluetoothAudioSink
    : public base::RefCounted<BluetoothAudioSink> {
 public:
  // Possible values indicating the connection states between the
  // BluetoothAudioSink and the remote device.
  enum State {
    STATE_INVALID,  // BluetoothAdapter not presented or not powered.
    STATE_DISCONNECTED,  // Not connected.
    STATE_IDLE,  // Connected but not streaming.
    STATE_PENDING,  // Connected, streaming but not acquired.
    STATE_ACTIVE,  // Connected, streaming and acquired.
  };

  // Possible types of error raised by Audio Sink object.
  enum ErrorCode {
    ERROR_UNSUPPORTED_PLATFORM,  // A2DP sink not supported on current platform.
    ERROR_INVALID_ADAPTER,  // BluetoothAdapter not present/powered.
    ERROR_NOT_REGISTERED,  // BluetoothAudioSink not registered.
    ERROR_NOT_UNREGISTERED,  // BluetoothAudioSink not unregistered.
  };

  // Options to configure an A2DP audio sink.
  struct Options {
    Options();
    ~Options();

    uint8_t codec;
    std::vector<uint8_t> capabilities;
  };

  // Interface for observing changes from a BluetoothAudioSink.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the state of the BluetoothAudioSink object is changed.
    // |audio_sink| indicates the object being changed, and |state| indicates
    // the new state of that object.
    virtual void BluetoothAudioSinkStateChanged(
        BluetoothAudioSink* audio_sink,
        BluetoothAudioSink::State state) = 0;

    // Called when the volume of the BluetoothAudioSink object is changed.
    // |audio_sink| indicates the object being changed, and |volume| indicates
    // the new volume level of that object.
    virtual void BluetoothAudioSinkVolumeChanged(
        BluetoothAudioSink* audio_sink,
        uint16_t volume) = 0;

    // Called when there is audio data available. |audio_sink| indicates the
    // object being changed. |data| is the pointer to the audio data and |size|
    // is the number of bytes in |data|. |read_mtu| is the max size of the RTP
    // packet. This method provides the raw audio data which hasn't been
    // processed, so RTP assembling and SBC decoding need to be handled in order
    // to get PCM data.
    virtual void BluetoothAudioSinkDataAvailable(BluetoothAudioSink* audio_sink,
                                                 char* data,
                                                 size_t size,
                                                 uint16_t read_mtu) = 0;
  };

  // The ErrorCallback is used for the methods that can fail in which case it
  // is called.
  typedef base::Callback<void(ErrorCode)> ErrorCallback;

  // Possible volumes for media transport are 0-127, and 128 is used to
  // represent invalid volume.
  static const uint16_t kInvalidVolume;

  // Unregisters the audio sink. An audio sink will unregister itself
  // automatically in its destructor, but calling Unregister is recommended,
  // since user applications can be notified of an error returned by the call.
  virtual void Unregister(const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Adds and removes an observer for events on the BluetoothAudioSink object.
  // If monitoring multiple audio sinks, check the |audio_sink| parameter of
  // observer methods to determine which audio sink is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Getters for state and volume.
  virtual State GetState() const = 0;

  // Returns the current volume level of the audio sink. The valid volumes are
  // 0-127, and |kInvalidVolume| is returned instead if the volume is unknown.
  virtual uint16_t GetVolume() const = 0;

 protected:
  friend class base::RefCounted<BluetoothAudioSink>;
  BluetoothAudioSink();

  // The destructor invokes Unregister() to ensure the audio sink will be
  // unregistered even if the user applications fail to do so.
  virtual ~BluetoothAudioSink();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothAudioSink);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_AUDIO_SINK_H_
