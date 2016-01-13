// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/dbus/dbus_external_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/file_descriptor.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "mojo/embedder/channel_init.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/shell/external_service.mojom.h"

namespace mojo {

DBusExternalServiceBase::DBusExternalServiceBase(
    const std::string& service_name)
    : service_name_(service_name),
      exported_object_(NULL) {
}
DBusExternalServiceBase::~DBusExternalServiceBase() {}

void DBusExternalServiceBase::Start() {
  InitializeDBus();
  ExportMethods();
  TakeDBusServiceOwnership();
  DVLOG(1) << "External service started";
}

void DBusExternalServiceBase::ConnectChannel(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  dbus::MessageReader reader(method_call);
  dbus::FileDescriptor wrapped_fd;
  if (!reader.PopFileDescriptor(&wrapped_fd)) {
    sender.Run(
        dbus::ErrorResponse::FromMethodCall(
            method_call,
            "org.chromium.Mojo.BadHandle",
            "Invalid FD.").PassAs<dbus::Response>());
    return;
  }
  wrapped_fd.CheckValidity();
  channel_init_.reset(new mojo::embedder::ChannelInit);
  mojo::ScopedMessagePipeHandle message_pipe =
      channel_init_->Init(wrapped_fd.TakeValue(),
                          base::MessageLoopProxy::current());
  CHECK(message_pipe.is_valid());

  Connect(message_pipe.Pass());
  sender.Run(dbus::Response::FromMethodCall(method_call));
}

void DBusExternalServiceBase::ExportMethods() {
  CHECK(exported_object_);
  CHECK(exported_object_->ExportMethodAndBlock(
      kMojoDBusInterface, kMojoDBusConnectMethod,
      base::Bind(&DBusExternalServiceBase::ConnectChannel,
                 base::Unretained(this))));
}

void DBusExternalServiceBase::InitializeDBus() {
  CHECK(!bus_);
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SESSION;
  bus_ = new dbus::Bus(options);
  CHECK(bus_->Connect());
  CHECK(bus_->SetUpAsyncOperations());

  exported_object_ =
      bus_->GetExportedObject(dbus::ObjectPath(kMojoDBusImplPath));
}

void DBusExternalServiceBase::TakeDBusServiceOwnership() {
  CHECK(bus_->RequestOwnershipAndBlock(
      service_name_,
      dbus::Bus::REQUIRE_PRIMARY_ALLOW_REPLACEMENT))
      << "Unable to take ownership of " << service_name_;
}

}  // namespace mojo
