// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_OBJECT_PROXY_H_
#define DBUS_OBJECT_PROXY_H_

#include <dbus/dbus.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "dbus/dbus_export.h"
#include "dbus/object_path.h"

namespace dbus {

class Bus;
class ErrorResponse;
class MethodCall;
class Response;
class Signal;

// ObjectProxy is used to communicate with remote objects, mainly for
// calling methods of these objects.
//
// ObjectProxy is a ref counted object, to ensure that |this| of the
// object is is alive when callbacks referencing |this| are called; the
// bus always holds at least one of those references so object proxies
// always last as long as the bus that created them.
class CHROME_DBUS_EXPORT ObjectProxy
    : public base::RefCountedThreadSafe<ObjectProxy> {
 public:
  // Client code should use Bus::GetObjectProxy() or
  // Bus::GetObjectProxyWithOptions() instead of this constructor.
  ObjectProxy(Bus* bus,
              const std::string& service_name,
              const ObjectPath& object_path,
              int options);

  // Options to be OR-ed together when calling Bus::GetObjectProxyWithOptions().
  // Set the IGNORE_SERVICE_UNKNOWN_ERRORS option to silence logging of
  // org.freedesktop.DBus.Error.ServiceUnknown errors.
  enum Options {
    DEFAULT_OPTIONS = 0,
    IGNORE_SERVICE_UNKNOWN_ERRORS = 1 << 0
  };

  // Special timeout constants.
  //
  // The constants correspond to DBUS_TIMEOUT_USE_DEFAULT and
  // DBUS_TIMEOUT_INFINITE. Here we use literal numbers instead of these
  // macros as these aren't defined with D-Bus earlier than 1.4.12.
  enum {
    TIMEOUT_USE_DEFAULT = -1,
    TIMEOUT_INFINITE = 0x7fffffff,
  };

  // Called when an error response is returned or no response is returned.
  // Used for CallMethodWithErrorCallback().
  typedef base::Callback<void(ErrorResponse*)> ErrorCallback;

  // Called when the response is returned. Used for CallMethod().
  typedef base::Callback<void(Response*)> ResponseCallback;

  // Called when a signal is received. Signal* is the incoming signal.
  typedef base::Callback<void (Signal*)> SignalCallback;

  // Called when NameOwnerChanged signal is received.
  typedef base::Callback<void(
      const std::string& old_owner,
      const std::string& new_owner)> NameOwnerChangedCallback;

  // Called when the service becomes available.
  typedef base::Callback<void(
      bool service_is_available)> WaitForServiceToBeAvailableCallback;

  // Called when the object proxy is connected to the signal.
  // Parameters:
  // - the interface name.
  // - the signal name.
  // - whether it was successful or not.
  typedef base::Callback<void (const std::string&, const std::string&, bool)>
      OnConnectedCallback;

  // Calls the method of the remote object and blocks until the response
  // is returned. Returns NULL on error.
  //
  // BLOCKING CALL.
  virtual scoped_ptr<Response> CallMethodAndBlock(MethodCall* method_call,
                                                  int timeout_ms);

  // Requests to call the method of the remote object.
  //
  // |callback| will be called in the origin thread, once the method call
  // is complete. As it's called in the origin thread, |callback| can
  // safely reference objects in the origin thread (i.e. UI thread in most
  // cases). If the caller is not interested in the response from the
  // method (i.e. calling a method that does not return a value),
  // EmptyResponseCallback() can be passed to the |callback| parameter.
  //
  // If the method call is successful, a pointer to Response object will
  // be passed to the callback. If unsuccessful, NULL will be passed to
  // the callback.
  //
  // Must be called in the origin thread.
  virtual void CallMethod(MethodCall* method_call,
                          int timeout_ms,
                          ResponseCallback callback);

  // Requests to call the method of the remote object.
  //
  // |callback| and |error_callback| will be called in the origin thread, once
  // the method call is complete. As it's called in the origin thread,
  // |callback| can safely reference objects in the origin thread (i.e.
  // UI thread in most cases). If the caller is not interested in the response
  // from the method (i.e. calling a method that does not return a value),
  // EmptyResponseCallback() can be passed to the |callback| parameter.
  //
  // If the method call is successful, a pointer to Response object will
  // be passed to the callback. If unsuccessful, the error callback will be
  // called and a pointer to ErrorResponse object will be passed to the error
  // callback if available, otherwise NULL will be passed.
  //
  // Must be called in the origin thread.
  virtual void CallMethodWithErrorCallback(MethodCall* method_call,
                                           int timeout_ms,
                                           ResponseCallback callback,
                                           ErrorCallback error_callback);

  // Requests to connect to the signal from the remote object, replacing
  // any previous |signal_callback| connected to that signal.
  //
  // |signal_callback| will be called in the origin thread, when the
  // signal is received from the remote object. As it's called in the
  // origin thread, |signal_callback| can safely reference objects in the
  // origin thread (i.e. UI thread in most cases).
  //
  // |on_connected_callback| is called when the object proxy is connected
  // to the signal, or failed to be connected, in the origin thread.
  //
  // Must be called in the origin thread.
  virtual void ConnectToSignal(const std::string& interface_name,
                               const std::string& signal_name,
                               SignalCallback signal_callback,
                               OnConnectedCallback on_connected_callback);

  // Sets a callback for "NameOwnerChanged" signal. The callback is called on
  // the origin thread when D-Bus system sends "NameOwnerChanged" for the name
  // represented by |service_name_|.
  virtual void SetNameOwnerChangedCallback(NameOwnerChangedCallback callback);

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback);

