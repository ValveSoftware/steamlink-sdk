// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_network/system_network_api.h"

namespace {
const char kNetworkListError[] = "Network lookup failed or unsupported";
}  // namespace

namespace extensions {
namespace api {

SystemNetworkGetNetworkInterfacesFunction::
    SystemNetworkGetNetworkInterfacesFunction() {
}

SystemNetworkGetNetworkInterfacesFunction::
    ~SystemNetworkGetNetworkInterfacesFunction() {
}

bool SystemNetworkGetNetworkInterfacesFunction::RunAsync() {
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &SystemNetworkGetNetworkInterfacesFunction::GetListOnFileThread,
          this));
  return true;
}

void SystemNetworkGetNetworkInterfacesFunction::GetListOnFileThread() {
  net::NetworkInterfaceList interface_list;
  if (net::GetNetworkList(&interface_list,
                          net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(
            &SystemNetworkGetNetworkInterfacesFunction::SendResponseOnUIThread,
            this,
            interface_list));
    return;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&SystemNetworkGetNetworkInterfacesFunction::HandleGetListError,
                 this));
}

void SystemNetworkGetNetworkInterfacesFunction::HandleGetListError() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  error_ = kNetworkListError;
  SendResponse(false);
}

void SystemNetworkGetNetworkInterfacesFunction::SendResponseOnUIThread(
    const net::NetworkInterfaceList& interface_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<api::system_network::NetworkInterface> create_arg;
  create_arg.reserve(interface_list.size());
  for (const net::NetworkInterface interface : interface_list) {
    api::system_network::NetworkInterface info;
    info.name = interface.name;
    info.address = interface.address.ToString();
    info.prefix_length = interface.prefix_length;
    create_arg.push_back(std::move(info));
  }

  results_ =
      api::system_network::GetNetworkInterfaces::Results::Create(create_arg);
  SendResponse(true);
}

}  // namespace api
}  // namespace extensions
