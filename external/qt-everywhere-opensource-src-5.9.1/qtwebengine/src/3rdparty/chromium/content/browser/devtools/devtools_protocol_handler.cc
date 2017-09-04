// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_protocol_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {

namespace {

const char kIdParam[] = "id";
const char kMethodParam[] = "method";
const char kParamsParam[] = "params";

// JSON RPC 2.0 spec: http://www.jsonrpc.org/specification#error_object
const int kStatusParseError = -32700;
const int kStatusInvalidRequest = -32600;
const int kStatusNoSuchMethod = -32601;

std::unique_ptr<base::DictionaryValue> TakeDictionary(
    base::DictionaryValue* dict,
    const std::string& key) {
  std::unique_ptr<base::Value> value;
  dict->Remove(key, &value);
  base::DictionaryValue* result = nullptr;
  if (value)
    value.release()->GetAsDictionary(&result);
  return base::WrapUnique(result);
}

}  // namespace

DevToolsProtocolHandler::DevToolsProtocolHandler(
    DevToolsAgentHostImpl* agent_host)
    : agent_host_(agent_host), client_(agent_host), dispatcher_(agent_host) {}

DevToolsProtocolHandler::~DevToolsProtocolHandler() {
}

void DevToolsProtocolHandler::HandleMessage(int session_id,
                                            const std::string& message) {
  std::unique_ptr<base::DictionaryValue> command =
      ParseCommand(session_id, message);
  if (!command)
    return;
  if (PassCommandToDelegate(session_id, command.get()))
    return;
  HandleCommand(session_id, std::move(command));
}

bool DevToolsProtocolHandler::HandleOptionalMessage(int session_id,
                                                    const std::string& message,
                                                    int* call_id,
                                                    std::string* method) {
  std::unique_ptr<base::DictionaryValue> command =
      ParseCommand(session_id, message);
  if (!command)
    return true;
  if (PassCommandToDelegate(session_id, command.get()))
    return true;
  return HandleOptionalCommand(session_id, std::move(command), call_id, method);
}

bool DevToolsProtocolHandler::PassCommandToDelegate(
    int session_id,
    base::DictionaryValue* command) {
  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (!delegate)
    return false;

  std::unique_ptr<base::DictionaryValue> response(
      delegate->HandleCommand(agent_host_, command));
  if (response) {
    client_.SendMessage(session_id, *response);
    return true;
  }

  return false;
}

std::unique_ptr<base::DictionaryValue> DevToolsProtocolHandler::ParseCommand(
    int session_id,
    const std::string& message) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(message);
  if (!value || !value->IsType(base::Value::TYPE_DICTIONARY)) {
    client_.SendError(
        DevToolsCommandId(DevToolsCommandId::kNoId, session_id),
        Response(kStatusParseError, "Message must be in JSON format"));
    return nullptr;
  }

  std::unique_ptr<base::DictionaryValue> command =
      base::WrapUnique(static_cast<base::DictionaryValue*>(value.release()));
  int call_id = DevToolsCommandId::kNoId;
  bool ok = command->GetInteger(kIdParam, &call_id) && call_id >= 0;
  if (!ok) {
    client_.SendError(DevToolsCommandId(call_id, session_id),
                      Response(kStatusInvalidRequest,
                               "The type of 'id' property must be number"));
    return nullptr;
  }

  std::string method;
  ok = command->GetString(kMethodParam, &method);
  if (!ok) {
    client_.SendError(DevToolsCommandId(call_id, session_id),
                      Response(kStatusInvalidRequest,
                               "The type of 'method' property must be string"));
    return nullptr;
  }

  return command;
}

void DevToolsProtocolHandler::HandleCommand(
    int session_id,
    std::unique_ptr<base::DictionaryValue> command) {
  int call_id = DevToolsCommandId::kNoId;
  std::string method;
  command->GetInteger(kIdParam, &call_id);
  command->GetString(kMethodParam, &method);
  DevToolsProtocolDispatcher::CommandHandler command_handler(
      dispatcher_.FindCommandHandler(method));
  if (command_handler.is_null()) {
    client_.SendError(DevToolsCommandId(call_id, session_id),
                      Response(kStatusNoSuchMethod, "No such method"));
    return;
  }

  bool result =
      command_handler.Run(DevToolsCommandId(call_id, session_id),
                          TakeDictionary(command.get(), kParamsParam));
  DCHECK(result);
}

bool DevToolsProtocolHandler::HandleOptionalCommand(
    int session_id,
    std::unique_ptr<base::DictionaryValue> command,
    int* call_id,
    std::string* method) {
  *call_id = DevToolsCommandId::kNoId;
  command->GetInteger(kIdParam, call_id);
  command->GetString(kMethodParam, method);
  DevToolsProtocolDispatcher::CommandHandler command_handler(
      dispatcher_.FindCommandHandler(*method));
  if (!command_handler.is_null()) {
    return command_handler.Run(DevToolsCommandId(*call_id, session_id),
                               TakeDictionary(command.get(), kParamsParam));
  }
  return false;
}

}  // namespace content
