// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/object_proxy.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "dbus/bus.h"
#include "dbus/dbus_statistics.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/scoped_dbus_error.h"
#include "dbus/util.h"

namespace dbus {

namespace {

const char kErrorServiceUnknown[] = "org.freedesktop.DBus.Error.ServiceUnknown";
const char kErrorObjectUnknown[] = "org.freedesktop.DBus.Error.UnknownObject";

// Used for success ratio histograms. 1 for success, 0 for failure.
const int kSuccessRatioHistogramMaxValue = 2;

// The path of D-Bus Object sending NameOwnerChanged signal.
const char kDBusSystemObjectPath[] = "/org/freedesktop/DBus";

// The D-Bus Object interface.
const char kDBusSystemObjectInterface[] = "org.freedesktop.DBus";

// The D-Bus Object address.
const char kDBusSystemObjectAddress[] = "org.freedesktop.DBus";

// The NameOwnerChanged member in |kDBusSystemObjectInterface|.
const char kNameOwnerChangedMember[] = "NameOwnerChanged";

// An empty function used for ObjectProxy::EmptyResponseCallback().
void EmptyResponseCallbackBody(Response* /*response*/) {
}

}  // namespace

ObjectProxy::ObjectProxy(Bus* bus,
                         const std::string& service_name,
                         const ObjectPath& object_path,
                         int options)
    : bus_(bus),
      service_name_(service_name),
      object_path_(object_path),
      ignore_service_unknown_errors_(
          options & IGNORE_SERVICE_UNKNOWN_ERRORS) {
}

ObjectProxy::~ObjectProxy() {
  DCHECK(pending_calls_.empty());
}

// Originally we tried to make |method_call| a const reference, but we
// gave up as dbus_connection_send_with_reply_and_block() takes a
// non-const pointer of DBusMessage as the second parameter.
std::unique_ptr<Response> ObjectProxy::CallMethodAndBlockWithErrorDetails(
    MethodCall* method_call,
    int timeout_ms,
    ScopedDBusError* error) {
  bus_->AssertOnDBusThread();

  if (!bus_->Connect() ||
      !method_call->SetDestination(service_name_) ||
      !method_call->SetPath(object_path_))
    return std::unique_ptr<Response>();

  DBusMessage* request_message = method_call->raw_message();

  // Send the message synchronously.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  DBusMessage* response_message =
      bus_->SendWithReplyAndBlock(request_message, timeout_ms, error->get());
  // Record if the method call is successful, or not. 1 if successful.
  UMA_HISTOGRAM_ENUMERATION("DBus.SyncMethodCallSuccess",
                            response_message ? 1 : 0,
                            kSuccessRatioHistogramMaxValue);
  statistics::AddBlockingSentMethodCall(service_name_,
                                        method_call->GetInterface(),
                                        method_call->GetMember());

  if (!response_message) {
    LogMethodCallFailure(method_call->GetInterface(),
                         method_call->GetMember(),
                         error->is_set() ? error->name() : "unknown error type",
                         error->is_set() ? error->message() : "");
    return std::unique_ptr<Response>();
  }
  // Record time spent for the method call. Don't include failures.
  UMA_HISTOGRAM_TIMES("DBus.SyncMethodCallTime",
                      base::TimeTicks::Now() - start_time);

  return Response::FromRawMessage(response_message);
}

std::unique_ptr<Response> ObjectProxy::CallMethodAndBlock(
    MethodCall* method_call,
    int timeout_ms) {
  ScopedDBusError error;
  return CallMethodAndBlockWithErrorDetails(method_call, timeout_ms, &error);
}

void ObjectProxy::CallMethod(MethodCall* method_call,
                             int timeout_ms,
                             ResponseCallback callback) {
  CallMethodWithErrorCallback(method_call, timeout_ms, callback,
                              base::Bind(&ObjectProxy::OnCallMethodError,
                                         this,
                                         method_call->GetInterface(),
                                         method_call->GetMember(),
                                         callback));
}

void ObjectProxy::CallMethodWithErrorCallback(MethodCall* method_call,
                                              int timeout_ms,
                                              ResponseCallback callback,
                                              ErrorCallback error_callback) {
  bus_->AssertOnOriginThread();

  const base::TimeTicks start_time = base::TimeTicks::Now();

  if (!method_call->SetDestination(service_name_) ||
      !method_call->SetPath(object_path_)) {
    // In case of a failure, run the error callback with NULL.
    DBusMessage* response_message = NULL;
    base::Closure task = base::Bind(&ObjectProxy::RunResponseCallback,
                                    this,
                                    callback,
                                    error_callback,
                                    start_time,
                                    response_message);
    bus_->GetOriginTaskRunner()->PostTask(FROM_HERE, task);
    return;
  }

  // Increment the reference count so we can safely reference the
  // underlying request message until the method call is complete. This
  // will be unref'ed in StartAsyncMethodCall().
  DBusMessage* request_message = method_call->raw_message();
  dbus_message_ref(request_message);

  base::Closure task = base::Bind(&ObjectProxy::StartAsyncMethodCall,
                                  this,
                                  timeout_ms,
                                  request_message,
                                  callback,
                                  error_callback,
                                  start_time);
  statistics::AddSentMethodCall(service_name_,
                                method_call->GetInterface(),
                                method_call->GetMember());

  // Wait for the response in the D-Bus thread.
  bus_->GetDBusTaskRunner()->PostTask(FROM_HERE, task);
}

void ObjectProxy::ConnectToSignal(const std::string& interface_name,
                                  const std::string& signal_name,
                                  SignalCallback signal_callback,
                                  OnConnectedCallback on_connected_callback) {
  bus_->AssertOnOriginThread();

  if (bus_->HasDBusThread()) {
    base::PostTaskAndReplyWithResult(
        bus_->GetDBusTaskRunner(), FROM_HERE,
        base::Bind(&ObjectProxy::ConnectToSignalInternal, this, interface_name,
                   signal_name, signal_callback),
        base::Bind(on_connected_callback, interface_name, signal_name));
  } else {
    // If the bus doesn't have a dedicated dbus thread we need to call
    // ConnectToSignalInternal directly otherwise we might miss a signal
    // that is currently queued if we do a PostTask.
    const bool success =
        ConnectToSignalInternal(interface_name, signal_name, signal_callback);
    on_connected_callback.Run(interface_name, signal_name, success);
  }
}

void ObjectProxy::SetNameOwnerChangedCallback(
    NameOwnerChangedCallback callback) {
  bus_->AssertOnOriginThread();

  name_owner_changed_callback_ = callback;
}

void ObjectProxy::WaitForServiceToBeAvailable(
    WaitForServiceToBeAvailableCallback callback) {
  bus_->AssertOnOriginThread();

  wait_for_service_to_be_available_callbacks_.push_back(callback);
  bus_->GetDBusTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ObjectProxy::WaitForServiceToBeAvailableInternal, this));
}

