// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/serial/serial_api.h"

#include <algorithm>
#include <vector>

#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "device/serial/serial_device_enumerator.h"
#include "extensions/browser/api/serial/serial_connection.h"
#include "extensions/browser/api/serial/serial_event_dispatcher.h"
#include "extensions/common/api/serial.h"

using content::BrowserThread;

namespace extensions {

namespace api {

namespace {

// It's a fool's errand to come up with a default bitrate, because we don't get
// to control both sides of the communication. Unless the other side has
// implemented auto-bitrate detection (rare), if we pick the wrong rate, then
// you're gonna have a bad time. Close doesn't count.
//
// But we'd like to pick something that has a chance of working, and 9600 is a
// good balance between popularity and speed. So 9600 it is.
const int kDefaultBufferSize = 4096;
const int kDefaultBitrate = 9600;
const serial::DataBits kDefaultDataBits = serial::DATA_BITS_EIGHT;
const serial::ParityBit kDefaultParityBit = serial::PARITY_BIT_NO;
const serial::StopBits kDefaultStopBits = serial::STOP_BITS_ONE;
const int kDefaultReceiveTimeout = 0;
const int kDefaultSendTimeout = 0;

const char kErrorConnectFailed[] = "Failed to connect to the port.";
const char kErrorSerialConnectionNotFound[] = "Serial connection not found.";
const char kErrorGetControlSignalsFailed[] = "Failed to get control signals.";

template <class T>
void SetDefaultScopedPtrValue(std::unique_ptr<T>& ptr, const T& value) {
  if (!ptr.get())
    ptr.reset(new T(value));
}

}  // namespace

SerialAsyncApiFunction::SerialAsyncApiFunction() : manager_(NULL) {
}

SerialAsyncApiFunction::~SerialAsyncApiFunction() {
}

bool SerialAsyncApiFunction::PrePrepare() {
  manager_ = ApiResourceManager<SerialConnection>::Get(browser_context());
  DCHECK(manager_);
  return true;
}

bool SerialAsyncApiFunction::Respond() {
  return error_.empty();
}

SerialConnection* SerialAsyncApiFunction::GetSerialConnection(
    int api_resource_id) {
  return manager_->Get(extension_->id(), api_resource_id);
}

void SerialAsyncApiFunction::RemoveSerialConnection(int api_resource_id) {
  manager_->Remove(extension_->id(), api_resource_id);
}

SerialGetDevicesFunction::SerialGetDevicesFunction() {
}

bool SerialGetDevicesFunction::Prepare() {
  set_work_thread_id(BrowserThread::FILE);
  return true;
}

void SerialGetDevicesFunction::Work() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  std::unique_ptr<device::SerialDeviceEnumerator> enumerator =
      device::SerialDeviceEnumerator::Create();
  mojo::Array<device::serial::DeviceInfoPtr> devices = enumerator->GetDevices();
  results_ = serial::GetDevices::Results::Create(
      devices.To<std::vector<serial::DeviceInfo>>());
}

SerialConnectFunction::SerialConnectFunction() {
}

SerialConnectFunction::~SerialConnectFunction() {
}

bool SerialConnectFunction::Prepare() {
  params_ = serial::Connect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  // Fill in any omitted options to ensure a known initial configuration.
  if (!params_->options.get())
    params_->options.reset(new serial::ConnectionOptions());
  serial::ConnectionOptions* options = params_->options.get();

  SetDefaultScopedPtrValue(options->persistent, false);
  SetDefaultScopedPtrValue(options->buffer_size, kDefaultBufferSize);
  SetDefaultScopedPtrValue(options->bitrate, kDefaultBitrate);
  SetDefaultScopedPtrValue(options->cts_flow_control, false);
  SetDefaultScopedPtrValue(options->receive_timeout, kDefaultReceiveTimeout);
  SetDefaultScopedPtrValue(options->send_timeout, kDefaultSendTimeout);

  if (options->data_bits == serial::DATA_BITS_NONE)
    options->data_bits = kDefaultDataBits;
  if (options->parity_bit == serial::PARITY_BIT_NONE)
    options->parity_bit = kDefaultParityBit;
  if (options->stop_bits == serial::STOP_BITS_NONE)
    options->stop_bits = kDefaultStopBits;

  serial_event_dispatcher_ = SerialEventDispatcher::Get(browser_context());
  DCHECK(serial_event_dispatcher_);

  return true;
}

void SerialConnectFunction::AsyncWorkStart() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  connection_ = CreateSerialConnection(params_->path, extension_->id());
  connection_->Open(*params_->options.get(),
                    base::Bind(&SerialConnectFunction::OnConnected, this));
}

