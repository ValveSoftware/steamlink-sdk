// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_CLIENT_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_CLIENT_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/values.h"

namespace content {

struct DevToolsCommandId {
  static const int kNoId;

  DevToolsCommandId(int call_id, int session_id)
      : call_id(call_id), session_id(session_id) {}

  int call_id;
  int session_id;
};

class DevToolsProtocolDelegate;
class DevToolsProtocolDispatcher;
class DevToolsProtocolHandler;

class DevToolsProtocolClient {
 public:
  struct Response {
   public:
    static Response FallThrough();
    static Response OK();
    static Response InvalidParams(const std::string& param);
    static Response InternalError(const std::string& message);
    static Response ServerError(const std::string& message);

    int status() const;
    const std::string& message() const;

    bool IsFallThrough() const;

   private:
    friend class DevToolsProtocolHandler;

    explicit Response(int status);
    Response(int status, const std::string& message);

    int status_;
    std::string message_;
  };

  bool SendError(DevToolsCommandId command_id,
                 const Response& response);

  // Sends notification to client, the caller is presumed to properly
  // format the message. Do not use unless you must.
  void SendRawNotification(const std::string& message);

  void SendMessage(int session_id, const base::DictionaryValue& message);

  explicit DevToolsProtocolClient(DevToolsProtocolDelegate* notifier);
  virtual ~DevToolsProtocolClient();

 protected:
  void SendSuccess(DevToolsCommandId command_id,
                   std::unique_ptr<base::DictionaryValue> params);
  void SendNotification(const std::string& method,
                        std::unique_ptr<base::DictionaryValue> params);

 private:
  friend class DevToolsProtocolDispatcher;

  DevToolsProtocolDelegate* notifier_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsProtocolClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_PROTOCOL_CLIENT_H_