void ObjectProxy::Detach() {
  bus_->AssertOnDBusThread();

  if (bus_->is_connected())
    bus_->RemoveFilterFunction(&ObjectProxy::HandleMessageThunk, this);

  for (const auto& match_rule : match_rules_) {
    ScopedDBusError error;
    bus_->RemoveMatch(match_rule, error.get());
    if (error.is_set()) {
      // There is nothing we can do to recover, so just print the error.
      LOG(ERROR) << "Failed to remove match rule: " << match_rule;
    }
  }
  match_rules_.clear();

  for (auto* pending_call : pending_calls_) {
    dbus_pending_call_cancel(pending_call);
    dbus_pending_call_unref(pending_call);
  }
  pending_calls_.clear();
}

// static
ObjectProxy::ResponseCallback ObjectProxy::EmptyResponseCallback() {
  return base::Bind(&EmptyResponseCallbackBody);
}

ObjectProxy::OnPendingCallIsCompleteData::OnPendingCallIsCompleteData(
    ObjectProxy* in_object_proxy,
    ResponseCallback in_response_callback,
    ErrorCallback in_error_callback,
    base::TimeTicks in_start_time)
    : object_proxy(in_object_proxy),
      response_callback(in_response_callback),
      error_callback(in_error_callback),
      start_time(in_start_time) {
}

ObjectProxy::OnPendingCallIsCompleteData::~OnPendingCallIsCompleteData() {
}

