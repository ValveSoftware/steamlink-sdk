// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_audio_sink_bluez.h"

#include <unistd.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "base/debug/stack_trace.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "dbus/message.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"

using dbus::ObjectPath;
using device::BluetoothAudioSink;

namespace {

// TODO(mcchou): Add the constant to dbus/service_constants.h.
const char kBluetoothAudioSinkServicePath[] = "/org/chromium/AudioSink";

const int kInvalidFd = -1;
const uint16_t kInvalidReadMtu = 0;
const uint16_t kInvalidWriteMtu = 0;

ObjectPath GenerateEndpointPath() {
  static unsigned int sequence_number = 0;
  ++sequence_number;
  std::stringstream path;
  path << kBluetoothAudioSinkServicePath << "/endpoint" << sequence_number;
  return ObjectPath(path.str());
}

std::string StateToString(const BluetoothAudioSink::State& state) {
  switch (state) {
    case BluetoothAudioSink::STATE_INVALID:
      return "invalid";
    case BluetoothAudioSink::STATE_DISCONNECTED:
      return "disconnected";
    case BluetoothAudioSink::STATE_IDLE:
      return "idle";
    case BluetoothAudioSink::STATE_PENDING:
      return "pending";
    case BluetoothAudioSink::STATE_ACTIVE:
      return "active";
    default:
      return "unknown";
  }
}

std::string ErrorCodeToString(const BluetoothAudioSink::ErrorCode& error_code) {
  switch (error_code) {
    case BluetoothAudioSink::ERROR_UNSUPPORTED_PLATFORM:
      return "unsupported platform";
    case BluetoothAudioSink::ERROR_INVALID_ADAPTER:
      return "invalid adapter";
    case BluetoothAudioSink::ERROR_NOT_REGISTERED:
      return "not registered";
    case BluetoothAudioSink::ERROR_NOT_UNREGISTERED:
      return "not unregistered";
    default:
      return "unknown";
  }
}

// A dummy error callback for calling Unregister() in destructor.
void UnregisterErrorCallback(device::BluetoothAudioSink::ErrorCode error_code) {
  VLOG(1) << "UnregisterErrorCallback - " << ErrorCodeToString(error_code)
          << "(" << error_code << ")";
}

}  // namespace