  // Detaches from the remote object. The Bus object will take care of
  // detaching so you don't have to do this manually.
  //
  // BLOCKING CALL.
  virtual void Detach();

  const ObjectPath& object_path() const { return object_path_; }

  // Returns an empty callback that does nothing. Can be used for
  // CallMethod().
  static ResponseCallback EmptyResponseCallback();

 protected:
  // This is protected, so we can define sub classes.
  virtual ~ObjectProxy();

 private:
  friend class base::RefCountedThreadSafe<ObjectProxy>;

  // Struct of data we'll be passing from StartAsyncMethodCall() to
  // OnPendingCallIsCompleteThunk().
  struct OnPendingCallIsCompleteData {
    OnPendingCallIsCompleteData(ObjectProxy* in_object_proxy,
                                ResponseCallback in_response_callback,
                                ErrorCallback error_callback,
                                base::TimeTicks start_time);
    ~OnPendingCallIsCompleteData();

    ObjectProxy* object_proxy;
    ResponseCallback response_callback;
    ErrorCallback error_callback;
    base::TimeTicks start_time;
  };

  // Starts the async method call. This is a helper function to implement
  // CallMethod().
  void StartAsyncMethodCall(int timeout_ms,
                            DBusMessage* request_message,
                            ResponseCallback response_callback,
                            ErrorCallback error_callback,
                            base::TimeTicks start_time);

  // Called when the pending call is complete.
  void OnPendingCallIsComplete(DBusPendingCall* pending_call,
                               ResponseCallback response_callback,
                               ErrorCallback error_callback,
                               base::TimeTicks start_time);

  // Runs the response callback with the given response object.
  void RunResponseCallback(ResponseCallback response_callback,
                           ErrorCallback error_callback,
                           base::TimeTicks start_time,
                           DBusMessage* response_message);

  // Redirects the function call to OnPendingCallIsComplete().
  static void OnPendingCallIsCompleteThunk(DBusPendingCall* pending_call,
                                           void* user_data);

  // Connects to NameOwnerChanged signal.
  bool ConnectToNameOwnerChangedSignal();

  // Helper function for ConnectToSignal().
  bool ConnectToSignalInternal(const std::string& interface_name,
                               const std::string& signal_name,
                               SignalCallback signal_callback);

  // Helper function for WaitForServiceToBeAvailable().
  void WaitForServiceToBeAvailableInternal();

  // Handles the incoming request messages and dispatches to the signal
  // callbacks.
  DBusHandlerResult HandleMessage(DBusConnection* connection,
                                  DBusMessage* raw_message);

  // Runs the method. Helper function for HandleMessage().
  void RunMethod(base::TimeTicks start_time,
                 std::vector<SignalCallback> signal_callbacks,
                 Signal* signal);

  // Redirects the function call to HandleMessage().
  static DBusHandlerResult HandleMessageThunk(DBusConnection* connection,
                                              DBusMessage* raw_message,
                                              void* user_data);

  // Helper method for logging response errors appropriately.
  void LogMethodCallFailure(const base::StringPiece& interface_name,
                            const base::StringPiece& method_name,
                            const base::StringPiece& error_name,
                            const base::StringPiece& error_message) const;

  // Used as ErrorCallback by CallMethod().
  void OnCallMethodError(const std::string& interface_name,
                         const std::string& method_name,
                         ResponseCallback response_callback,
                         ErrorResponse* error_response);

  // Adds the match rule to the bus and associate the callback with the signal.
  bool AddMatchRuleWithCallback(const std::string& match_rule,
                                const std::string& absolute_signal_name,
                                SignalCallback signal_callback);

  // Adds the match rule to the bus so that HandleMessage can see the signal.
  bool AddMatchRuleWithoutCallback(const std::string& match_rule,
                                   const std::string& absolute_signal_name);

  // Calls D-Bus's GetNameOwner method synchronously to update
  // |service_name_owner_| with the current owner of |service_name_|.
  //
  // BLOCKING CALL.
  void UpdateNameOwnerAndBlock();

  // Handles NameOwnerChanged signal from D-Bus's special message bus.
  DBusHandlerResult HandleNameOwnerChanged(scoped_ptr<dbus::Signal> signal);

  // Runs |name_owner_changed_callback_|.
  void RunNameOwnerChangedCallback(const std::string& old_owner,
                                   const std::string& new_owner);

  // Runs |wait_for_service_to_be_available_callbacks_|.
  void RunWaitForServiceToBeAvailableCallbacks(bool service_is_available);

  scoped_refptr<Bus> bus_;
  std::string service_name_;
  ObjectPath object_path_;

  // True if the message filter was added.
  bool filter_added_;

  // The method table where keys are absolute signal names (i.e. interface
  // name + signal name), and values are lists of the corresponding callbacks.
  typedef std::map<std::string, std::vector<SignalCallback> > MethodTable;
  MethodTable method_table_;

  // The callback called when NameOwnerChanged signal is received.
  NameOwnerChangedCallback name_owner_changed_callback_;

  // Called when the service becomes available.
  std::vector<WaitForServiceToBeAvailableCallback>
      wait_for_service_to_be_available_callbacks_;

  std::set<std::string> match_rules_;

  const bool ignore_service_unknown_errors_;

  // Known name owner of the well-known bus name represnted by |service_name_|.
  std::string service_name_owner_;

  DISALLOW_COPY_AND_ASSIGN(ObjectProxy);
};

}  // namespace dbus

#endif  // DBUS_OBJECT_PROXY_H_
