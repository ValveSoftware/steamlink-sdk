// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extensions_browser_client.h"

#include "base/logging.h"
#include "components/update_client/update_client.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/updater/update_client_config.h"

namespace extensions {

namespace {

ExtensionsBrowserClient* g_client = NULL;

}  // namespace

scoped_refptr<update_client::UpdateClient>
ExtensionsBrowserClient::CreateUpdateClient(content::BrowserContext* context) {
  return scoped_refptr<update_client::UpdateClient>(nullptr);
}

std::unique_ptr<ExtensionApiFrameIdMapHelper>
ExtensionsBrowserClient::CreateExtensionApiFrameIdMapHelper(
    ExtensionApiFrameIdMap* map) {
  return nullptr;
}

std::unique_ptr<content::BluetoothChooser>
ExtensionsBrowserClient::CreateBluetoothChooser(
    content::RenderFrameHost* frame,
    const content::BluetoothChooser::EventHandler& event_handler) {
  return nullptr;
}

void ExtensionsBrowserClient::ReportError(
    content::BrowserContext* context,
    std::unique_ptr<ExtensionError> error) {
  LOG(ERROR) << error->GetDebugString();
}

bool ExtensionsBrowserClient::IsActivityLoggingEnabled(
    content::BrowserContext* context) {
  return false;
}

ExtensionNavigationUIData*
ExtensionsBrowserClient::GetExtensionNavigationUIData(
    net::URLRequest* request) {
  return nullptr;
}

ExtensionsBrowserClient* ExtensionsBrowserClient::Get() {
  return g_client;
}

void ExtensionsBrowserClient::Set(ExtensionsBrowserClient* client) {
  g_client = client;
}

}  // namespace extensions
