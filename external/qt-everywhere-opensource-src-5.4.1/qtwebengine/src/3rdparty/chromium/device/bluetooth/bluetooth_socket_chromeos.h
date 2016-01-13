// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_CHROMEOS_H_

#include <queue>
#include <string>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_profile_manager_client.h"
#include "chromeos/dbus/bluetooth_profile_service_provider.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_net.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace dbus {
class FileDescriptor;
}  // namespace dbus

namespace chromeos {

class BluetoothDeviceChromeOS;

// The BluetoothSocketChromeOS class implements BluetoothSocket for the
// Chrome OS platform.
class CHROMEOS_EXPORT BluetoothSocketChromeOS
    : public device::BluetoothSocketNet,
      public device::BluetoothAdapter::Observer,
      public BluetoothProfileServiceProvider::Delegate {
 public:
  static scoped_refptr<BluetoothSocketChromeOS> CreateBluetoothSocket(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<device::BluetoothSocketThread> socket_thread,
      net::NetLog* net_log,
      const net::NetLog::Source& source);

  // Connects this socket to the service on |device| published as UUID |uuid|,
  // the underlying protocol and PSM or Channel is obtained through service
  // discovery. On a successful connection the socket properties will be updated
  // and |success_callback| called. On failure |error_callback| will be called
  // with a message explaining the cause of the failure.
  virtual void Connect(const BluetoothDeviceChromeOS* device,
                       const device::BluetoothUUID& uuid,
                       const base::Closure& success_callback,
                       const ErrorCompletionCallback& error_callback);

  // Listens using this socket using a service published on |adapter|. The
  // service is either RFCOMM or L2CAP depending on |socket_type| and published
  // as UUID |uuid|. The |psm_or_channel| argument is interpreted according to
  // |socket_type|. |success_callback| will be called if the service is
  // successfully registered, |error_callback| on failure with a message
  // explaining the cause.
  enum SocketType { kRfcomm, kL2cap };
  virtual void Listen(scoped_refptr<device::BluetoothAdapter> adapter,
                      SocketType socket_type,
                      const device::BluetoothUUID& uuid,
                      int psm_or_channel,
                      const base::Closure& success_callback,
                      const ErrorCompletionCallback& error_callback);

  // BluetoothSocket:
  virtual void Close() OVERRIDE;
  virtual void Disconnect(const base::Closure& callback) OVERRIDE;
  virtual void Accept(const AcceptCompletionCallback& success_callback,
                      const ErrorCompletionCallback& error_callback) OVERRIDE;

  // Returns the object path of the socket.
  const dbus::ObjectPath& object_path() const { return object_path_; }

 protected:
  virtual ~BluetoothSocketChromeOS();

 private:
  BluetoothSocketChromeOS(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<device::BluetoothSocketThread> socket_thread,
      net::NetLog* net_log,
      const net::NetLog::Source& source);

  // Register the underlying profile client object with the Bluetooth Daemon.
  void RegisterProfile(const base::Closure& success_callback,
                       const ErrorCompletionCallback& error_callback);
  void OnRegisterProfile(const base::Closure& success_callback,
                         const ErrorCompletionCallback& error_callback);
  void OnRegisterProfileError(const ErrorCompletionCallback& error_callback,
                              const std::string& error_name,
                              const std::string& error_message);

  // Called by dbus:: on completion of the ConnectProfile() method.
  void OnConnectProfile(const base::Closure& success_callback);
  void OnConnectProfileError(const ErrorCompletionCallback& error_callback,
                             const std::string& error_name,
                             const std::string& error_message);

  // BluetoothAdapter::Observer:
  virtual void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                     bool present) OVERRIDE;

  // Called by dbus:: on completion of the RegisterProfile() method call
  // triggered as a result of the adapter becoming present again.
  void OnInternalRegisterProfile();
  void OnInternalRegisterProfileError(const std::string& error_name,
                                      const std::string& error_message);

  // BluetoothProfileServiceProvider::Delegate:
  virtual void Released() OVERRIDE;
  virtual void NewConnection(
      const dbus::ObjectPath& device_path,
      scoped_ptr<dbus::FileDescriptor> fd,
      const BluetoothProfileServiceProvider::Delegate::Options& options,
      const ConfirmationCallback& callback) OVERRIDE;
  virtual void RequestDisconnection(
      const dbus::ObjectPath& device_path,
      const ConfirmationCallback& callback) OVERRIDE;
  virtual void Cancel() OVERRIDE;

  // Method run to accept a single incoming connection.
  void AcceptConnectionRequest();

  // Method run on the socket thread to validate the file descriptor of a new
  // connection and set up the underlying net::TCPSocket() for it.
  void DoNewConnection(
      const dbus::ObjectPath& device_path,
      scoped_ptr<dbus::FileDescriptor> fd,
      const BluetoothProfileServiceProvider::Delegate::Options& options,
      const ConfirmationCallback& callback);

  // Method run on the UI thread after a new connection has been accepted and
  // a socket allocated in |socket|. Takes care of calling the Accept()
  // callback and |callback| with the right arguments based on |status|.
  void OnNewConnection(scoped_refptr<BluetoothSocket> socket,
                       const ConfirmationCallback& callback,
                       Status status);

  // Method run on the socket thread with a valid file descriptor |fd|, once
  // complete calls |callback| on the UI thread with an appropriate argument
  // indicating success or failure.
  void DoConnect(scoped_ptr<dbus::FileDescriptor> fd,
                 const ConfirmationCallback& callback);

  // Method run to clean-up a listening socket.
  void DoCloseListening();

  // Unregister the underlying profile client object from the Bluetooth Daemon.
  void UnregisterProfile();
  void OnUnregisterProfile(const dbus::ObjectPath& object_path);
  void OnUnregisterProfileError(const dbus::ObjectPath& object_path,
                                const std::string& error_name,
                                const std::string& error_message);

  // Adapter the profile is registered against; this is only present when the
  // socket is listening.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Address and D-Bus object path of the device being connected to, empty and
  // ignored if the socket is listening.
  std::string device_address_;
  dbus::ObjectPath device_path_;

  // UUID of the profile being connected to, or listening on.
  device::BluetoothUUID uuid_;

  // Copy of the profile options used for registering the profile.
  scoped_ptr<BluetoothProfileManagerClient::Options> options_;

  // Object path of the local profile D-Bus object.
  dbus::ObjectPath object_path_;

  // Local profile D-Bus object used for receiving profile delegate methods
  // from BlueZ.
  scoped_ptr<BluetoothProfileServiceProvider> profile_;

  // Pending request to an Accept() call.
  struct AcceptRequest {
    AcceptRequest();
    ~AcceptRequest();

    AcceptCompletionCallback success_callback;
    ErrorCompletionCallback error_callback;
  };
  scoped_ptr<AcceptRequest> accept_request_;

  // Queue of incoming connection requests.
  struct ConnectionRequest {
    ConnectionRequest();
    ~ConnectionRequest();

    dbus::ObjectPath device_path;
    scoped_ptr<dbus::FileDescriptor> fd;
    BluetoothProfileServiceProvider::Delegate::Options options;
    ConfirmationCallback callback;
    bool accepting;
    bool cancelled;
  };
  std::queue<linked_ptr<ConnectionRequest> > connection_request_queue_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_CHROMEOS_H_
