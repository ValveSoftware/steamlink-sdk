// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_IOS_BROWSER_STATE_CONTEXT_CONVERTER_H_
#define COMPONENTS_KEYED_SERVICE_IOS_BROWSER_STATE_CONTEXT_CONVERTER_H_

#include "base/macros.h"

namespace base {
class SupportsUserData;
}

// BrowserStateContextConverter does safe conversion of base::SupportsUserData*
// to web::BrowserState* or content::BrowserContext*.
//
// iOS code is still using BrowserContextKeyedServiceFactory and until the
// conversion is complete — http://crbug.com/478763 — there is need to have
// mixed dependency between BCKSF and BSKSF.
//
// The implementation has BrowserStateKeyedServiceFactory supporting a
// BrowserContextDependencyManager as DependencyManager. Thus the context
// parameter passed to the BrowserStateKeyedServiceFactory can either be
// content::BrowserContext if the method is invoked by DependencyManager
// or web::BrowserState if the method is invoked via the type-safe public
// API.
//
// The public API of BrowserStateKeyedServiceFactory is type-safe (all
// public method receive web::BrowserState for context object), so only
// methods that take a base::SupportsUserData need to discriminate
// between the two objects.
class BrowserStateContextConverter {
 public:
  // Sets/Gets the global BrowserStateContextConverter instance. May return null
  // when mixed dependencies are disabled.
  static void SetInstance(BrowserStateContextConverter* instance);
  static BrowserStateContextConverter* GetInstance();

  // Converts |context| to a web::BrowserState* and returns it casted as a
  // base::SupportsUserData*.
  virtual base::SupportsUserData* GetBrowserStateForContext(
      base::SupportsUserData* context) = 0;
  // Converts |context| to a content::BrowserContext* and returns it casted as a
  // base::SupportsUserData*.
  virtual base::SupportsUserData* GetBrowserContextForContext(
      base::SupportsUserData* context) = 0;

 protected:
  BrowserStateContextConverter();
  virtual ~BrowserStateContextConverter();

  DISALLOW_COPY_AND_ASSIGN(BrowserStateContextConverter);
};

#endif  // COMPONENTS_KEYED_SERVICE_IOS_BROWSER_STATE_CONTEXT_CONVERTER_H_
