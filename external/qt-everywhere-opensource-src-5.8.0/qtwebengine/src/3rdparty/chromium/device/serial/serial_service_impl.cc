// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_service_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "device/serial/serial_io_handler.h"

namespace device {

SerialServiceImpl::SerialServiceImpl(
    scoped_refptr<SerialConnectionFactory> connection_factory,
    mojo::InterfaceRequest<serial::SerialService> request)
    : connection_factory_(connection_factory),
      binding_(this, std::move(request)) {}

SerialServiceImpl::SerialServiceImpl(
    scoped_refptr<SerialConnectionFactory> connection_factory,
    std::unique_ptr<SerialDeviceEnumerator> device_enumerator,
    mojo::InterfaceRequest<serial::SerialService> request)
    : device_enumerator_(std::move(device_enumerator)),
      connection_factory_(connection_factory),
      binding_(this, std::move(request)) {}

SerialServiceImpl::~SerialServiceImpl() {
}

// static
void SerialServiceImpl::Create(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    mojo::InterfaceRequest<serial::SerialService> request) {
  new SerialServiceImpl(
      new SerialConnectionFactory(
          base::Bind(SerialIoHandler::Create,
                     base::ThreadTaskRunnerHandle::Get(), ui_task_runner),
          io_task_runner),
      std::move(request));
}

// static
void SerialServiceImpl::CreateOnMessageLoop(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    mojo::InterfaceRequest<serial::SerialService> request) {
  task_runner->PostTask(FROM_HERE,
                        base::Bind(&SerialServiceImpl::Create, io_task_runner,
                                   ui_task_runner, base::Passed(&request)));
}

void SerialServiceImpl::GetDevices(const GetDevicesCallback& callback) {
  callback.Run(GetDeviceEnumerator()->GetDevices());
}

void SerialServiceImpl::Connect(
    const mojo::String& path,
    serial::ConnectionOptionsPtr options,
    mojo::InterfaceRequest<serial::Connection> connection_request,
    mojo::InterfaceRequest<serial::DataSink> sink,
    mojo::InterfaceRequest<serial::DataSource> source,
    mojo::InterfacePtr<serial::DataSourceClient> source_client) {
  if (!IsValidPath(path))
    return;
  connection_factory_->CreateConnection(
      path, std::move(options), std::move(connection_request), std::move(sink),
      std::move(source), std::move(source_client));
}

SerialDeviceEnumerator* SerialServiceImpl::GetDeviceEnumerator() {
  if (!device_enumerator_)
    device_enumerator_ = SerialDeviceEnumerator::Create();
  return device_enumerator_.get();
}

bool SerialServiceImpl::IsValidPath(const mojo::String& path) {
  mojo::Array<serial::DeviceInfoPtr> devices(
      GetDeviceEnumerator()->GetDevices());
  for (size_t i = 0; i < devices.size(); i++) {
    if (path == devices[i]->path)
      return true;
  }
  return false;
}

}  // namespace device
