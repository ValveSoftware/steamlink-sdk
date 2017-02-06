// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "extensions/browser/api/system_network/system_network_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/test_util.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"

using extensions::Extension;
using extensions::api_test_utils::RunFunctionAndReturnSingleResult;
using extensions::api::SystemNetworkGetNetworkInterfacesFunction;
using extensions::api::system_network::NetworkInterface;

namespace {

class SystemNetworkApiTest : public extensions::ShellApiTest {};

}  // namespace

IN_PROC_BROWSER_TEST_F(SystemNetworkApiTest, SystemNetworkExtension) {
  ASSERT_TRUE(RunAppTest("system/network")) << message_;
}

IN_PROC_BROWSER_TEST_F(SystemNetworkApiTest, GetNetworkInterfaces) {
  scoped_refptr<SystemNetworkGetNetworkInterfacesFunction> socket_function(
      new SystemNetworkGetNetworkInterfacesFunction());
  scoped_refptr<Extension> empty_extension(
      extensions::test_util::CreateEmptyExtension());

  socket_function->set_extension(empty_extension.get());
  socket_function->set_has_callback(true);

  std::unique_ptr<base::Value> result(RunFunctionAndReturnSingleResult(
      socket_function.get(), "[]", browser_context()));
  ASSERT_EQ(base::Value::TYPE_LIST, result->GetType());

  // All we can confirm is that we have at least one address, but not what it
  // is.
  base::ListValue* value = static_cast<base::ListValue*>(result.get());
  ASSERT_TRUE(value->GetSize() > 0);

  for (const auto& network_interface_value : *value) {
    NetworkInterface network_interface;
    ASSERT_TRUE(NetworkInterface::Populate(*network_interface_value,
                                           &network_interface));

    LOG(INFO) << "Network interface: address=" << network_interface.address
              << ", name=" << network_interface.name
              << ", prefix length=" << network_interface.prefix_length;
    ASSERT_NE(std::string(), network_interface.address);
    ASSERT_NE(std::string(), network_interface.name);
    ASSERT_LE(0, network_interface.prefix_length);
  }
}
