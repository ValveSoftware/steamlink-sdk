// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/test_extensions_browser_client.h"

#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_host_delegate.h"
#include "extensions/browser/test_runtime_api_delegate.h"
#include "extensions/browser/updater/null_extension_cache.h"

using content::BrowserContext;

namespace extensions {

TestExtensionsBrowserClient::TestExtensionsBrowserClient(
    BrowserContext* main_context)
    : main_context_(main_context),
      incognito_context_(NULL),
      process_manager_delegate_(NULL),
      extension_system_factory_(NULL),
      extension_cache_(new NullExtensionCache) {
  DCHECK(main_context_);
  DCHECK(!main_context_->IsOffTheRecord());
}

TestExtensionsBrowserClient::~TestExtensionsBrowserClient() {}

void TestExtensionsBrowserClient::SetUpdateClientFactory(
    const base::Callback<update_client::UpdateClient*(void)>& factory) {
  update_client_factory_ = factory;
}

void TestExtensionsBrowserClient::SetIncognitoContext(BrowserContext* context) {
  // If a context is provided it must be off-the-record.
  DCHECK(!context || context->IsOffTheRecord());
  incognito_context_ = context;
}

bool TestExtensionsBrowserClient::IsShuttingDown() { return false; }

bool TestExtensionsBrowserClient::AreExtensionsDisabled(
    const base::CommandLine& command_line,
    BrowserContext* context) {
  return false;
}

bool TestExtensionsBrowserClient::IsValidContext(BrowserContext* context) {
  return context == main_context_ ||
         (incognito_context_ && context == incognito_context_);
}

bool TestExtensionsBrowserClient::IsSameContext(BrowserContext* first,
                                                BrowserContext* second) {
  DCHECK(first);
  DCHECK(second);
  return first == second ||
         (first == main_context_ && second == incognito_context_) ||
         (first == incognito_context_ && second == main_context_);
}

bool TestExtensionsBrowserClient::HasOffTheRecordContext(
    BrowserContext* context) {
  return context == main_context_ && incognito_context_ != NULL;
}

BrowserContext* TestExtensionsBrowserClient::GetOffTheRecordContext(
    BrowserContext* context) {
  if (context == main_context_)
    return incognito_context_;
  return NULL;
}

BrowserContext* TestExtensionsBrowserClient::GetOriginalContext(
    BrowserContext* context) {
  return main_context_;
}

#if defined(OS_CHROMEOS)
std::string TestExtensionsBrowserClient::GetUserIdHashFromContext(
    content::BrowserContext* context) {
  return "";
}
#endif

bool TestExtensionsBrowserClient::IsGuestSession(
    BrowserContext* context) const {
  return false;
}

bool TestExtensionsBrowserClient::IsExtensionIncognitoEnabled(
    const std::string& extension_id,
    content::BrowserContext* context) const {
  return false;
}

bool TestExtensionsBrowserClient::CanExtensionCrossIncognito(
    const extensions::Extension* extension,
    content::BrowserContext* context) const {
  return false;
}

net::URLRequestJob*
TestExtensionsBrowserClient::MaybeCreateResourceBundleRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const base::FilePath& directory_path,
    const std::string& content_security_policy,
    bool send_cors_header) {
  return NULL;
}

bool TestExtensionsBrowserClient::AllowCrossRendererResourceLoad(
    net::URLRequest* request,
    bool is_incognito,
    const Extension* extension,
    InfoMap* extension_info_map) {
  return false;
}

PrefService* TestExtensionsBrowserClient::GetPrefServiceForContext(
    BrowserContext* context) {
  return nullptr;
}

void TestExtensionsBrowserClient::GetEarlyExtensionPrefsObservers(
    content::BrowserContext* context,
    std::vector<ExtensionPrefsObserver*>* observers) const {}

ProcessManagerDelegate* TestExtensionsBrowserClient::GetProcessManagerDelegate()
    const {
  return process_manager_delegate_;
}

std::unique_ptr<ExtensionHostDelegate>
TestExtensionsBrowserClient::CreateExtensionHostDelegate() {
  return std::unique_ptr<ExtensionHostDelegate>();
}

bool TestExtensionsBrowserClient::DidVersionUpdate(BrowserContext* context) {
  return false;
}

void TestExtensionsBrowserClient::PermitExternalProtocolHandler() {
}

bool TestExtensionsBrowserClient::IsRunningInForcedAppMode() { return false; }

bool TestExtensionsBrowserClient::IsLoggedInAsPublicAccount() {
  return false;
}

ExtensionSystemProvider*
TestExtensionsBrowserClient::GetExtensionSystemFactory() {
  DCHECK(extension_system_factory_);
  return extension_system_factory_;
}

void TestExtensionsBrowserClient::RegisterExtensionFunctions(
    ExtensionFunctionRegistry* registry) const {}

void TestExtensionsBrowserClient::RegisterMojoServices(
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) const {
}

std::unique_ptr<RuntimeAPIDelegate>
TestExtensionsBrowserClient::CreateRuntimeAPIDelegate(
    content::BrowserContext* context) const {
  return std::unique_ptr<RuntimeAPIDelegate>(new TestRuntimeAPIDelegate());
}

const ComponentExtensionResourceManager*
TestExtensionsBrowserClient::GetComponentExtensionResourceManager() {
  return NULL;
}

void TestExtensionsBrowserClient::BroadcastEventToRenderers(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> args) {}

net::NetLog* TestExtensionsBrowserClient::GetNetLog() {
  return NULL;
}

ExtensionCache* TestExtensionsBrowserClient::GetExtensionCache() {
  return extension_cache_.get();
}

bool TestExtensionsBrowserClient::IsBackgroundUpdateAllowed() {
  return true;
}

bool TestExtensionsBrowserClient::IsMinBrowserVersionSupported(
    const std::string& min_version) {
  return true;
}

ExtensionWebContentsObserver*
TestExtensionsBrowserClient::GetExtensionWebContentsObserver(
    content::WebContents* web_contents) {
  return nullptr;
}

scoped_refptr<update_client::UpdateClient>
TestExtensionsBrowserClient::CreateUpdateClient(
    content::BrowserContext* context) {
  return update_client_factory_.is_null()
             ? nullptr
             : make_scoped_refptr(update_client_factory_.Run());
}

}  // namespace extensions
