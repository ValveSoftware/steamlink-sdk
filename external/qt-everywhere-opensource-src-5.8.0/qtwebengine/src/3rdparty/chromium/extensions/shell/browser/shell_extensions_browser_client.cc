// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_extensions_browser_client.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/generated_api_registration.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/mojo/service_registration.h"
#include "extensions/browser/null_app_sorting.h"
#include "extensions/browser/updater/null_extension_cache.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/shell/browser/api/generated_api_registration.h"
#include "extensions/shell/browser/shell_extension_host_delegate.h"
#include "extensions/shell/browser/shell_extension_system_factory.h"
#include "extensions/shell/browser/shell_extension_web_contents_observer.h"
#include "extensions/shell/browser/shell_extensions_api_client.h"
#include "extensions/shell/browser/shell_runtime_api_delegate.h"

#if defined(OS_CHROMEOS)
#include "chromeos/login/login_state.h"
#endif

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

ShellExtensionsBrowserClient::ShellExtensionsBrowserClient(
    BrowserContext* context,
    PrefService* pref_service)
    : browser_context_(context),
      pref_service_(pref_service),
      api_client_(new ShellExtensionsAPIClient),
      extension_cache_(new NullExtensionCache()) {
}

ShellExtensionsBrowserClient::~ShellExtensionsBrowserClient() {
}

bool ShellExtensionsBrowserClient::IsShuttingDown() {
  return false;
}

bool ShellExtensionsBrowserClient::AreExtensionsDisabled(
    const base::CommandLine& command_line,
    BrowserContext* context) {
  return false;
}

bool ShellExtensionsBrowserClient::IsValidContext(BrowserContext* context) {
  return context == browser_context_;
}

bool ShellExtensionsBrowserClient::IsSameContext(BrowserContext* first,
                                                 BrowserContext* second) {
  return first == second;
}

bool ShellExtensionsBrowserClient::HasOffTheRecordContext(
    BrowserContext* context) {
  return false;
}

BrowserContext* ShellExtensionsBrowserClient::GetOffTheRecordContext(
    BrowserContext* context) {
  // app_shell only supports a single context.
  return NULL;
}

BrowserContext* ShellExtensionsBrowserClient::GetOriginalContext(
    BrowserContext* context) {
  return context;
}

#if defined(OS_CHROMEOS)
std::string ShellExtensionsBrowserClient::GetUserIdHashFromContext(
    content::BrowserContext* context) {
  return chromeos::LoginState::Get()->primary_user_hash();
}
#endif

bool ShellExtensionsBrowserClient::IsGuestSession(
    BrowserContext* context) const {
  return false;
}

bool ShellExtensionsBrowserClient::IsExtensionIncognitoEnabled(
    const std::string& extension_id,
    content::BrowserContext* context) const {
  return false;
}

bool ShellExtensionsBrowserClient::CanExtensionCrossIncognito(
    const Extension* extension,
    content::BrowserContext* context) const {
  return false;
}

net::URLRequestJob*
ShellExtensionsBrowserClient::MaybeCreateResourceBundleRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const base::FilePath& directory_path,
    const std::string& content_security_policy,
    bool send_cors_header) {
  return NULL;
}

bool ShellExtensionsBrowserClient::AllowCrossRendererResourceLoad(
    net::URLRequest* request,
    bool is_incognito,
    const Extension* extension,
    InfoMap* extension_info_map) {
  bool allowed = false;
  if (url_request_util::AllowCrossRendererResourceLoad(
          request, is_incognito, extension, extension_info_map, &allowed)) {
    return allowed;
  }

  // Couldn't determine if resource is allowed. Block the load.
  return false;
}

PrefService* ShellExtensionsBrowserClient::GetPrefServiceForContext(
    BrowserContext* context) {
  return pref_service_;
}

void ShellExtensionsBrowserClient::GetEarlyExtensionPrefsObservers(
    content::BrowserContext* context,
    std::vector<ExtensionPrefsObserver*>* observers) const {
}

ProcessManagerDelegate*
ShellExtensionsBrowserClient::GetProcessManagerDelegate() const {
  return NULL;
}

std::unique_ptr<ExtensionHostDelegate>
ShellExtensionsBrowserClient::CreateExtensionHostDelegate() {
  return base::WrapUnique(new ShellExtensionHostDelegate);
}

bool ShellExtensionsBrowserClient::DidVersionUpdate(BrowserContext* context) {
  // TODO(jamescook): We might want to tell extensions when app_shell updates.
  return false;
}

void ShellExtensionsBrowserClient::PermitExternalProtocolHandler() {
}

bool ShellExtensionsBrowserClient::IsRunningInForcedAppMode() {
  return false;
}

bool ShellExtensionsBrowserClient::IsLoggedInAsPublicAccount() {
  return false;
}

ExtensionSystemProvider*
ShellExtensionsBrowserClient::GetExtensionSystemFactory() {
  return ShellExtensionSystemFactory::GetInstance();
}

void ShellExtensionsBrowserClient::RegisterExtensionFunctions(
    ExtensionFunctionRegistry* registry) const {
  // Register core extension-system APIs.
  api::GeneratedFunctionRegistry::RegisterAll(registry);

  // app_shell-only APIs.
  shell::api::ShellGeneratedFunctionRegistry::RegisterAll(registry);
}

void ShellExtensionsBrowserClient::RegisterMojoServices(
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) const {
  RegisterServicesForFrame(render_frame_host, extension);
}

std::unique_ptr<RuntimeAPIDelegate>
ShellExtensionsBrowserClient::CreateRuntimeAPIDelegate(
    content::BrowserContext* context) const {
  return base::WrapUnique(new ShellRuntimeAPIDelegate());
}

const ComponentExtensionResourceManager*
ShellExtensionsBrowserClient::GetComponentExtensionResourceManager() {
  return NULL;
}

void ShellExtensionsBrowserClient::BroadcastEventToRenderers(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> args) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ShellExtensionsBrowserClient::BroadcastEventToRenderers,
                   base::Unretained(this), histogram_value, event_name,
                   base::Passed(&args)));
    return;
  }

  std::unique_ptr<Event> event(
      new Event(histogram_value, event_name, std::move(args)));
  EventRouter::Get(browser_context_)->BroadcastEvent(std::move(event));
}

net::NetLog* ShellExtensionsBrowserClient::GetNetLog() {
  return NULL;
}

ExtensionCache* ShellExtensionsBrowserClient::GetExtensionCache() {
  return extension_cache_.get();
}

bool ShellExtensionsBrowserClient::IsBackgroundUpdateAllowed() {
  return true;
}

bool ShellExtensionsBrowserClient::IsMinBrowserVersionSupported(
    const std::string& min_version) {
  return true;
}

void ShellExtensionsBrowserClient::SetAPIClientForTest(
    ExtensionsAPIClient* api_client) {
  api_client_.reset(api_client);
}

ExtensionWebContentsObserver*
ShellExtensionsBrowserClient::GetExtensionWebContentsObserver(
    content::WebContents* web_contents) {
  return ShellExtensionWebContentsObserver::FromWebContents(web_contents);
}

}  // namespace extensions
