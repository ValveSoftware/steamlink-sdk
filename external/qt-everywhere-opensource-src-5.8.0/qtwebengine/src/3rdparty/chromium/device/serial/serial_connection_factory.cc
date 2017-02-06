// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_connection_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "device/serial/serial_connection.h"
#include "device/serial/serial_io_handler.h"

namespace device {
namespace {

void FillDefaultConnectionOptions(serial::ConnectionOptions* options) {
  if (!options->bitrate)
    options->bitrate = 9600;
  if (options->data_bits == serial::DataBits::NONE)
    options->data_bits = serial::DataBits::EIGHT;
  if (options->stop_bits == serial::StopBits::NONE)
    options->stop_bits = serial::StopBits::ONE;
  if (options->parity_bit == serial::ParityBit::NONE)
    options->parity_bit = serial::ParityBit::NO;
  if (!options->has_cts_flow_control) {
    options->has_cts_flow_control = true;
    options->cts_flow_control = false;
  }
}

}  // namespace

class SerialConnectionFactory::ConnectTask
    : public base::RefCountedThreadSafe<SerialConnectionFactory::ConnectTask> {
 public:
  ConnectTask(scoped_refptr<SerialConnectionFactory> factory,
              const std::string& path,
              serial::ConnectionOptionsPtr options,
              mojo::InterfaceRequest<serial::Connection> connection_request,
              mojo::InterfaceRequest<serial::DataSink> sink,
              mojo::InterfaceRequest<serial::DataSource> source,
              mojo::InterfacePtr<serial::DataSourceClient> source_client);
  void Run();

 private:
  friend class base::RefCountedThreadSafe<SerialConnectionFactory::ConnectTask>;
  virtual ~ConnectTask();
  void Connect();
  void OnConnected(bool success);

  scoped_refptr<SerialConnectionFactory> factory_;
  const std::string path_;
  serial::ConnectionOptionsPtr options_;
  mojo::InterfaceRequest<serial::Connection> connection_request_;
  mojo::InterfaceRequest<serial::DataSink> sink_;
  mojo::InterfaceRequest<serial::DataSource> source_;
  mojo::InterfacePtrInfo<serial::DataSourceClient> source_client_;
  scoped_refptr<SerialIoHandler> io_handler_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTask);
};

SerialConnectionFactory::SerialConnectionFactory(
    const IoHandlerFactory& io_handler_factory,
    scoped_refptr<base::SingleThreadTaskRunner> connect_task_runner)
    : io_handler_factory_(io_handler_factory),
      connect_task_runner_(connect_task_runner) {
}

void SerialConnectionFactory::CreateConnection(
    const std::string& path,
    serial::ConnectionOptionsPtr options,
    mojo::InterfaceRequest<serial::Connection> connection_request,
    mojo::InterfaceRequest<serial::DataSink> sink,
    mojo::InterfaceRequest<serial::DataSource> source,
    mojo::InterfacePtr<serial::DataSourceClient> source_client) {
  scoped_refptr<ConnectTask> task(new ConnectTask(
      this, path, std::move(options), std::move(connection_request),
      std::move(sink), std::move(source), std::move(source_client)));
  task->Run();
}

SerialConnectionFactory::~SerialConnectionFactory() {
}

SerialConnectionFactory::ConnectTask::ConnectTask(
    scoped_refptr<SerialConnectionFactory> factory,
    const std::string& path,
    serial::ConnectionOptionsPtr options,
    mojo::InterfaceRequest<serial::Connection> connection_request,
    mojo::InterfaceRequest<serial::DataSink> sink,
    mojo::InterfaceRequest<serial::DataSource> source,
    mojo::InterfacePtr<serial::DataSourceClient> source_client)
    : factory_(factory),
      path_(path),
      options_(std::move(options)),
      connection_request_(std::move(connection_request)),
      sink_(std::move(sink)),
      source_(std::move(source)),
      source_client_(source_client.PassInterface()) {
  if (!options_) {
    options_ = serial::ConnectionOptions::New();
  }
  FillDefaultConnectionOptions(options_.get());
}

void SerialConnectionFactory::ConnectTask::Run() {
  factory_->connect_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SerialConnectionFactory::ConnectTask::Connect, this));
}

SerialConnectionFactory::ConnectTask::~ConnectTask() {
}

void SerialConnectionFactory::ConnectTask::Connect() {
  io_handler_ = factory_->io_handler_factory_.Run();
  io_handler_->Open(
      path_, *options_,
      base::Bind(&SerialConnectionFactory::ConnectTask::OnConnected, this));
}

void SerialConnectionFactory::ConnectTask::OnConnected(bool success) {
  DCHECK(io_handler_.get());
  if (!success) {
    return;
  }

  new SerialConnection(io_handler_, std::move(sink_), std::move(source_),
                       mojo::MakeProxy(std::move(source_client_)),
                       std::move(connection_request_));
}

}  // namespace device
