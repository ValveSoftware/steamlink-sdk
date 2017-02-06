// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_SERIAL_CONNECTION_FACTORY_H_
#define DEVICE_SERIAL_SERIAL_CONNECTION_FACTORY_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/serial/data_stream.mojom.h"
#include "device/serial/serial.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace device {

class SerialIoHandler;

class SerialConnectionFactory
    : public base::RefCountedThreadSafe<SerialConnectionFactory> {
 public:
  typedef base::Callback<scoped_refptr<SerialIoHandler>()> IoHandlerFactory;

  SerialConnectionFactory(
      const IoHandlerFactory& io_handler_factory,
      scoped_refptr<base::SingleThreadTaskRunner> connect_task_runner);

  void CreateConnection(
      const std::string& path,
      serial::ConnectionOptionsPtr options,
      mojo::InterfaceRequest<serial::Connection> connection_request,
      mojo::InterfaceRequest<serial::DataSink> sink,
      mojo::InterfaceRequest<serial::DataSource> source,
      mojo::InterfacePtr<serial::DataSourceClient> source_client);

 private:
  friend class base::RefCountedThreadSafe<SerialConnectionFactory>;
  class ConnectTask;

  virtual ~SerialConnectionFactory();

  const IoHandlerFactory io_handler_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> connect_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SerialConnectionFactory);
};

}  // namespace device

#endif  // DEVICE_SERIAL_SERIAL_CONNECTION_FACTORY_H_
