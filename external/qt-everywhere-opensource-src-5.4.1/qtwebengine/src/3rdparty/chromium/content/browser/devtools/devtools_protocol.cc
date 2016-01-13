// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_protocol.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"

namespace content {

namespace {

const char kIdParam[] = "id";
const char kMethodParam[] = "method";
const char kParamsParam[] = "params";
const char kResultParam[] = "result";
const char kErrorParam[] = "error";
const char kErrorCodeParam[] = "code";
const char kErrorMessageParam[] = "message";
const int kNoId = -1;

// JSON RPC 2.0 spec: http://www.jsonrpc.org/specification#error_object
enum Error {
  kErrorParseError = -32700,
  kErrorInvalidRequest = -32600,
  kErrorNoSuchMethod = -32601,
  kErrorInvalidParams = -32602,
  kErrorInternalError = -32603,
  kErrorServerError = -32000
};

}  // namespace

using base::Value;

DevToolsProtocol::Message::~Message() {
}

DevToolsProtocol::Message::Message(const std::string& method,
                                   base::DictionaryValue* params)
    : method_(method),
      params_(params) {
  size_t pos = method.find(".");
  if (pos != std::string::npos && pos > 0)
    domain_ = method.substr(0, pos);
}

DevToolsProtocol::Command::~Command() {
}

std::string DevToolsProtocol::Command::Serialize() {
  base::DictionaryValue command;
  command.SetInteger(kIdParam, id_);
  command.SetString(kMethodParam, method_);
  if (params_)
    command.Set(kParamsParam, params_->DeepCopy());

  std::string json_command;
  base::JSONWriter::Write(&command, &json_command);
  return json_command;
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsProtocol::Command::SuccessResponse(base::DictionaryValue* result) {
  return new DevToolsProtocol::Response(id_, result);
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsProtocol::Command::InternalErrorResponse(const std::string& message) {
  return new DevToolsProtocol::Response(id_, kErrorInternalError, message);
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsProtocol::Command::InvalidParamResponse(const std::string& param) {
  std::string message =
      base::StringPrintf("Missing or invalid '%s' parameter", param.c_str());
  return new DevToolsProtocol::Response(id_, kErrorInvalidParams, message);
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsProtocol::Command::NoSuchMethodErrorResponse() {
  return new Response(id_, kErrorNoSuchMethod, "No such method");
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsProtocol::Command::ServerErrorResponse(const std::string& message) {
  return new Response(id_, kErrorServerError, message);
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsProtocol::Command::AsyncResponsePromise() {
  scoped_refptr<DevToolsProtocol::Response> promise =
      new DevToolsProtocol::Response(0, NULL);
  promise->is_async_promise_ = true;
  return promise;
}

DevToolsProtocol::Command::Command(int id,
                                   const std::string& method,
                                   base::DictionaryValue* params)
    : Message(method, params),
      id_(id) {
}

DevToolsProtocol::Response::~Response() {
}

std::string DevToolsProtocol::Response::Serialize() {
  base::DictionaryValue response;

  if (id_ != kNoId)
    response.SetInteger(kIdParam, id_);

  if (error_code_) {
    base::DictionaryValue* error_object = new base::DictionaryValue();
    response.Set(kErrorParam, error_object);
    error_object->SetInteger(kErrorCodeParam, error_code_);
    if (!error_message_.empty())
      error_object->SetString(kErrorMessageParam, error_message_);
  } else if (result_) {
    response.Set(kResultParam, result_->DeepCopy());
  }

  std::string json_response;
  base::JSONWriter::Write(&response, &json_response);
  return json_response;
}

DevToolsProtocol::Response::Response(int id, base::DictionaryValue* result)
    : id_(id),
      result_(result),
      error_code_(0),
      is_async_promise_(false) {
}

DevToolsProtocol::Response::Response(int id,
                                     int error_code,
                                     const std::string& error_message)
    : id_(id),
      error_code_(error_code),
      error_message_(error_message),
      is_async_promise_(false) {
}

DevToolsProtocol::Notification::Notification(const std::string& method,
                                             base::DictionaryValue* params)
    : Message(method, params) {
}

DevToolsProtocol::Notification::~Notification() {
}

std::string DevToolsProtocol::Notification::Serialize() {
  base::DictionaryValue notification;
  notification.SetString(kMethodParam, method_);
  if (params_)
    notification.Set(kParamsParam, params_->DeepCopy());

  std::string json_notification;
  base::JSONWriter::Write(&notification, &json_notification);
  return json_notification;
}

DevToolsProtocol::Handler::~Handler() {
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsProtocol::Handler::HandleCommand(
    scoped_refptr<DevToolsProtocol::Command> command) {
  CommandHandlers::iterator it = command_handlers_.find(command->method());
  if (it == command_handlers_.end())
    return NULL;
  return (it->second).Run(command);
}

void DevToolsProtocol::Handler::SetNotifier(const Notifier& notifier) {
  notifier_ = notifier;
}

DevToolsProtocol::Handler::Handler() {
}

void DevToolsProtocol::Handler::RegisterCommandHandler(
    const std::string& command,
    const CommandHandler& handler) {
  command_handlers_[command] = handler;
}

void DevToolsProtocol::Handler::SendNotification(
    const std::string& method,
    base::DictionaryValue* params) {
  scoped_refptr<DevToolsProtocol::Notification> notification =
      new DevToolsProtocol::Notification(method, params);
  SendRawMessage(notification->Serialize());
}

void DevToolsProtocol::Handler::SendAsyncResponse(
    scoped_refptr<DevToolsProtocol::Response> response) {
  SendRawMessage(response->Serialize());
}

void DevToolsProtocol::Handler::SendRawMessage(const std::string& message) {
  if (!notifier_.is_null())
    notifier_.Run(message);
}

static bool ParseMethod(base::DictionaryValue* command,
                        std::string* method) {
  if (!command->GetString(kMethodParam, method))
    return false;
  size_t pos = method->find(".");
  if (pos == std::string::npos || pos == 0)
    return false;
  return true;
}

// static
scoped_refptr<DevToolsProtocol::Command> DevToolsProtocol::ParseCommand(
    const std::string& json,
    std::string* error_response) {
  scoped_ptr<base::DictionaryValue> command_dict(
      ParseMessage(json, error_response));
  return ParseCommand(command_dict.get(), error_response);
}

// static
scoped_refptr<DevToolsProtocol::Command> DevToolsProtocol::ParseCommand(
    base::DictionaryValue* command_dict,
    std::string* error_response) {
  if (!command_dict)
    return NULL;

  int id;
  std::string method;
  bool ok = command_dict->GetInteger(kIdParam, &id) && id >= 0;
  ok = ok && ParseMethod(command_dict, &method);
  if (!ok) {
    scoped_refptr<Response> response =
        new Response(kNoId, kErrorInvalidRequest, "No such method");
    *error_response = response->Serialize();
    return NULL;
  }

  base::DictionaryValue* params = NULL;
  command_dict->GetDictionary(kParamsParam, &params);
  return new Command(id, method, params ? params->DeepCopy() : NULL);
}

// static
scoped_refptr<DevToolsProtocol::Command>
DevToolsProtocol::CreateCommand(
    int id,
    const std::string& method,
    base::DictionaryValue* params) {
  return new Command(id, method, params);
}

//static
scoped_refptr<DevToolsProtocol::Response>
DevToolsProtocol::ParseResponse(
    base::DictionaryValue* response_dict) {
  int id;
  if (!response_dict->GetInteger(kIdParam, &id))
    id = kNoId;

  const base::DictionaryValue* error_dict;
  if (response_dict->GetDictionary(kErrorParam, &error_dict)) {
    int error_code = kErrorInternalError;
    response_dict->GetInteger(kErrorCodeParam, &error_code);
    std::string error_message;
    response_dict->GetString(kErrorMessageParam, &error_message);
    return new Response(id, error_code, error_message);
  }

  const base::DictionaryValue* result = NULL;
  response_dict->GetDictionary(kResultParam, &result);
  return new Response(id, result ? result->DeepCopy() : NULL);
}

// static
scoped_refptr<DevToolsProtocol::Notification>
DevToolsProtocol::ParseNotification(const std::string& json) {
  scoped_ptr<base::DictionaryValue> dict(ParseMessage(json, NULL));
  if (!dict)
    return NULL;

  std::string method;
  bool ok = ParseMethod(dict.get(), &method);
  if (!ok)
    return NULL;

  base::DictionaryValue* params = NULL;
  dict->GetDictionary(kParamsParam, &params);
  return new Notification(method, params ? params->DeepCopy() : NULL);
}

//static
scoped_refptr<DevToolsProtocol::Notification>
DevToolsProtocol::CreateNotification(
    const std::string& method,
    base::DictionaryValue* params) {
  return new Notification(method, params);
}

// static
base::DictionaryValue* DevToolsProtocol::ParseMessage(
    const std::string& json,
    std::string* error_response) {
  int parse_error_code;
  std::string error_message;
  scoped_ptr<base::Value> message(
      base::JSONReader::ReadAndReturnError(
          json, 0, &parse_error_code, &error_message));

  if (!message || !message->IsType(base::Value::TYPE_DICTIONARY)) {
    scoped_refptr<Response> response =
        new Response(0, kErrorParseError, error_message);
    if (error_response)
      *error_response = response->Serialize();
    return NULL;
  }

  return static_cast<base::DictionaryValue*>(message.release());
}

}  // namespace content
