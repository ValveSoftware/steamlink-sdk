// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_CONTROLLER_FACTORY_REGISTRY_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_CONTROLLER_FACTORY_REGISTRY_H_

#include "base/memory/singleton.h"
#include "content/public/browser/web_ui_controller_factory.h"

namespace content {

// A singleton which holds on to all the registered WebUIControllerFactory
// instances.
class CONTENT_EXPORT WebUIControllerFactoryRegistry
    : public WebUIControllerFactory {
 public:
  static WebUIControllerFactoryRegistry* GetInstance();

  // WebUIControllerFactory implementation. Each method loops through the same
  // method on all the factories.
  virtual WebUIController* CreateWebUIControllerForURL(
      WebUI* web_ui, const GURL& url) const OVERRIDE;
  virtual WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                                     const GURL& url) const OVERRIDE;
  virtual bool UseWebUIForURL(BrowserContext* browser_context,
                              const GURL& url) const OVERRIDE;
  virtual bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                                      const GURL& url) const OVERRIDE;

  // Returns true if the given URL can be loaded by Web UI system. This allows
  // URLs that UseWebUIForURL returns true for, and also URLs that can be loaded
  // by normal tabs such as javascript: URLs or about:hang.
  bool IsURLAcceptableForWebUI(BrowserContext* browser_context,
                               const GURL& url) const;

 private:
  friend struct DefaultSingletonTraits<WebUIControllerFactoryRegistry>;

  WebUIControllerFactoryRegistry();
  virtual ~WebUIControllerFactoryRegistry();

  DISALLOW_COPY_AND_ASSIGN(WebUIControllerFactoryRegistry);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_CONTROLLER_FACTORY_REGISTRY_H_