void ObjectProxy::StartAsyncMethodCall(int timeout_ms,
                                       DBusMessage* request_message,
                                       ResponseCallback response_callback,
                                       ErrorCallback error_callback,
                                       base::TimeTicks start_time) {
  bus_->AssertOnDBusThread();

  if (!bus_->Connect() || !bus_->SetUpAsyncOperations()) {
    // In case of a failure, run the error callback with NULL.
    DBusMessage* response_message = NULL;
    base::Closure task = base::Bind(&ObjectProxy::RunResponseCallback,
                                    this,
                                    response_callback,
                                    error_callback,
                                    start_time,
                                    response_message);
    bus_->GetOriginTaskRunner()->PostTask(FROM_HERE, task);

    dbus_message_unref(request_message);
    return;
  }

  DBusPendingCall* pending_call = NULL;

  bus_->SendWithReply(request_message, &pending_call, timeout_ms);

  // Prepare the data we'll be passing to OnPendingCallIsCompleteThunk().
  // The data will be deleted in OnPendingCallIsCompleteThunk().
  OnPendingCallIsCompleteData* data =
      new OnPendingCallIsCompleteData(this, response_callback, error_callback,
                                      start_time);

  // This returns false only when unable to allocate memory.
  const bool success = dbus_pending_call_set_notify(
      pending_call,
      &ObjectProxy::OnPendingCallIsCompleteThunk,
      data,
      &DeleteVoidPointer<OnPendingCallIsCompleteData>);
  CHECK(success) << "Unable to allocate memory";
  pending_calls_.insert(pending_call);

  // It's now safe to unref the request message.
  dbus_message_unref(request_message);
}

void ObjectProxy::OnPendingCallIsComplete(DBusPendingCall* pending_call,
                                          ResponseCallback response_callback,
                                          ErrorCallback error_callback,
                                          base::TimeTicks start_time) {
  bus_->AssertOnDBusThread();

  DBusMessage* response_message = dbus_pending_call_steal_reply(pending_call);
  base::Closure task = base::Bind(&ObjectProxy::RunResponseCallback,
                                  this,
                                  response_callback,
                                  error_callback,
                                  start_time,
                                  response_message);
  bus_->GetOriginTaskRunner()->PostTask(FROM_HERE, task);

  // Remove the pending call from the set.
  pending_calls_.erase(pending_call);
  dbus_pending_call_unref(pending_call);
}

void ObjectProxy::RunResponseCallback(ResponseCallback response_callback,
                                      ErrorCallback error_callback,
                                      base::TimeTicks start_time,
                                      DBusMessage* response_message) {
  bus_->AssertOnOriginThread();

  bool method_call_successful = false;
  if (!response_message) {
    // The response is not received.
    error_callback.Run(NULL);
  } else if (dbus_message_get_type(response_message) ==
             DBUS_MESSAGE_TYPE_ERROR) {
    // This will take |response_message| and release (unref) it.
    std::unique_ptr<ErrorResponse> error_response(
        ErrorResponse::FromRawMessage(response_message));
    error_callback.Run(error_response.get());
    // Delete the message  on the D-Bus thread. See below for why.
    bus_->GetDBusTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&base::DeletePointer<ErrorResponse>,
                   error_response.release()));
  } else {
    // This will take |response_message| and release (unref) it.
    std::unique_ptr<Response> response(
        Response::FromRawMessage(response_message));
    // The response is successfully received.
    response_callback.Run(response.get());
    // The message should be deleted on the D-Bus thread for a complicated
    // reason:
    //
    // libdbus keeps track of the number of bytes in the incoming message
    // queue to ensure that the data size in the queue is manageable. The
    // bookkeeping is partly done via dbus_message_unref(), and immediately
    // asks the client code (Chrome) to stop monitoring the underlying
    // socket, if the number of bytes exceeds a certian number, which is set
    // to 63MB, per dbus-transport.cc:
    //
    //   /* Try to default to something that won't totally hose the system,
    //    * but doesn't impose too much of a limitation.
    //    */
    //   transport->max_live_messages_size = _DBUS_ONE_MEGABYTE * 63;
    //
    // The monitoring of the socket is done on the D-Bus thread (see Watch
    // class in bus.cc), hence we should stop the monitoring from D-Bus
    // thread, not from the current thread here, which is likely UI thread.
    bus_->GetDBusTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&base::DeletePointer<Response>, response.release()));

    method_call_successful = true;
    // Record time spent for the method call. Don't include failures.
    UMA_HISTOGRAM_TIMES("DBus.AsyncMethodCallTime",
                        base::TimeTicks::Now() - start_time);
  }
  // Record if the method call is successful, or not. 1 if successful.
  UMA_HISTOGRAM_ENUMERATION("DBus.AsyncMethodCallSuccess",
                            method_call_successful,
                            kSuccessRatioHistogramMaxValue);
}

