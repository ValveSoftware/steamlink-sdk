// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_PORT_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_PORT_H_

#include "base/threading/thread_checker.h"
#include "chrome/browser/extensions/api/messaging/message_service.h"

namespace extensions {
class NativeMessageProcessHost;

// A port that manages communication with a native application.
// All methods must be called on the UI Thread of the browser process.
class NativeMessagePort : public MessageService::MessagePort {
 public:
  NativeMessagePort(base::WeakPtr<MessageService> message_service,
                    int port_id,
                    std::unique_ptr<NativeMessageHost> native_message_host);
  ~NativeMessagePort() override;

  // MessageService::MessagePort implementation.
  bool IsValidPort() override;
  void DispatchOnMessage(const Message& message) override;

 private:
  class Core;
  void PostMessageFromNativeHost(const std::string& message);
  void CloseChannel(const std::string& error_message);

  base::ThreadChecker thread_checker_;
  base::WeakPtr<MessageService> weak_message_service_;
  scoped_refptr<base::SingleThreadTaskRunner> host_task_runner_;
  int port_id_;
  std::unique_ptr<Core> core_;

  base::WeakPtrFactory<NativeMessagePort> weak_factory_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_PORT_H_
