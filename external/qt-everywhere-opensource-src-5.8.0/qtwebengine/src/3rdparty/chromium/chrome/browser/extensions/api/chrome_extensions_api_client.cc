// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/chrome_device_permissions_prompt.h"
#include "chrome/browser/extensions/api/declarative_content/chrome_content_rules_registry.h"
#include "chrome/browser/extensions/api/declarative_content/default_content_predicate_evaluators.h"
#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"
#include "chrome/browser/extensions/api/storage/managed_value_store_cache.h"
#include "chrome/browser/extensions/api/storage/sync_value_store_cache.h"
#include "chrome/browser/extensions/api/web_request/chrome_extension_web_request_event_router_delegate.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/guest_view/app_view/chrome_app_view_guest_delegate.h"
#include "chrome/browser/guest_view/chrome_guest_view_manager_delegate.h"
#include "chrome/browser/guest_view/extension_options/chrome_extension_options_guest_delegate.h"
#include "chrome/browser/guest_view/mime_handler_view/chrome_mime_handler_view_guest_delegate.h"
#include "chrome/browser/guest_view/web_view/chrome_web_view_guest_delegate.h"
#include "chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.h"
#include "chrome/browser/ui/pdf/chrome_pdf_web_contents_helper_client.h"
#include "components/pdf/browser/pdf_web_contents_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/extensions/api/virtual_keyboard_private/chrome_virtual_keyboard_delegate.h"
#endif

#if defined(ENABLE_PRINTING)
#if defined(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // defined(ENABLE_PRINT_PREVIEW)
#endif  // defined(ENABLE_PRINTING)

namespace extensions {

ChromeExtensionsAPIClient::ChromeExtensionsAPIClient() {}

ChromeExtensionsAPIClient::~ChromeExtensionsAPIClient() {}

void ChromeExtensionsAPIClient::AddAdditionalValueStoreCaches(
    content::BrowserContext* context,
    const scoped_refptr<ValueStoreFactory>& factory,
    const scoped_refptr<base::ObserverListThreadSafe<SettingsObserver>>&
        observers,
    std::map<settings_namespace::Namespace, ValueStoreCache*>* caches) {
  // Add support for chrome.storage.sync.
  (*caches)[settings_namespace::SYNC] =
      new SyncValueStoreCache(factory, observers, context->GetPath());

  // Add support for chrome.storage.managed.
  (*caches)[settings_namespace::MANAGED] =
      new ManagedValueStoreCache(context, factory, observers);
}

void ChromeExtensionsAPIClient::AttachWebContentsHelpers(
    content::WebContents* web_contents) const {
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
#if defined(ENABLE_PRINTING)
#if defined(ENABLE_PRINT_PREVIEW)
  printing::PrintViewManager::CreateForWebContents(web_contents);
  printing::PrintPreviewMessageHandler::CreateForWebContents(web_contents);
#else
  printing::PrintViewManagerBasic::CreateForWebContents(web_contents);
#endif  // defined(ENABLE_PRINT_PREVIEW)
#endif  // defined(ENABLE_PRINTING)
  pdf::PDFWebContentsHelper::CreateForWebContentsWithClient(
      web_contents, std::unique_ptr<pdf::PDFWebContentsHelperClient>(
                        new ChromePDFWebContentsHelperClient()));

  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
}

AppViewGuestDelegate* ChromeExtensionsAPIClient::CreateAppViewGuestDelegate()
    const {
  return new ChromeAppViewGuestDelegate();
}

ExtensionOptionsGuestDelegate*
ChromeExtensionsAPIClient::CreateExtensionOptionsGuestDelegate(
    ExtensionOptionsGuest* guest) const {
  return new ChromeExtensionOptionsGuestDelegate(guest);
}

std::unique_ptr<guest_view::GuestViewManagerDelegate>
ChromeExtensionsAPIClient::CreateGuestViewManagerDelegate(
    content::BrowserContext* context) const {
  return base::WrapUnique(new ChromeGuestViewManagerDelegate(context));
}

std::unique_ptr<MimeHandlerViewGuestDelegate>
ChromeExtensionsAPIClient::CreateMimeHandlerViewGuestDelegate(
    MimeHandlerViewGuest* guest) const {
  return base::WrapUnique(new ChromeMimeHandlerViewGuestDelegate());
}

WebViewGuestDelegate* ChromeExtensionsAPIClient::CreateWebViewGuestDelegate(
    WebViewGuest* web_view_guest) const {
  return new ChromeWebViewGuestDelegate(web_view_guest);
}

WebViewPermissionHelperDelegate* ChromeExtensionsAPIClient::
    CreateWebViewPermissionHelperDelegate(
        WebViewPermissionHelper* web_view_permission_helper) const {
  return new ChromeWebViewPermissionHelperDelegate(web_view_permission_helper);
}

std::unique_ptr<WebRequestEventRouterDelegate>
ChromeExtensionsAPIClient::CreateWebRequestEventRouterDelegate() const {
  return base::WrapUnique(new ChromeExtensionWebRequestEventRouterDelegate());
}

scoped_refptr<ContentRulesRegistry>
ChromeExtensionsAPIClient::CreateContentRulesRegistry(
    content::BrowserContext* browser_context,
    RulesCacheDelegate* cache_delegate) const {
  return scoped_refptr<ContentRulesRegistry>(
      new ChromeContentRulesRegistry(
          browser_context,
          cache_delegate,
          base::Bind(&CreateDefaultContentPredicateEvaluators,
                     base::Unretained(browser_context))));
}

std::unique_ptr<DevicePermissionsPrompt>
ChromeExtensionsAPIClient::CreateDevicePermissionsPrompt(
    content::WebContents* web_contents) const {
  return base::WrapUnique(new ChromeDevicePermissionsPrompt(web_contents));
}

std::unique_ptr<VirtualKeyboardDelegate>
ChromeExtensionsAPIClient::CreateVirtualKeyboardDelegate() const {
#if defined(OS_CHROMEOS)
  return base::WrapUnique(new ChromeVirtualKeyboardDelegate());
#else
  return nullptr;
#endif
}

ManagementAPIDelegate* ChromeExtensionsAPIClient::CreateManagementAPIDelegate()
    const {
  return new ChromeManagementAPIDelegate;
}

}  // namespace extensions