namespace bluez {

BluetoothAudioSinkBlueZ::BluetoothAudioSinkBlueZ(
    scoped_refptr<device::BluetoothAdapter> adapter)
    : state_(BluetoothAudioSink::STATE_INVALID),
      volume_(BluetoothAudioSink::kInvalidVolume),
      read_mtu_(kInvalidReadMtu),
      write_mtu_(kInvalidWriteMtu),
      read_has_failed_(false),
      adapter_(adapter),
      weak_ptr_factory_(this) {
  VLOG(1) << "BluetoothAudioSinkBlueZ created";

  CHECK(adapter_.get());
  CHECK(adapter_->IsPresent());
  CHECK(bluez::BluezDBusManager::IsInitialized());

  adapter_->AddObserver(this);

  bluez::BluetoothMediaClient* media =
      bluez::BluezDBusManager::Get()->GetBluetoothMediaClient();
  CHECK(media);
  media->AddObserver(this);

  bluez::BluetoothMediaTransportClient* transport =
      bluez::BluezDBusManager::Get()->GetBluetoothMediaTransportClient();
  CHECK(transport);
  transport->AddObserver(this);

  StateChanged(device::BluetoothAudioSink::STATE_DISCONNECTED);
}

BluetoothAudioSinkBlueZ::~BluetoothAudioSinkBlueZ() {
  VLOG(1) << "BluetoothAudioSinkBlueZ destroyed";

  DCHECK(adapter_.get());

  if (state_ != BluetoothAudioSink::STATE_INVALID && media_endpoint_.get()) {
    Unregister(base::Bind(&base::DoNothing),
               base::Bind(&UnregisterErrorCallback));
  }

  adapter_->RemoveObserver(this);

  bluez::BluetoothMediaClient* media =
      bluez::BluezDBusManager::Get()->GetBluetoothMediaClient();
  CHECK(media);
  media->RemoveObserver(this);

  bluez::BluetoothMediaTransportClient* transport =
      bluez::BluezDBusManager::Get()->GetBluetoothMediaTransportClient();
  CHECK(transport);
  transport->RemoveObserver(this);
}

void BluetoothAudioSinkBlueZ::Unregister(
    const base::Closure& callback,
    const device::BluetoothAudioSink::ErrorCallback& error_callback) {
  VLOG(1) << "Unregister";

  if (!bluez::BluezDBusManager::IsInitialized())
    error_callback.Run(BluetoothAudioSink::ERROR_NOT_UNREGISTERED);

  bluez::BluetoothMediaClient* media =
      bluez::BluezDBusManager::Get()->GetBluetoothMediaClient();
  CHECK(media);

  media->UnregisterEndpoint(
      media_path_, endpoint_path_,
      base::Bind(&BluetoothAudioSinkBlueZ::OnUnregisterSucceeded,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&BluetoothAudioSinkBlueZ::OnUnregisterFailed,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothAudioSinkBlueZ::AddObserver(
    BluetoothAudioSink::Observer* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothAudioSinkBlueZ::RemoveObserver(
    BluetoothAudioSink::Observer* observer) {
  CHECK(observer);
  observers_.RemoveObserver(observer);
}

BluetoothAudioSink::State BluetoothAudioSinkBlueZ::GetState() const {
  return state_;
}

uint16_t BluetoothAudioSinkBlueZ::GetVolume() const {
  return volume_;
}

void BluetoothAudioSinkBlueZ::Register(
    const BluetoothAudioSink::Options& options,
    const base::Closure& callback,
    const BluetoothAudioSink::ErrorCallback& error_callback) {
  VLOG(1) << "Register";

  DCHECK(adapter_.get());
  DCHECK_EQ(state_, BluetoothAudioSink::STATE_DISCONNECTED);

  // Gets system bus.
  dbus::Bus* system_bus = bluez::BluezDBusManager::Get()->GetSystemBus();

  // Creates a Media Endpoint with newly-generated path.
  endpoint_path_ = GenerateEndpointPath();
  media_endpoint_.reset(bluez::BluetoothMediaEndpointServiceProvider::Create(
      system_bus, endpoint_path_, this));

  DCHECK(media_endpoint_.get());

  // Creates endpoint properties with |options|.
  options_ = options;
  bluez::BluetoothMediaClient::EndpointProperties endpoint_properties;
  endpoint_properties.uuid =
      bluez::BluetoothMediaClient::kBluetoothAudioSinkUUID;
  endpoint_properties.codec = options_.codec;
  endpoint_properties.capabilities = options_.capabilities;

  media_path_ =
      static_cast<BluetoothAdapterBlueZ*>(adapter_.get())->object_path();

  bluez::BluetoothMediaClient* media =
      bluez::BluezDBusManager::Get()->GetBluetoothMediaClient();
  CHECK(media);
  media->RegisterEndpoint(
      media_path_, endpoint_path_, endpoint_properties,
      base::Bind(&BluetoothAudioSinkBlueZ::OnRegisterSucceeded,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&BluetoothAudioSinkBlueZ::OnRegisterFailed,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

bluez::BluetoothMediaEndpointServiceProvider*
BluetoothAudioSinkBlueZ::GetEndpointServiceProvider() {
  return media_endpoint_.get();
}

void BluetoothAudioSinkBlueZ::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  VLOG(1) << "AdapterPresentChanged: " << present;

  if (adapter != adapter_.get())
    return;

  if (adapter->IsPresent()) {
    StateChanged(BluetoothAudioSink::STATE_DISCONNECTED);
  } else {
    adapter_->RemoveObserver(this);
    StateChanged(BluetoothAudioSink::STATE_INVALID);
  }
}

void BluetoothAudioSinkBlueZ::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  VLOG(1) << "AdapterPoweredChanged: " << powered;

  if (adapter != adapter_.get())
    return;

  // Regardless of the new powered state, |state_| goes to STATE_DISCONNECTED.
  // If false, the transport is closed, but the endpoint is still valid for use.
  // If true, the previous transport has been torn down, so the |state_| has to
  // be disconnected before SetConfigruation is called.
  if (state_ != BluetoothAudioSink::STATE_INVALID)
    StateChanged(BluetoothAudioSink::STATE_DISCONNECTED);
}

void BluetoothAudioSinkBlueZ::MediaRemoved(const ObjectPath& object_path) {
  if (object_path == media_path_) {
    VLOG(1) << "MediaRemoved: " << object_path.value();
    StateChanged(BluetoothAudioSink::STATE_INVALID);
  }
}

void BluetoothAudioSinkBlueZ::MediaTransportRemoved(
    const ObjectPath& object_path) {
  // Whenever powered of |adapter_| turns false while present stays true, media
  // transport object should be removed accordingly, and the state should be
  // changed to STATE_DISCONNECTED.
  if (object_path == transport_path_) {
    VLOG(1) << "MediaTransportRemoved: " << object_path.value();
    StateChanged(BluetoothAudioSink::STATE_DISCONNECTED);
  }
}

void BluetoothAudioSinkBlueZ::MediaTransportPropertyChanged(
    const ObjectPath& object_path,
    const std::string& property_name) {
  if (object_path != transport_path_)
    return;

  VLOG(1) << "MediaTransportPropertyChanged: " << property_name;

  // Retrieves the property set of the transport object with |object_path|.
  bluez::BluetoothMediaTransportClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothMediaTransportClient()
          ->GetProperties(object_path);

  // Dispatches a property changed event to the corresponding handler.
  if (property_name == properties->state.name()) {
    if (properties->state.value() ==
        bluez::BluetoothMediaTransportClient::kStateIdle) {
      StateChanged(BluetoothAudioSink::STATE_IDLE);
    } else if (properties->state.value() ==
               bluez::BluetoothMediaTransportClient::kStatePending) {
      StateChanged(BluetoothAudioSink::STATE_PENDING);
    } else if (properties->state.value() ==
               bluez::BluetoothMediaTransportClient::kStateActive) {
      StateChanged(BluetoothAudioSink::STATE_ACTIVE);
    }
  } else if (property_name == properties->volume.name()) {
    VolumeChanged(properties->volume.value());
  }
}

void BluetoothAudioSinkBlueZ::SetConfiguration(
    const ObjectPath& transport_path,
    const TransportProperties& properties) {
  VLOG(1) << "SetConfiguration";
  transport_path_ = transport_path;

  // The initial state for a connection should be "idle".
  if (properties.state != bluez::BluetoothMediaTransportClient::kStateIdle) {
    VLOG(1) << "SetConfiugration - unexpected state :" << properties.state;
    return;
  }

  // Updates |volume_| if the volume level is provided in |properties|.
  if (properties.volume.get()) {
    VolumeChanged(*properties.volume);
  }

  StateChanged(BluetoothAudioSink::STATE_IDLE);
}

void BluetoothAudioSinkBlueZ::SelectConfiguration(
    const std::vector<uint8_t>& capabilities,
    const SelectConfigurationCallback& callback) {
  VLOG(1) << "SelectConfiguration";
  callback.Run(options_.capabilities);
}

void BluetoothAudioSinkBlueZ::ClearConfiguration(
    const ObjectPath& transport_path) {
  if (transport_path != transport_path_)
    return;

  VLOG(1) << "ClearConfiguration";
  StateChanged(BluetoothAudioSink::STATE_DISCONNECTED);
}

void BluetoothAudioSinkBlueZ::Released() {
  VLOG(1) << "Released";
  StateChanged(BluetoothAudioSink::STATE_INVALID);
}

void BluetoothAudioSinkBlueZ::OnFileCanReadWithoutBlocking(int fd) {
  ReadFromFile();
}

void BluetoothAudioSinkBlueZ::OnFileCanWriteWithoutBlocking(int fd) {
  // Do nothing for now.
}

void BluetoothAudioSinkBlueZ::AcquireFD() {
  VLOG(1) << "AcquireFD - transport path: " << transport_path_.value();

  read_has_failed_ = false;

  bluez::BluezDBusManager::Get()->GetBluetoothMediaTransportClient()->Acquire(
      transport_path_, base::Bind(&BluetoothAudioSinkBlueZ::OnAcquireSucceeded,
                                  weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothAudioSinkBlueZ::OnAcquireFailed,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothAudioSinkBlueZ::WatchFD() {
  CHECK(file_.get() && file_->IsValid());

  VLOG(1) << "WatchFD - file: " << file_->GetPlatformFile()
          << ", file validity: " << file_->IsValid();

  base::MessageLoopForIO::current()->WatchFileDescriptor(
      file_->GetPlatformFile(), true, base::MessageLoopForIO::WATCH_READ,
      &fd_read_watcher_, this);
}

void BluetoothAudioSinkBlueZ::StopWatchingFD() {
  if (!file_.get()) {
    VLOG(1) << "StopWatchingFD - skip";
    return;
  }

  bool stopped = fd_read_watcher_.StopWatchingFileDescriptor();
  VLOG(1) << "StopWatchingFD - watch stopped: " << stopped;
  CHECK(stopped);

  read_mtu_ = kInvalidReadMtu;
  write_mtu_ = kInvalidWriteMtu;
  file_.reset();  // This will close the file descriptor.
}

void BluetoothAudioSinkBlueZ::ReadFromFile() {
  DCHECK(file_.get() && file_->IsValid());
  DCHECK(data_.get());

  int size = file_->ReadAtCurrentPosNoBestEffort(data_.get(), read_mtu_);

  if (size == -1) {
    // To reduce the number of logs, log only once for multiple failures.
    if (!read_has_failed_) {
      VLOG(1) << "ReadFromFile - failed";
      read_has_failed_ = true;
    }
    return;
  }

  VLOG(1) << "ReadFromFile - read " << size << " bytes";
  FOR_EACH_OBSERVER(
      BluetoothAudioSink::Observer, observers_,
      BluetoothAudioSinkDataAvailable(this, data_.get(), size, read_mtu_));
}

void BluetoothAudioSinkBlueZ::StateChanged(BluetoothAudioSink::State state) {
  if (state == state_)
    return;

  VLOG(1) << "StateChanged - state: " << StateToString(state);

  switch (state) {
    case BluetoothAudioSink::STATE_INVALID:
      ResetMedia();
      ResetEndpoint();
    case BluetoothAudioSink::STATE_DISCONNECTED:
      ResetTransport();
      break;
    case BluetoothAudioSink::STATE_IDLE:
      StopWatchingFD();
      break;
    case BluetoothAudioSink::STATE_PENDING:
      AcquireFD();
      break;
    case BluetoothAudioSink::STATE_ACTIVE:
      WatchFD();
      break;
    default:
      break;
  }

  state_ = state;
  FOR_EACH_OBSERVER(BluetoothAudioSink::Observer, observers_,
                    BluetoothAudioSinkStateChanged(this, state_));
}

void BluetoothAudioSinkBlueZ::VolumeChanged(uint16_t volume) {
  if (volume == volume_)
    return;

  VLOG(1) << "VolumeChanged: " << volume;

  volume_ = std::min(volume, BluetoothAudioSink::kInvalidVolume);
  FOR_EACH_OBSERVER(BluetoothAudioSink::Observer, observers_,
                    BluetoothAudioSinkVolumeChanged(this, volume_));
}

void BluetoothAudioSinkBlueZ::OnRegisterSucceeded(
    const base::Closure& callback) {
  DCHECK(media_endpoint_.get());
  VLOG(1) << "OnRegisterSucceeded";

  StateChanged(BluetoothAudioSink::STATE_DISCONNECTED);
  callback.Run();
}

void BluetoothAudioSinkBlueZ::OnRegisterFailed(
    const BluetoothAudioSink::ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  VLOG(1) << "OnRegisterFailed - error name: " << error_name
          << ", error message: " << error_message;

  ResetEndpoint();
  error_callback.Run(BluetoothAudioSink::ERROR_NOT_REGISTERED);
}

void BluetoothAudioSinkBlueZ::OnUnregisterSucceeded(
    const base::Closure& callback) {
  VLOG(1) << "Unregistered - endpoint: " << endpoint_path_.value();

  // Once the state becomes STATE_INVALID, media, media transport and media
  // endpoint will be reset.
  StateChanged(BluetoothAudioSink::STATE_INVALID);
  callback.Run();
}

void BluetoothAudioSinkBlueZ::OnUnregisterFailed(
    const device::BluetoothAudioSink::ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  VLOG(1) << "OnUnregisterFailed - error name: " << error_name
          << ", error message: " << error_message;

  error_callback.Run(BluetoothAudioSink::ERROR_NOT_UNREGISTERED);
}

void BluetoothAudioSinkBlueZ::OnAcquireSucceeded(dbus::FileDescriptor* fd,
                                                 const uint16_t read_mtu,
                                                 const uint16_t write_mtu) {
  CHECK(fd);
  fd->CheckValidity();
  CHECK(fd->is_valid() && fd->value() != kInvalidFd);
  CHECK_GT(read_mtu, kInvalidReadMtu);
  CHECK_GT(write_mtu, kInvalidWriteMtu);

  // Avoids unnecessary memory reallocation if read MTU doesn't change.
  if (read_mtu != read_mtu_) {
    read_mtu_ = read_mtu;
    data_.reset(new char[read_mtu_]);
    VLOG(1) << "OnAcquireSucceeded - allocate " << read_mtu_
            << " bytes of memory";
  }

  write_mtu_ = write_mtu;

  // Avoids closing the same file descriptor caused by reassignment.
  if (!file_.get() || file_->GetPlatformFile() != fd->value()) {
    // Takes ownership of the file descriptor.
    file_.reset(new base::File(fd->TakeValue()));
    DCHECK(file_->IsValid());
    VLOG(1) << "OnAcquireSucceeded - update file";
  }

  VLOG(1) << "OnAcquireSucceeded - file: " << file_->GetPlatformFile()
          << ", read MTU: " << read_mtu_ << ", write MTU: " << write_mtu_;
}

void BluetoothAudioSinkBlueZ::OnAcquireFailed(
    const std::string& error_name,
    const std::string& error_message) {
  VLOG(1) << "OnAcquireFailed - error name: " << error_name
          << ", error message: " << error_message;
}

void BluetoothAudioSinkBlueZ::OnReleaseFDSucceeded() {
  VLOG(1) << "OnReleaseFDSucceeded";
}

void BluetoothAudioSinkBlueZ::OnReleaseFDFailed(
    const std::string& error_name,
    const std::string& error_message) {
  VLOG(1) << "OnReleaseFDFailed - error name: " << error_name
          << ", error message: " << error_message;
}

void BluetoothAudioSinkBlueZ::ResetMedia() {
  VLOG(1) << "ResetMedia";

  media_path_ = dbus::ObjectPath("");
}

void BluetoothAudioSinkBlueZ::ResetTransport() {
  if (!transport_path_.IsValid()) {
    VLOG(1) << "ResetTransport - skip";
    return;
  }

  VLOG(1) << "ResetTransport - clean-up";

  VolumeChanged(BluetoothAudioSink::kInvalidVolume);
  transport_path_ = dbus::ObjectPath("");
  read_mtu_ = kInvalidReadMtu;
  write_mtu_ = kInvalidWriteMtu;
  file_.reset();
}

void BluetoothAudioSinkBlueZ::ResetEndpoint() {
  VLOG(1) << "ResetEndpoint";

  endpoint_path_ = ObjectPath("");
  media_endpoint_ = nullptr;
}

}  // namespace bluez
