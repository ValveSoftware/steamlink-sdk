// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_chromeos.h"

#include <queue>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_profile_manager_client.h"
#include "chromeos/dbus/bluetooth_profile_service_provider.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "dbus/bus.h"
#include "dbus/file_descriptor.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_chromeos.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_device_chromeos.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_net.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothSocketThread;
using device::BluetoothUUID;

namespace {

const char kAcceptFailed[] = "Failed to accept connection.";
const char kInvalidUUID[] = "Invalid UUID";
const char kSocketNotListening[] = "Socket is not listening.";

}  // namespace

namespace chromeos {

// static
scoped_refptr<BluetoothSocketChromeOS>
BluetoothSocketChromeOS::CreateBluetoothSocket(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<BluetoothSocketThread> socket_thread,
    net::NetLog* net_log,
    const net::NetLog::Source& source) {
  DCHECK(ui_task_runner->RunsTasksOnCurrentThread());

  return make_scoped_refptr(
      new BluetoothSocketChromeOS(
          ui_task_runner, socket_thread, net_log, source));
}

BluetoothSocketChromeOS::AcceptRequest::AcceptRequest() {}

BluetoothSocketChromeOS::AcceptRequest::~AcceptRequest() {}

BluetoothSocketChromeOS::ConnectionRequest::ConnectionRequest()
    : accepting(false),
      cancelled(false) {}

BluetoothSocketChromeOS::ConnectionRequest::~ConnectionRequest() {}

BluetoothSocketChromeOS::BluetoothSocketChromeOS(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<BluetoothSocketThread> socket_thread,
    net::NetLog* net_log,
    const net::NetLog::Source& source)
    : BluetoothSocketNet(ui_task_runner, socket_thread, net_log, source) {
}

BluetoothSocketChromeOS::~BluetoothSocketChromeOS() {
  DCHECK(object_path_.value().empty());
  DCHECK(profile_.get() == NULL);

  if (adapter_.get()) {
    adapter_->RemoveObserver(this);
    adapter_ = NULL;
  }
}

void BluetoothSocketChromeOS::Connect(
    const BluetoothDeviceChromeOS* device,
    const BluetoothUUID& uuid,
    const base::Closure& success_callback,
    const ErrorCompletionCallback& error_callback) {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());
  DCHECK(object_path_.value().empty());
  DCHECK(!profile_.get());

  if (!uuid.IsValid()) {
    error_callback.Run(kInvalidUUID);
    return;
  }

  device_address_ = device->GetAddress();
  device_path_ = device->object_path();
  uuid_ = uuid;
  options_.reset(new BluetoothProfileManagerClient::Options());

  RegisterProfile(success_callback, error_callback);
}

void BluetoothSocketChromeOS::Listen(
    scoped_refptr<BluetoothAdapter> adapter,
    SocketType socket_type,
    const BluetoothUUID& uuid,
    int psm_or_channel,
    const base::Closure& success_callback,
    const ErrorCompletionCallback& error_callback) {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());
  DCHECK(object_path_.value().empty());
  DCHECK(!profile_.get());

  if (!uuid.IsValid()) {
    error_callback.Run(kInvalidUUID);
    return;
  }

  adapter_ = adapter;
  adapter_->AddObserver(this);

  uuid_ = uuid;
  options_.reset(new BluetoothProfileManagerClient::Options());

  switch (socket_type) {
    case kRfcomm:
      options_->channel.reset(new uint16(
          psm_or_channel == BluetoothAdapter::kChannelAuto
              ? 0 : psm_or_channel));
      break;
    case kL2cap:
      options_->psm.reset(new uint16(
          psm_or_channel == BluetoothAdapter::kPsmAuto
              ? 0 : psm_or_channel));
      break;
    default:
      NOTREACHED();
  }

  RegisterProfile(success_callback, error_callback);
}

void BluetoothSocketChromeOS::Close() {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());

  if (profile_)
    UnregisterProfile();

  if (!device_path_.value().empty()) {
    BluetoothSocketNet::Close();
  } else {
    DoCloseListening();
  }
}

void BluetoothSocketChromeOS::Disconnect(const base::Closure& callback) {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());

  if (profile_)
   UnregisterProfile();

  if (!device_path_.value().empty()) {
    BluetoothSocketNet::Disconnect(callback);
  } else {
    DoCloseListening();
    callback.Run();
  }
}

