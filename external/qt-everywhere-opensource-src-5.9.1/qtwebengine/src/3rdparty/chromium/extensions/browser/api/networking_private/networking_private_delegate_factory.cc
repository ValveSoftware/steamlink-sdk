// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"

#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extensions_browser_client.h"

#if defined(OS_CHROMEOS)
#include "extensions/browser/api/networking_private/networking_private_chromeos.h"
#elif defined(OS_LINUX)
#include "extensions/browser/api/networking_private/networking_private_linux.h"
#elif defined(OS_WIN) || defined(OS_MACOSX)
#include "components/wifi/wifi_service.h"
#include "extensions/browser/api/networking_private/networking_private_service_client.h"
#endif

namespace extensions {

using content::BrowserContext;

NetworkingPrivateDelegateFactory::VerifyDelegateFactory::
    VerifyDelegateFactory() {
}

NetworkingPrivateDelegateFactory::VerifyDelegateFactory::
    ~VerifyDelegateFactory() {
}

NetworkingPrivateDelegateFactory::UIDelegateFactory::UIDelegateFactory() {}

NetworkingPrivateDelegateFactory::UIDelegateFactory::~UIDelegateFactory() {}

// static
NetworkingPrivateDelegate*
NetworkingPrivateDelegateFactory::GetForBrowserContext(
    BrowserContext* browser_context) {
  return static_cast<NetworkingPrivateDelegate*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
NetworkingPrivateDelegateFactory*
NetworkingPrivateDelegateFactory::GetInstance() {
  return base::Singleton<NetworkingPrivateDelegateFactory>::get();
}

NetworkingPrivateDelegateFactory::NetworkingPrivateDelegateFactory()
    : BrowserContextKeyedServiceFactory(
          "NetworkingPrivateDelegate",
          BrowserContextDependencyManager::GetInstance()) {
}

NetworkingPrivateDelegateFactory::~NetworkingPrivateDelegateFactory() {
}

void NetworkingPrivateDelegateFactory::SetVerifyDelegateFactory(
    std::unique_ptr<VerifyDelegateFactory> factory) {
  verify_factory_.reset(factory.release());
}

void NetworkingPrivateDelegateFactory::SetUIDelegateFactory(
    std::unique_ptr<UIDelegateFactory> factory) {
  ui_factory_.reset(factory.release());
}

KeyedService* NetworkingPrivateDelegateFactory::BuildServiceInstanceFor(
    BrowserContext* browser_context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<NetworkingPrivateDelegate::VerifyDelegate> verify_delegate;
  if (verify_factory_)
    verify_delegate = verify_factory_->CreateDelegate();

  NetworkingPrivateDelegate* delegate;
#if defined(OS_CHROMEOS)
  delegate = new NetworkingPrivateChromeOS(browser_context,
                                           std::move(verify_delegate));
#elif defined(OS_LINUX)
  delegate = new NetworkingPrivateLinux(std::move(verify_delegate));
#elif defined(OS_WIN) || defined(OS_MACOSX)
  std::unique_ptr<wifi::WiFiService> wifi_service(wifi::WiFiService::Create());
  delegate = new NetworkingPrivateServiceClient(std::move(wifi_service),
                                                std::move(verify_delegate));
#else
  NOTREACHED();
  delegate = nullptr;
#endif

  if (ui_factory_)
    delegate->set_ui_delegate(ui_factory_->CreateDelegate());

  return delegate;
}

BrowserContext* NetworkingPrivateDelegateFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

bool NetworkingPrivateDelegateFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return false;
}

bool NetworkingPrivateDelegateFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

}  // namespace extensions
