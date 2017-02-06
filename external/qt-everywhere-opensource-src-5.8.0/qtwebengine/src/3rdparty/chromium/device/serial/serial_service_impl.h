// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_SERIAL_SERVICE_IMPL_H_
#define DEVICE_SERIAL_SERIAL_SERVICE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/serial/data_stream.mojom.h"
#include "device/serial/serial.mojom.h"
#include "device/serial/serial_connection_factory.h"
#include "device/serial/serial_device_enumerator.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

class SerialServiceImpl : public serial::SerialService {
 public:
  SerialServiceImpl(scoped_refptr<SerialConnectionFactory> connection_factory,
                    mojo::InterfaceRequest<serial::SerialService> request);
  SerialServiceImpl(scoped_refptr<SerialConnectionFactory> connection_factory,
                    std::unique_ptr<SerialDeviceEnumerator> device_enumerator,
                    mojo::InterfaceRequest<serial::SerialService> request);
  ~SerialServiceImpl() override;

  static void Create(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                     scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
                     mojo::InterfaceRequest<serial::SerialService> request);
  static void CreateOnMessageLoop(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      mojo::InterfaceRequest<serial::SerialService> request);

  // SerialService overrides.
  void GetDevices(const GetDevicesCallback& callback) override;
  void Connect(
      const mojo::String& path,
      serial::ConnectionOptionsPtr options,
      mojo::InterfaceRequest<serial::Connection> connection_request,
      mojo::InterfaceRequest<serial::DataSink> sink,
      mojo::InterfaceRequest<serial::DataSource> source,
      mojo::InterfacePtr<serial::DataSourceClient> source_client) override;

 private:
  SerialDeviceEnumerator* GetDeviceEnumerator();
  bool IsValidPath(const mojo::String& path);

  std::unique_ptr<SerialDeviceEnumerator> device_enumerator_;
  scoped_refptr<SerialConnectionFactory> connection_factory_;
  mojo::StrongBinding<serial::SerialService> binding_;

  DISALLOW_COPY_AND_ASSIGN(SerialServiceImpl);
};

}  // namespace device

#endif  // DEVICE_SERIAL_SERIAL_SERVICE_IMPL_H_
