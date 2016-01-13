// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_MOCK_OBJECT_PROXY_H_
#define DBUS_MOCK_OBJECT_PROXY_H_

#include <string>

#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace dbus {

// Mock for ObjectProxy.
class MockObjectProxy : public ObjectProxy {
 public:
  MockObjectProxy(Bus* bus,
                  const std::string& service_name,
                  const ObjectPath& object_path);

  // GMock doesn't support the return type of scoped_ptr<> because scoped_ptr is
  // uncopyable. This is a workaround which defines |MockCallMethodAndBlock| as
  // a mock method and makes |CallMethodAndBlock| call the mocked method.
  // Use |MockCallMethodAndBlock| for setting/testing expectations.
  MOCK_METHOD2(MockCallMethodAndBlock, Response*(MethodCall* method_call,
                                                 int timeout_ms));
  virtual scoped_ptr<Response> CallMethodAndBlock(MethodCall* method_call,
                                                  int timeout_ms) OVERRIDE {
    return scoped_ptr<Response>(MockCallMethodAndBlock(method_call,
                                                       timeout_ms));
  }
  MOCK_METHOD3(CallMethod, void(MethodCall* method_call,
                                int timeout_ms,
                                ResponseCallback callback));
  MOCK_METHOD4(CallMethodWithErrorCallback, void(MethodCall* method_call,
                                                 int timeout_ms,
                                                 ResponseCallback callback,
                                                 ErrorCallback error_callback));
  MOCK_METHOD4(ConnectToSignal,
               void(const std::string& interface_name,
                    const std::string& signal_name,
                    SignalCallback signal_callback,
                    OnConnectedCallback on_connected_callback));
  MOCK_METHOD0(Detach, void());

 protected:
  virtual ~MockObjectProxy();
};

}  // namespace dbus

#endif  // DBUS_MOCK_OBJECT_PROXY_H_
