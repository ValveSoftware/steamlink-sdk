// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_API_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_API_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"

namespace extensions {

class DialAPI;

class DialAPIFactory : public RefcountedBrowserContextKeyedServiceFactory {
 public:
  static scoped_refptr<DialAPI> GetForBrowserContext(
      content::BrowserContext* context);

  static DialAPIFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<DialAPIFactory>;

  DialAPIFactory();
  ~DialAPIFactory() override;

  // BrowserContextKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(DialAPIFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_API_FACTORY_H_