void BluetoothSocketChromeOS::Accept(
    const AcceptCompletionCallback& success_callback,
    const ErrorCompletionCallback& error_callback) {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());

  if (!device_path_.value().empty()) {
    error_callback.Run(kSocketNotListening);
    return;
  }

  // Only one pending accept at a time
  if (accept_request_.get()) {
    error_callback.Run(net::ErrorToString(net::ERR_IO_PENDING));
    return;
  }

  accept_request_.reset(new AcceptRequest);
  accept_request_->success_callback = success_callback;
  accept_request_->error_callback = error_callback;

  if (connection_request_queue_.size() >= 1) {
    AcceptConnectionRequest();
  }
}

void BluetoothSocketChromeOS::RegisterProfile(
    const base::Closure& success_callback,
    const ErrorCompletionCallback& error_callback) {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());
  DCHECK(object_path_.value().empty());
  DCHECK(!profile_.get());

  // The object path is relatively meaningless, but has to be unique, so for
  // connecting profiles use a combination of the device address and profile
  // UUID.
  std::string device_address_path, uuid_path;
  base::ReplaceChars(device_address_, ":-", "_", &device_address_path);
  base::ReplaceChars(uuid_.canonical_value(), ":-", "_", &uuid_path);
  if (!device_address_path.empty()) {
    object_path_ = dbus::ObjectPath("/org/chromium/bluetooth_profile/" +
                                    device_address_path + "/" + uuid_path);
  } else {
    object_path_ = dbus::ObjectPath("/org/chromium/bluetooth_profile/" +
                                    uuid_path);
  }

  // Create the service provider for the profile object.
  dbus::Bus* system_bus = DBusThreadManager::Get()->GetSystemBus();
  profile_.reset(BluetoothProfileServiceProvider::Create(
      system_bus, object_path_, this));
  DCHECK(profile_.get());

  // Before reaching out to the Bluetooth Daemon to register a listening socket,
  // make sure it's actually running. If not, report success and carry on;
  // the profile will be registered when the daemon becomes available.
  if (adapter_ && !adapter_->IsPresent()) {
    VLOG(1) << object_path_.value() << ": Delaying profile registration.";
    success_callback.Run();
    return;
  }

  VLOG(1) << object_path_.value() << ": Registering profile.";
  DBusThreadManager::Get()->GetBluetoothProfileManagerClient()->
      RegisterProfile(
          object_path_,
          uuid_.canonical_value(),
          *options_,
          base::Bind(&BluetoothSocketChromeOS::OnRegisterProfile,
                     this,
                     success_callback,
                     error_callback),
          base::Bind(&BluetoothSocketChromeOS::OnRegisterProfileError,
                     this,
                     error_callback));
}

void BluetoothSocketChromeOS::OnRegisterProfile(
    const base::Closure& success_callback,
    const ErrorCompletionCallback& error_callback) {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());
  if (!device_path_.value().empty()) {
    VLOG(1) << object_path_.value() << ": Profile registered, connecting to "
            << device_path_.value();

    DBusThreadManager::Get()->GetBluetoothDeviceClient()->
        ConnectProfile(
            device_path_,
            uuid_.canonical_value(),
            base::Bind(
                &BluetoothSocketChromeOS::OnConnectProfile,
                this,
                success_callback),
            base::Bind(
                &BluetoothSocketChromeOS::OnConnectProfileError,
                this,
                error_callback));
  } else {
    VLOG(1) << object_path_.value() << ": Profile registered.";
    success_callback.Run();
  }
}

void BluetoothSocketChromeOS::OnRegisterProfileError(
    const ErrorCompletionCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());
  LOG(WARNING) << object_path_.value() << ": Failed to register profile: "
               << error_name << ": " << error_message;
  error_callback.Run(error_message);
}

void BluetoothSocketChromeOS::OnConnectProfile(
    const base::Closure& success_callback) {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());
  VLOG(1) << object_path_.value() << ": Profile connected.";
  UnregisterProfile();
  success_callback.Run();
}

void BluetoothSocketChromeOS::OnConnectProfileError(
    const ErrorCompletionCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());
  LOG(WARNING) << object_path_.value() << ": Failed to connect profile: "
               << error_name << ": " << error_message;
  UnregisterProfile();
  error_callback.Run(error_message);
}