void ObjectProxy::OnPendingCallIsCompleteThunk(DBusPendingCall* pending_call,
                                               void* user_data) {
  OnPendingCallIsCompleteData* data =
      reinterpret_cast<OnPendingCallIsCompleteData*>(user_data);
  ObjectProxy* self = data->object_proxy;
  self->OnPendingCallIsComplete(pending_call,
                                data->response_callback,
                                data->error_callback,
                                data->start_time);
}

bool ObjectProxy::ConnectToNameOwnerChangedSignal() {
  bus_->AssertOnDBusThread();

  if (!bus_->Connect() || !bus_->SetUpAsyncOperations())
    return false;

  bus_->AddFilterFunction(&ObjectProxy::HandleMessageThunk, this);

  // Add a match_rule listening NameOwnerChanged for the well-known name
  // |service_name_|.
  const std::string name_owner_changed_match_rule =
      base::StringPrintf(
          "type='signal',interface='org.freedesktop.DBus',"
          "member='NameOwnerChanged',path='/org/freedesktop/DBus',"
          "sender='org.freedesktop.DBus',arg0='%s'",
          service_name_.c_str());

  const bool success =
      AddMatchRuleWithoutCallback(name_owner_changed_match_rule,
                                  "org.freedesktop.DBus.NameOwnerChanged");

  // Try getting the current name owner. It's not guaranteed that we can get
  // the name owner at this moment, as the service may not yet be started. If
  // that's the case, we'll get the name owner via NameOwnerChanged signal,
  // as soon as the service is started.
  UpdateNameOwnerAndBlock();

  return success;
}

bool ObjectProxy::ConnectToSignalInternal(const std::string& interface_name,
                                          const std::string& signal_name,
                                          SignalCallback signal_callback) {
  bus_->AssertOnDBusThread();

  if (!ConnectToNameOwnerChangedSignal())
    return false;

  const std::string absolute_signal_name =
      GetAbsoluteMemberName(interface_name, signal_name);

  // Add a match rule so the signal goes through HandleMessage().
  const std::string match_rule =
      base::StringPrintf("type='signal', interface='%s', path='%s'",
                         interface_name.c_str(),
                         object_path_.value().c_str());
  return AddMatchRuleWithCallback(match_rule,
                                  absolute_signal_name,
                                  signal_callback);
}

void ObjectProxy::WaitForServiceToBeAvailableInternal() {
  bus_->AssertOnDBusThread();

  if (!ConnectToNameOwnerChangedSignal()) {  // Failed to connect to the signal.
    const bool service_is_ready = false;
    bus_->GetOriginTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ObjectProxy::RunWaitForServiceToBeAvailableCallbacks,
                   this, service_is_ready));
    return;
  }

  const bool service_is_available = !service_name_owner_.empty();
  if (service_is_available) {  // Service is already available.
    bus_->GetOriginTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ObjectProxy::RunWaitForServiceToBeAvailableCallbacks,
                   this, service_is_available));
    return;
  }
}

