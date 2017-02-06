// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_AUDIO_SINK_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_AUDIO_SINK_BLUEZ_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list.h"
#include "dbus/file_descriptor.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_audio_sink.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_media_client.h"
#include "device/bluetooth/dbus/bluetooth_media_endpoint_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_media_transport_client.h"

namespace bluez {

class BluetoothAudioSinkBlueZTest;

class DEVICE_BLUETOOTH_EXPORT BluetoothAudioSinkBlueZ
    : public device::BluetoothAudioSink,
      public device::BluetoothAdapter::Observer,
      public bluez::BluetoothMediaClient::Observer,
      public bluez::BluetoothMediaTransportClient::Observer,
      public bluez::BluetoothMediaEndpointServiceProvider::Delegate,
      public base::MessageLoopForIO::Watcher {
 public:
  explicit BluetoothAudioSinkBlueZ(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // device::BluetoothAudioSink overrides.
  // Unregisters a BluetoothAudioSink. |callback| should handle
  // the clean-up after the audio sink is deleted successfully, otherwise
  // |error_callback| will be called.
  void Unregister(
      const base::Closure& callback,
      const device::BluetoothAudioSink::ErrorCallback& error_callback) override;
  void AddObserver(BluetoothAudioSink::Observer* observer) override;
  void RemoveObserver(BluetoothAudioSink::Observer* observer) override;
  device::BluetoothAudioSink::State GetState() const override;
  uint16_t GetVolume() const override;

  // Registers a BluetoothAudioSink. User applications can use |options| to
  // configure the audio sink. |callback| will be executed if the audio sink is
  // successfully registered, otherwise |error_callback| will be called. Called
  // by BluetoothAdapterBlueZ.
  void Register(
      const device::BluetoothAudioSink::Options& options,
      const base::Closure& callback,
      const device::BluetoothAudioSink::ErrorCallback& error_callback);

  // Returns a pointer to the media endpoint object. This function should be
  // used for testing purpose only.
  bluez::BluetoothMediaEndpointServiceProvider* GetEndpointServiceProvider();

 private:
  ~BluetoothAudioSinkBlueZ() override;

  // device::BluetoothAdapter::Observer overrides.
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  // bluez::BluetoothMediaClient::Observer overrides.
  void MediaRemoved(const dbus::ObjectPath& object_path) override;

  // bluez::BluetoothMediaTransportClient::Observer overrides.
  void MediaTransportRemoved(const dbus::ObjectPath& object_path) override;
  void MediaTransportPropertyChanged(const dbus::ObjectPath& object_path,
                                     const std::string& property_name) override;

  // bluez::BluetoothMediaEndpointServiceProvider::Delegate overrides.
  void SetConfiguration(const dbus::ObjectPath& transport_path,
                        const TransportProperties& properties) override;
  void SelectConfiguration(
      const std::vector<uint8_t>& capabilities,
      const SelectConfigurationCallback& callback) override;
  void ClearConfiguration(const dbus::ObjectPath& transport_path) override;
  void Released() override;

  // base::MessageLoopForIO::Watcher overrides.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // Acquires file descriptor via current transport object when the state change
  // is triggered by MediaTransportPropertyChanged.
  void AcquireFD();

  // Watches if there is any available data from |fd_|.
  void WatchFD();

  // Stops watching |fd_| and resets |fd_|.
  void StopWatchingFD();

  // Reads from the file descriptor acquired via Media Transport object and
  // notify |observer_| while the audio data is available.
  void ReadFromFile();

  // Called when the state property of BluetoothMediaTransport has been updated.
  void StateChanged(device::BluetoothAudioSink::State state);

  // Called when the volume property of BluetoothMediaTransport has been
  // updated.
  void VolumeChanged(uint16_t volume);

  // Called when the registration of Media Endpoint has succeeded.
  void OnRegisterSucceeded(const base::Closure& callback);

  // Called when the registration of Media Endpoint failed.
  void OnRegisterFailed(
      const device::BluetoothAudioSink::ErrorCallback& error_callback,
      const std::string& error_name,
      const std::string& error_message);

  // Called when the unregistration of Media Endpoint has succeeded. The
  // clean-up of media, media transport and media endpoint will be handled here.
  void OnUnregisterSucceeded(const base::Closure& callback);

  // Called when the unregistration of Media Endpoint failed.
  void OnUnregisterFailed(
      const device::BluetoothAudioSink::ErrorCallback& error_callback,
      const std::string& error_name,
      const std::string& error_message);

  // Called when the file descriptor, read MTU and write MTU are retrieved
  // successfully using |transport_path_|.
  void OnAcquireSucceeded(dbus::FileDescriptor* fd,
                          const uint16_t read_mtu,
                          const uint16_t write_mtu);

  // Called when acquiring the file descriptor, read MTU and write MTU failed.
  void OnAcquireFailed(const std::string& error_name,
                       const std::string& error_message);

  // Called when the file descriptor is released successfully.
  void OnReleaseFDSucceeded();

  // Called when it failed to release file descriptor.
  void OnReleaseFDFailed(const std::string& error_name,
                         const std::string& error_message);

  // Helper functions to clean up media, media transport and media endpoint.
  // Called when the |state_| changes to either STATE_INVALID or
  // STATE_DISCONNECTED.
  void ResetMedia();
  void ResetTransport();
  void ResetEndpoint();

  // The connection state between the BluetoothAudioSinkBlueZ and the remote
  // device.
  device::BluetoothAudioSink::State state_;

  // The volume control by the remote device during the streaming. The valid
  // range of volume is 0-127, and 128 is used to represent invalid volume.
  uint16_t volume_;

  // Read MTU of the file descriptor acquired via Media Transport object.
  uint16_t read_mtu_;

  // Write MTU of the file descriptor acquired via Media Transport object.
  uint16_t write_mtu_;

  // Flag for logging the read failure in ReadFromFD.
  bool read_has_failed_;

  // The file which takes ownership of the file descriptor acquired via Media
  // Transport object.
  std::unique_ptr<base::File> file_;

  // To avoid reallocation of memory, data will be updated only when |read_mtu_|
  // changes.
  std::unique_ptr<char[]> data_;

  // File descriptor watcher for the file descriptor acquired via Media
  // Transport object.
  base::MessageLoopForIO::FileDescriptorWatcher fd_read_watcher_;

  // Object path of the media object being used.
  dbus::ObjectPath media_path_;

  // Object path of the transport object being used.
  dbus::ObjectPath transport_path_;

  // Object path of the media endpoint object being used.
  dbus::ObjectPath endpoint_path_;

  // BT adapter which the audio sink binds to. |adapter_| should outlive
  // a BluetoothAudioSinkBlueZ object.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Options used to initiate Media Endpoint and select configuration for the
  // transport.
  device::BluetoothAudioSink::Options options_;

  // Media Endpoint object owned by the audio sink object.
  std::unique_ptr<bluez::BluetoothMediaEndpointServiceProvider> media_endpoint_;

  // List of observers interested in event notifications from us. Objects in
  // |observers_| are expected to outlive a BluetoothAudioSinkBlueZ object.
  base::ObserverList<BluetoothAudioSink::Observer> observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAudioSinkBlueZ> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAudioSinkBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_AUDIO_SINK_BLUEZ_H_