void BluetoothSocketChromeOS::AdapterPresentChanged(BluetoothAdapter* adapter,
                                                    bool present) {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());
  DCHECK(!object_path_.value().empty());
  DCHECK(profile_.get());

  if (!present)
    return;

  VLOG(1) << object_path_.value() << ": Re-register profile.";
  DBusThreadManager::Get()->GetBluetoothProfileManagerClient()->
      RegisterProfile(
          object_path_,
          uuid_.canonical_value(),
          *options_,
          base::Bind(&BluetoothSocketChromeOS::OnInternalRegisterProfile,
                     this),
          base::Bind(&BluetoothSocketChromeOS::OnInternalRegisterProfileError,
                     this));
}

void BluetoothSocketChromeOS::OnInternalRegisterProfile() {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());

  VLOG(1) << object_path_.value() << ": Profile re-registered";
}

void BluetoothSocketChromeOS::OnInternalRegisterProfileError(
    const std::string& error_name,
    const std::string& error_message) {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());

  // It's okay if the profile already exists, it means we registered it on
  // initialization.
  if (error_name == bluetooth_profile_manager::kErrorAlreadyExists)
    return;

  LOG(WARNING) << object_path_.value() << ": Failed to re-register profile: "
               << error_name << ": " << error_message;
}

void BluetoothSocketChromeOS::Released() {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());
  VLOG(1) << object_path_.value() << ": Release";
}

void BluetoothSocketChromeOS::NewConnection(
    const dbus::ObjectPath& device_path,
    scoped_ptr<dbus::FileDescriptor> fd,
    const BluetoothProfileServiceProvider::Delegate::Options& options,
    const ConfirmationCallback& callback) {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());
  VLOG(1) << object_path_.value() << ": New connection from device: "
          << device_path.value();

  if (!device_path_.value().empty()) {
    DCHECK(device_path_ == device_path);

    socket_thread()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(
            &BluetoothSocketChromeOS::DoNewConnection,
            this,
            device_path_,
            base::Passed(&fd),
            options,
            callback));
  } else {
    linked_ptr<ConnectionRequest> request(new ConnectionRequest());
    request->device_path = device_path;
    request->fd = fd.Pass();
    request->options = options;
    request->callback = callback;

    connection_request_queue_.push(request);
    VLOG(1) << object_path_.value() << ": Connection is now pending.";
    if (accept_request_) {
      AcceptConnectionRequest();
    }
  }
}

void BluetoothSocketChromeOS::RequestDisconnection(
    const dbus::ObjectPath& device_path,
    const ConfirmationCallback& callback) {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());
  VLOG(1) << object_path_.value() << ": Request disconnection";
  callback.Run(SUCCESS);
}

void BluetoothSocketChromeOS::Cancel() {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());
  VLOG(1) << object_path_.value() << ": Cancel";

  if (!connection_request_queue_.size())
    return;

  // If the front request is being accepted mark it as cancelled, otherwise
  // just pop it from the queue.
  linked_ptr<ConnectionRequest> request = connection_request_queue_.front();
  if (!request->accepting) {
    request->cancelled = true;
  } else {
    connection_request_queue_.pop();
  }
}

void BluetoothSocketChromeOS::AcceptConnectionRequest() {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());
  DCHECK(accept_request_.get());
  DCHECK(connection_request_queue_.size() >= 1);

  VLOG(1) << object_path_.value() << ": Accepting pending connection.";

  linked_ptr<ConnectionRequest> request = connection_request_queue_.front();
  request->accepting = true;

  BluetoothDeviceChromeOS* device =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get())->
          GetDeviceWithPath(request->device_path);
  DCHECK(device);

  scoped_refptr<BluetoothSocketChromeOS> client_socket =
      BluetoothSocketChromeOS::CreateBluetoothSocket(
          ui_task_runner(),
          socket_thread(),
          net_log(),
          source());

  client_socket->device_address_ = device->GetAddress();
  client_socket->device_path_ = request->device_path;
  client_socket->uuid_ = uuid_;

  socket_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(
          &BluetoothSocketChromeOS::DoNewConnection,
          client_socket,
          request->device_path,
          base::Passed(&request->fd),
          request->options,
          base::Bind(&BluetoothSocketChromeOS::OnNewConnection,
                     this,
                     client_socket,
                     request->callback)));
}