DBusHandlerResult ObjectProxy::HandleMessage(
    DBusConnection* connection,
    DBusMessage* raw_message) {
  bus_->AssertOnDBusThread();

  if (dbus_message_get_type(raw_message) != DBUS_MESSAGE_TYPE_SIGNAL)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  // raw_message will be unrefed on exit of the function. Increment the
  // reference so we can use it in Signal.
  dbus_message_ref(raw_message);
  std::unique_ptr<Signal> signal(Signal::FromRawMessage(raw_message));

  // Verify the signal comes from the object we're proxying for, this is
  // our last chance to return DBUS_HANDLER_RESULT_NOT_YET_HANDLED and
  // allow other object proxies to handle instead.
  const ObjectPath path = signal->GetPath();
  if (path != object_path_) {
    if (path.value() == kDBusSystemObjectPath &&
        signal->GetMember() == kNameOwnerChangedMember) {
      // Handle NameOwnerChanged separately
      return HandleNameOwnerChanged(std::move(signal));
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  const std::string interface = signal->GetInterface();
  const std::string member = signal->GetMember();

  statistics::AddReceivedSignal(service_name_, interface, member);

  // Check if we know about the signal.
  const std::string absolute_signal_name = GetAbsoluteMemberName(
      interface, member);
  MethodTable::const_iterator iter = method_table_.find(absolute_signal_name);
  if (iter == method_table_.end()) {
    // Don't know about the signal.
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  VLOG(1) << "Signal received: " << signal->ToString();

  std::string sender = signal->GetSender();
  if (service_name_owner_ != sender) {
    LOG(ERROR) << "Rejecting a message from a wrong sender.";
    UMA_HISTOGRAM_COUNTS("DBus.RejectedSignalCount", 1);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  const base::TimeTicks start_time = base::TimeTicks::Now();
  if (bus_->HasDBusThread()) {
    // Post a task to run the method in the origin thread.
    // Transfer the ownership of |signal| to RunMethod().
    // |released_signal| will be deleted in RunMethod().
    Signal* released_signal = signal.release();
    bus_->GetOriginTaskRunner()->PostTask(FROM_HERE,
                                          base::Bind(&ObjectProxy::RunMethod,
                                                     this,
                                                     start_time,
                                                     iter->second,
                                                     released_signal));
  } else {
    const base::TimeTicks start_time = base::TimeTicks::Now();
    // If the D-Bus thread is not used, just call the callback on the
    // current thread. Transfer the ownership of |signal| to RunMethod().
    Signal* released_signal = signal.release();
    RunMethod(start_time, iter->second, released_signal);
  }

  // We don't return DBUS_HANDLER_RESULT_HANDLED for signals because other
  // objects may be interested in them. (e.g. Signals from org.freedesktop.DBus)
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void ObjectProxy::RunMethod(base::TimeTicks start_time,
                            std::vector<SignalCallback> signal_callbacks,
                            Signal* signal) {
  bus_->AssertOnOriginThread();

  for (std::vector<SignalCallback>::iterator iter = signal_callbacks.begin();
       iter != signal_callbacks.end(); ++iter)
    iter->Run(signal);

  // Delete the message on the D-Bus thread. See comments in
  // RunResponseCallback().
  bus_->GetDBusTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&base::DeletePointer<Signal>, signal));

  // Record time spent for handling the signal.
  UMA_HISTOGRAM_TIMES("DBus.SignalHandleTime",
                      base::TimeTicks::Now() - start_time);
}

DBusHandlerResult ObjectProxy::HandleMessageThunk(
    DBusConnection* connection,
    DBusMessage* raw_message,
    void* user_data) {
  ObjectProxy* self = reinterpret_cast<ObjectProxy*>(user_data);
  return self->HandleMessage(connection, raw_message);
}

void ObjectProxy::LogMethodCallFailure(
    const base::StringPiece& interface_name,
    const base::StringPiece& method_name,
    const base::StringPiece& error_name,
    const base::StringPiece& error_message) const {
  if (ignore_service_unknown_errors_ &&
      (error_name == kErrorServiceUnknown || error_name == kErrorObjectUnknown))
    return;

  std::ostringstream msg;
  msg << "Failed to call method: " << interface_name << "." << method_name
      << ": object_path= " << object_path_.value()
      << ": " << error_name << ": " << error_message;

  // "UnknownObject" indicates that an object or service is no longer available,
  // e.g. a Shill network service has gone out of range. Treat these as warnings
  // not errors.
  if (error_name == kErrorObjectUnknown)
    LOG(WARNING) << msg.str();
  else
    LOG(ERROR) << msg.str();
}

void ObjectProxy::OnCallMethodError(const std::string& interface_name,
                                    const std::string& method_name,
                                    ResponseCallback response_callback,
                                    ErrorResponse* error_response) {
  if (error_response) {
    // Error message may contain the error message as string.
    MessageReader reader(error_response);
    std::string error_message;
    reader.PopString(&error_message);
    LogMethodCallFailure(interface_name,
                         method_name,
                         error_response->GetErrorName(),
                         error_message);
  }
  response_callback.Run(NULL);
}

bool ObjectProxy::AddMatchRuleWithCallback(
    const std::string& match_rule,
    const std::string& absolute_signal_name,
    SignalCallback signal_callback) {
  DCHECK(!match_rule.empty());
  DCHECK(!absolute_signal_name.empty());
  bus_->AssertOnDBusThread();

  if (match_rules_.find(match_rule) == match_rules_.end()) {
    ScopedDBusError error;
    bus_->AddMatch(match_rule, error.get());
    if (error.is_set()) {
      LOG(ERROR) << "Failed to add match rule \"" << match_rule << "\". Got "
                 << error.name() << ": " << error.message();
      return false;
    } else {
      // Store the match rule, so that we can remove this in Detach().
      match_rules_.insert(match_rule);
      // Add the signal callback to the method table.
      method_table_[absolute_signal_name].push_back(signal_callback);
      return true;
    }
  } else {
    // We already have the match rule.
    method_table_[absolute_signal_name].push_back(signal_callback);
    return true;
  }
}

bool ObjectProxy::AddMatchRuleWithoutCallback(
    const std::string& match_rule,
    const std::string& absolute_signal_name) {
  DCHECK(!match_rule.empty());
  DCHECK(!absolute_signal_name.empty());
  bus_->AssertOnDBusThread();

  if (match_rules_.find(match_rule) != match_rules_.end())
    return true;

  ScopedDBusError error;
  bus_->AddMatch(match_rule, error.get());
  if (error.is_set()) {
    LOG(ERROR) << "Failed to add match rule \"" << match_rule << "\". Got "
               << error.name() << ": " << error.message();
    return false;
  }
  // Store the match rule, so that we can remove this in Detach().
  match_rules_.insert(match_rule);
  return true;
}

void ObjectProxy::UpdateNameOwnerAndBlock() {
  bus_->AssertOnDBusThread();
  // Errors should be suppressed here, as the service may not be yet running
  // when connecting to signals of the service, which is just fine.
  // The ObjectProxy will be notified when the service is launched via
  // NameOwnerChanged signal. See also comments in ConnectToSignalInternal().
  service_name_owner_ =
      bus_->GetServiceOwnerAndBlock(service_name_, Bus::SUPPRESS_ERRORS);
}

DBusHandlerResult ObjectProxy::HandleNameOwnerChanged(
    std::unique_ptr<Signal> signal) {
  DCHECK(signal);
  bus_->AssertOnDBusThread();

  // Confirm the validity of the NameOwnerChanged signal.
  if (signal->GetMember() == kNameOwnerChangedMember &&
      signal->GetInterface() == kDBusSystemObjectInterface &&
      signal->GetSender() == kDBusSystemObjectAddress) {
    MessageReader reader(signal.get());
    std::string name, old_owner, new_owner;
    if (reader.PopString(&name) &&
        reader.PopString(&old_owner) &&
        reader.PopString(&new_owner) &&
        name == service_name_) {
      service_name_owner_ = new_owner;
      bus_->GetOriginTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(&ObjectProxy::RunNameOwnerChangedCallback,
                     this, old_owner, new_owner));

      const bool service_is_available = !service_name_owner_.empty();
      if (service_is_available) {
        bus_->GetOriginTaskRunner()->PostTask(
            FROM_HERE,
            base::Bind(&ObjectProxy::RunWaitForServiceToBeAvailableCallbacks,
                       this, service_is_available));
      }
    }
  }

  // Always return unhandled to let other object proxies handle the same
  // signal.
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void ObjectProxy::RunNameOwnerChangedCallback(const std::string& old_owner,
                                              const std::string& new_owner) {
  bus_->AssertOnOriginThread();
  if (!name_owner_changed_callback_.is_null())
    name_owner_changed_callback_.Run(old_owner, new_owner);
}

void ObjectProxy::RunWaitForServiceToBeAvailableCallbacks(
    bool service_is_available) {
  bus_->AssertOnOriginThread();

  std::vector<WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(wait_for_service_to_be_available_callbacks_);
  for (size_t i = 0; i < callbacks.size(); ++i)
    callbacks[i].Run(service_is_available);
}

}  // namespace dbus