void SerialConnectFunction::OnConnected(bool success) {
  DCHECK(connection_);

  if (!success) {
    delete connection_;
    connection_ = NULL;
  }

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SerialConnectFunction::FinishConnect, this));
}

void SerialConnectFunction::FinishConnect() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!connection_) {
    error_ = kErrorConnectFailed;
  } else {
    int id = manager_->Add(connection_);
    serial::ConnectionInfo info;
    info.connection_id = id;
    if (connection_->GetInfo(&info)) {
      serial_event_dispatcher_->PollConnection(extension_->id(), id);
      results_ = serial::Connect::Results::Create(info);
    } else {
      RemoveSerialConnection(id);
      error_ = kErrorConnectFailed;
    }
  }
  AsyncWorkCompleted();
}

SerialConnection* SerialConnectFunction::CreateSerialConnection(
    const std::string& port,
    const std::string& extension_id) const {
  return new SerialConnection(port, extension_id);
}

SerialUpdateFunction::SerialUpdateFunction() {
}

SerialUpdateFunction::~SerialUpdateFunction() {
}

bool SerialUpdateFunction::Prepare() {
  params_ = serial::Update::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialUpdateFunction::Work() {
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    return;
  }
  bool success = connection->Configure(params_->options);
  results_ = serial::Update::Results::Create(success);
}

SerialDisconnectFunction::SerialDisconnectFunction() {
}

SerialDisconnectFunction::~SerialDisconnectFunction() {
}

bool SerialDisconnectFunction::Prepare() {
  params_ = serial::Disconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialDisconnectFunction::Work() {
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    return;
  }
  RemoveSerialConnection(params_->connection_id);
  results_ = serial::Disconnect::Results::Create(true);
}

SerialSendFunction::SerialSendFunction() {
}

SerialSendFunction::~SerialSendFunction() {
}

bool SerialSendFunction::Prepare() {
  params_ = serial::Send::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialSendFunction::AsyncWorkStart() {
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    AsyncWorkCompleted();
    return;
  }

  if (!connection->Send(
          params_->data,
          base::Bind(&SerialSendFunction::OnSendComplete, this))) {
    OnSendComplete(0, serial::SEND_ERROR_PENDING);
  }
}

void SerialSendFunction::OnSendComplete(int bytes_sent,
                                        serial::SendError error) {
  serial::SendInfo send_info;
  send_info.bytes_sent = bytes_sent;
  send_info.error = error;
  results_ = serial::Send::Results::Create(send_info);
  AsyncWorkCompleted();
}

SerialFlushFunction::SerialFlushFunction() {
}

SerialFlushFunction::~SerialFlushFunction() {
}

bool SerialFlushFunction::Prepare() {
  params_ = serial::Flush::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SerialFlushFunction::Work() {
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    return;
  }

  bool success = connection->Flush();
  results_ = serial::Flush::Results::Create(success);
}

SerialSetPausedFunction::SerialSetPausedFunction() {
}

SerialSetPausedFunction::~SerialSetPausedFunction() {
}

bool SerialSetPausedFunction::Prepare() {
  params_ = serial::SetPaused::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  serial_event_dispatcher_ = SerialEventDispatcher::Get(browser_context());
  DCHECK(serial_event_dispatcher_);
  return true;
}

void SerialSetPausedFunction::Work() {
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    return;
  }

  if (params_->paused != connection->paused()) {
    connection->set_paused(params_->paused);
    if (!params_->paused) {
      serial_event_dispatcher_->PollConnection(extension_->id(),
                                               params_->connection_id);
    }
  }

  results_ = serial::SetPaused::Results::Create();
}