void BluetoothSocketChromeOS::DoNewConnection(
    const dbus::ObjectPath& device_path,
    scoped_ptr<dbus::FileDescriptor> fd,
    const BluetoothProfileServiceProvider::Delegate::Options& options,
    const ConfirmationCallback& callback) {
  DCHECK(socket_thread()->task_runner()->RunsTasksOnCurrentThread());
  base::ThreadRestrictions::AssertIOAllowed();
  fd->CheckValidity();

  VLOG(1) << object_path_.value() << ": Validity check complete.";
  if (!fd->is_valid()) {
    LOG(WARNING) << object_path_.value() << " :" << fd->value()
                 << ": Invalid file descriptor received from Bluetooth Daemon.";
    ui_task_runner()->PostTask(FROM_HERE,
                               base::Bind(callback, REJECTED));;
    return;
  }

  if (tcp_socket()) {
    LOG(WARNING) << object_path_.value() << ": Already connected";
    ui_task_runner()->PostTask(FROM_HERE,
                               base::Bind(callback, REJECTED));;
    return;
  }

  ResetTCPSocket();

  // Note: We don't have a meaningful |IPEndPoint|, but that is ok since the
  // TCPSocket implementation does not actually require one.
  int net_result = tcp_socket()->AdoptConnectedSocket(fd->value(),
                                                      net::IPEndPoint());
  if (net_result != net::OK) {
    LOG(WARNING) << object_path_.value() << ": Error adopting socket: "
                 << std::string(net::ErrorToString(net_result));
    ui_task_runner()->PostTask(FROM_HERE,
                               base::Bind(callback, REJECTED));;
    return;
  }

  VLOG(2) << object_path_.value() << ": Taking descriptor, confirming success.";
  fd->TakeValue();
  ui_task_runner()->PostTask(FROM_HERE,
                             base::Bind(callback, SUCCESS));;
}

void BluetoothSocketChromeOS::OnNewConnection(
    scoped_refptr<BluetoothSocket> socket,
    const ConfirmationCallback& callback,
    Status status) {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());
  DCHECK(accept_request_.get());
  DCHECK(connection_request_queue_.size() >= 1);

  linked_ptr<ConnectionRequest> request = connection_request_queue_.front();
  if (status == SUCCESS && !request->cancelled) {
    BluetoothDeviceChromeOS* device =
        static_cast<BluetoothAdapterChromeOS*>(adapter_.get())->
            GetDeviceWithPath(request->device_path);
    DCHECK(device);

    accept_request_->success_callback.Run(device, socket);
  } else {
    accept_request_->error_callback.Run(kAcceptFailed);
  }

  accept_request_.reset(NULL);
  connection_request_queue_.pop();

  callback.Run(status);
}

void BluetoothSocketChromeOS::DoCloseListening() {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());

  if (accept_request_) {
    accept_request_->error_callback.Run(
        net::ErrorToString(net::ERR_CONNECTION_CLOSED));
    accept_request_.reset(NULL);
  }

  while (connection_request_queue_.size() > 0) {
    linked_ptr<ConnectionRequest> request = connection_request_queue_.front();
    request->callback.Run(REJECTED);
    connection_request_queue_.pop();
  }
}

void BluetoothSocketChromeOS::UnregisterProfile() {
  DCHECK(ui_task_runner()->RunsTasksOnCurrentThread());
  DCHECK(!object_path_.value().empty());
  DCHECK(profile_.get());

  VLOG(1) << object_path_.value() << ": Unregister profile";
  DBusThreadManager::Get()->GetBluetoothProfileManagerClient()->
      UnregisterProfile(
          object_path_,
          base::Bind(&BluetoothSocketChromeOS::OnUnregisterProfile,
                     this,
                     object_path_),
          base::Bind(&BluetoothSocketChromeOS::OnUnregisterProfileError,
                     this,
                     object_path_));

  profile_.reset();
  object_path_ = dbus::ObjectPath("");
}

void BluetoothSocketChromeOS::OnUnregisterProfile(
    const dbus::ObjectPath& object_path) {
  VLOG(1) << object_path.value() << ": Profile unregistered";
}

void BluetoothSocketChromeOS::OnUnregisterProfileError(
    const dbus::ObjectPath& object_path,
    const std::string& error_name,
    const std::string& error_message) {
  // It's okay if the profile doesn't exist, it means we haven't registered it
  // yet.
  if (error_name == bluetooth_profile_manager::kErrorDoesNotExist)
    return;

  LOG(WARNING) << object_path_.value() << ": Failed to unregister profile: "
               << error_name << ": " << error_message;
}

}  // namespace chromeos
