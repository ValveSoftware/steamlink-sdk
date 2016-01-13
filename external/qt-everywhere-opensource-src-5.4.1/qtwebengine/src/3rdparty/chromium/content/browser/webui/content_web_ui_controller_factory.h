// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_CONTENT_WEB_UI_CONTROLLER_FACTORY_H_
#define CONTENT_BROWSER_WEBUI_CONTENT_WEB_UI_CONTROLLER_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller_factory.h"

namespace content {

class ContentWebUIControllerFactory : public WebUIControllerFactory {
 public:
  virtual WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                                     const GURL& url) const OVERRIDE;
  virtual bool UseWebUIForURL(BrowserContext* browser_context,
                              const GURL& url) const OVERRIDE;
  virtual bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                                      const GURL& url) const OVERRIDE;
  virtual WebUIController* CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) const OVERRIDE;

  static ContentWebUIControllerFactory* GetInstance();

 protected:
  ContentWebUIControllerFactory();
  virtual ~ContentWebUIControllerFactory();

 private:
  friend struct DefaultSingletonTraits<ContentWebUIControllerFactory>;

  DISALLOW_COPY_AND_ASSIGN(ContentWebUIControllerFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_CONTENT_WEB_UI_CONTROLLER_FACTORY_H_