SerialGetInfoFunction::SerialGetInfoFunction() {
}

SerialGetInfoFunction::~SerialGetInfoFunction() {
}

bool SerialGetInfoFunction::Prepare() {
  params_ = serial::GetInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialGetInfoFunction::Work() {
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    return;
  }

  serial::ConnectionInfo info;
  info.connection_id = params_->connection_id;
  connection->GetInfo(&info);
  results_ = serial::GetInfo::Results::Create(info);
}

SerialGetConnectionsFunction::SerialGetConnectionsFunction() {
}

SerialGetConnectionsFunction::~SerialGetConnectionsFunction() {
}

bool SerialGetConnectionsFunction::Prepare() {
  return true;
}

void SerialGetConnectionsFunction::Work() {
  std::vector<serial::ConnectionInfo> infos;
  const base::hash_set<int>* connection_ids =
      manager_->GetResourceIds(extension_->id());
  if (connection_ids) {
    for (base::hash_set<int>::const_iterator it = connection_ids->begin();
         it != connection_ids->end();
         ++it) {
      int connection_id = *it;
      SerialConnection* connection = GetSerialConnection(connection_id);
      if (connection) {
        serial::ConnectionInfo info;
        info.connection_id = connection_id;
        connection->GetInfo(&info);
        infos.push_back(std::move(info));
      }
    }
  }
  results_ = serial::GetConnections::Results::Create(infos);
}

SerialGetControlSignalsFunction::SerialGetControlSignalsFunction() {
}

SerialGetControlSignalsFunction::~SerialGetControlSignalsFunction() {
}

bool SerialGetControlSignalsFunction::Prepare() {
  params_ = serial::GetControlSignals::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialGetControlSignalsFunction::Work() {
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    return;
  }

  serial::DeviceControlSignals signals;
  if (!connection->GetControlSignals(&signals)) {
    error_ = kErrorGetControlSignalsFailed;
    return;
  }

  results_ = serial::GetControlSignals::Results::Create(signals);
}

SerialSetControlSignalsFunction::SerialSetControlSignalsFunction() {
}

SerialSetControlSignalsFunction::~SerialSetControlSignalsFunction() {
}

bool SerialSetControlSignalsFunction::Prepare() {
  params_ = serial::SetControlSignals::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialSetControlSignalsFunction::Work() {
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    return;
  }

  bool success = connection->SetControlSignals(params_->signals);
  results_ = serial::SetControlSignals::Results::Create(success);
}

SerialSetBreakFunction::SerialSetBreakFunction() {
}

SerialSetBreakFunction::~SerialSetBreakFunction() {
}

bool SerialSetBreakFunction::Prepare() {
  params_ = serial::SetBreak::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialSetBreakFunction::Work() {
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    return;
  }
  bool success = connection->SetBreak();
  results_ = serial::SetBreak::Results::Create(success);
}

SerialClearBreakFunction::SerialClearBreakFunction() {
}

SerialClearBreakFunction::~SerialClearBreakFunction() {
}

bool SerialClearBreakFunction::Prepare() {
  params_ = serial::ClearBreak::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialClearBreakFunction::Work() {
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    return;
  }

  bool success = connection->ClearBreak();
  results_ = serial::ClearBreak::Results::Create(success);
}

}  // namespace api

}  // namespace extensions

namespace mojo {

// static
extensions::api::serial::DeviceInfo TypeConverter<
    extensions::api::serial::DeviceInfo,
    device::serial::DeviceInfoPtr>::Convert(const device::serial::DeviceInfoPtr&
                                                device) {
  extensions::api::serial::DeviceInfo info;
  info.path = device->path;
  if (device->has_vendor_id)
    info.vendor_id.reset(new int(static_cast<int>(device->vendor_id)));
  if (device->has_product_id)
    info.product_id.reset(new int(static_cast<int>(device->product_id)));
  if (device->display_name)
    info.display_name.reset(new std::string(device->display_name));
  return info;
}

}  // namespace mojo
