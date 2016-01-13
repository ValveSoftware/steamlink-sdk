// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_UI_RESOURCE_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_UI_RESOURCE_PROVIDER_H_

#include "cc/resources/ui_resource_client.h"
#include "content/common/content_export.h"

namespace content {

class UIResourceClientAndroid;

class CONTENT_EXPORT UIResourceProvider {
 public:
  virtual ~UIResourceProvider() {}

  virtual cc::UIResourceId CreateUIResource(
      UIResourceClientAndroid* client) = 0;

  virtual void DeleteUIResource(cc::UIResourceId resource_id) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_UI_RESOURCE_PROVIDER_H_
